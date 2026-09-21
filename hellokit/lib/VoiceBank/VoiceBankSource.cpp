#include "VoiceBankSource.h"

#include <fstream>
#include <system_error>

#include <QtCore/QCoreApplication>

namespace hello::kit {

    namespace fs = std::filesystem;

    namespace {

        void fail(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Error, message});
        }

        void complain(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Warning, message});
        }

        QByteArrayView viewOf(const std::string &s) {
            return QByteArrayView(s.data(), qsizetype(s.size()));
        }

        QString displayed(const fs::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

        /// Lower case, for comparing a name against one this program knows.
        std::string folded(const fs::path &name) {
            auto s = name.string();
            for (auto &c : s) {
                c = char(std::tolower(static_cast<unsigned char>(c)));
            }
            return s;
        }

        bool isAudioName(const std::string &name) {
            const auto dot = name.rfind('.');
            if (dot == std::string::npos) {
                return false;
            }
            const auto suffix = name.substr(dot);
            return suffix == ".wav" || suffix == ".flac" || suffix == ".ogg";
        }

        QByteArray readWhole(const fs::path &path) {
            std::ifstream in(path, std::ios::binary);
            if (!in) {
                return {};
            }
            const std::string bytes((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
            return QByteArray(bytes.data(), qsizetype(bytes.size()));
        }

        /// Reads one directory. Subdirectories are collected into \a children rather than
        /// followed here, so that the walk stays iterative and the limits are checked in one
        /// place.
        VoiceBankDirectorySource readDirectory(const fs::path &root, const fs::path &relative,
                                               std::vector<fs::path> &children,
                                               DiagnosticList &diagnostics) {
            VoiceBankDirectorySource directory;
            directory.path = relative;

            const auto absolute = relative.empty() ? root : root / relative;

            std::error_code error;
            for (const auto &entry : fs::directory_iterator(
                     absolute, fs::directory_options::skip_permission_denied, error)) {
                // A symbolic link is followed nowhere. A voice bank is a folder a user picked,
                // and one link is enough to walk the whole disk or to loop.
                if (entry.is_symlink()) {
                    continue;
                }
                if (entry.is_directory()) {
                    children.push_back(relative / entry.path().filename());
                    continue;
                }
                if (!entry.is_regular_file()) {
                    continue;
                }

                const auto name = folded(entry.path().filename());
                if (name == voiceBankConfigFileName) {
                    DiagnosticList ignored;
                    directory.config = VoiceBankConfig::open(entry.path(), ignored);
                    if (!directory.config) {
                        complain(diagnostics,
                                 VoiceBankSource::tr("The HelloUTAU record in \"%1\" could not be read, so its "
                                    "encoding has to be chosen again.")
                                     .arg(displayed(absolute)));
                    }
                } else if (name == "oto.ini") {
                    utau::OtoIni oto;
                    if (oto.load(entry.path())) {
                        directory.oto = std::move(oto);
                    }
                } else if (name == "prefix.map") {
                    utau::PrefixMap map;
                    if (map.load(entry.path())) {
                        directory.prefixMap = std::move(map);
                    }
                } else if (name == "character.txt") {
                    utau::CharacterTxt character;
                    if (character.load(entry.path())) {
                        directory.character = std::move(character);
                    }
                } else if (name == "readme.txt") {
                    directory.readme = readWhole(entry.path());
                } else if (isAudioName(name)) {
                    directory.audioFiles.push_back(entry.path().filename());
                }
            }

            if (error) {
                complain(diagnostics,
                         VoiceBankSource::tr("\"%1\" could not be listed and was left out.")
                             .arg(displayed(absolute)));
            }
            return directory;
        }

    }

    bool VoiceBankDirectorySource::needsCharset() const {
        return oto || prefixMap || character || !readme.isEmpty();
    }

    QList<QByteArrayView> VoiceBankDirectorySource::rawAliases() const {
        QList<QByteArrayView> aliases;
        if (!oto) {
            return aliases;
        }
        for (const auto &[file, entries] : oto->contents) {
            for (const auto &entry : entries) {
                if (!entry.alias.empty()) {
                    aliases.push_back(viewOf(entry.alias));
                }
            }
        }
        return aliases;
    }

    VoiceBankCharsetSelector::~VoiceBankCharsetSelector() = default;

    FixedCharsetSelector::FixedCharsetSelector(QString charset) : m_charset(std::move(charset)) {
    }

    FixedCharsetSelector::~FixedCharsetSelector() = default;

    std::optional<QString> FixedCharsetSelector::selectCharset(const VoiceBankDirectorySource &,
                                                               DiagnosticList &) {
        return m_charset;
    }

    std::optional<VoiceBankSource> VoiceBankSource::open(const fs::path &root,
                                                         DiagnosticList &diagnostics,
                                                         const VoiceBankLimits &limits) {
        std::error_code error;
        if (!fs::is_directory(root, error)) {
            fail(diagnostics, VoiceBankSource::tr("\"%1\" is not a folder.").arg(displayed(root)));
            return std::nullopt;
        }

        VoiceBankSource source;
        source.m_root = fs::absolute(root, error);
        if (error) {
            source.m_root = root;
        }

        // Breadth first, one level at a time, so that the depth limit is a count of rounds and
        // the directory limit stops the scan where it is rather than part way down one branch.
        std::vector<fs::path> level{fs::path()};
        bool stopped = false;
        for (int depth = 0; depth <= limits.maxDepth && !level.empty(); ++depth) {
            std::vector<fs::path> next;
            for (const auto &relative : level) {
                if (source.m_directories.size() >= limits.maxDirectories) {
                    stopped = true;
                    break;
                }
                source.m_directories.push_back(
                    readDirectory(source.m_root, relative, next, diagnostics));
            }
            if (stopped) {
                break;
            }
            if (depth == limits.maxDepth && !next.empty()) {
                stopped = true;
                break;
            }
            level = std::move(next);
        }

        if (stopped) {
            complain(diagnostics,
                     VoiceBankSource::tr("This folder is larger or deeper than a voice bank is expected to be, so "
                        "only part of it was read."));
        }
        return source;
    }

    QList<const VoiceBankDirectorySource *> VoiceBankSource::unsettled() const {
        QList<const VoiceBankDirectorySource *> directories;
        for (const auto &directory : m_directories) {
            if (directory.needsCharset() &&
                (!directory.config || directory.config->charset.isEmpty())) {
                directories.push_back(&directory);
            }
        }
        return directories;
    }

}
