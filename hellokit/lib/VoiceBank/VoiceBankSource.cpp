#include "VoiceBankSource.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <system_error>

#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>

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

        /// The lowercase form of a file name, for comparison with the known names, which are
        /// all ASCII.
        ///
        /// Converted through UTF-8 rather than path::string() , which on Windows uses the system
        /// code page and throws for a name that the code page cannot represent. Only ASCII
        /// letters are folded, so that no byte of a multibyte character is altered regardless
        /// of the locale.
        std::string folded(const fs::path &name) {
            const auto u8 = name.u8string();
            std::string s(u8.begin(), u8.end());
            for (auto &c : s) {
                if (c >= 'A' && c <= 'Z') {
                    c = char(c - 'A' + 'a');
                }
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

        /// The entire file, or \c std::nullopt if it cannot be read, which differs from an empty
        /// file.
        std::optional<QByteArray> readWhole(const fs::path &path) {
            std::ifstream in(path, std::ios::binary);
            if (!in) {
                return std::nullopt;
            }
            const std::string bytes((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
            if (in.bad()) {
                return std::nullopt;
            }
            return QByteArray(bytes.data(), qsizetype(bytes.size()));
        }

        std::string_view textOf(const QByteArray &bytes) {
            return std::string_view(bytes.constData(), size_t(bytes.size()));
        }

        /// Reads one directory. Subdirectories are collected into \a children rather than
        /// traversed here, so that the traversal remains iterative and the limits are checked
        /// in one place.
        VoiceBankDirectorySource readOne(const fs::path &root, const fs::path &relative,
                                         std::vector<fs::path> &children,
                                         DiagnosticList &diagnostics) {
            VoiceBankDirectorySource directory;
            directory.path = relative;

            const auto absolute = relative.empty() ? root : root / relative;
            directory.stamp =
                VoiceBankDirectoryStamp::take(absolute).value_or(VoiceBankDirectoryStamp());

            std::error_code error;
            for (const auto &entry : fs::directory_iterator(
                     absolute, fs::directory_options::skip_permission_denied, error)) {
                // Symbolic links are never followed. A voice bank is a folder selected by the
                // user, and a single link suffices to traverse the entire disk or to loop.
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
                const auto kind = voiceBankFileNamed(name);
                if (!kind) {
                    if (isAudioName(name)) {
                        directory.audioFiles.push_back(entry.path().filename());
                    }
                    continue;
                }

                // Read once and parsed from the same buffer, so that the digest covers exactly
                // the bytes from which the directory was built.
                const auto bytes = readWhole(entry.path());
                if (!bytes) {
                    complain(diagnostics, VoiceBankSource::tr("\"%1\" could not be read.")
                                              .arg(displayed(entry.path())));
                    continue;
                }
                directory.files[*kind] = VoiceBankFileRecord{
                    entry.path().filename(),
                    QCryptographicHash::hash(*bytes, QCryptographicHash::Sha1),
                };

                if (*kind == VoiceBankFile::Config) {
                    DiagnosticList ignored;
                    directory.config = VoiceBankConfig::fromJson(*bytes, ignored);
                    if (!directory.config) {
                        complain(
                            diagnostics,
                            VoiceBankSource::tr("The HelloUTAU configuration in \"%1\" could not "
                                                "be read, so its encoding must be selected again.")
                                .arg(displayed(absolute)));
                    }
                } else if (*kind == VoiceBankFile::Oto) {
                    utau::OtoIni oto;
                    oto.read(textOf(*bytes));
                    directory.oto = std::move(oto);
                } else if (*kind == VoiceBankFile::PrefixMap) {
                    utau::PrefixMap map;
                    map.read(textOf(*bytes));
                    directory.prefixMap = std::move(map);
                } else if (*kind == VoiceBankFile::Character) {
                    utau::CharacterTxt character;
                    character.read(textOf(*bytes));
                    directory.character = std::move(character);
                } else if (*kind == VoiceBankFile::Readme) {
                    directory.readme = *bytes;
                }
            }

            if (error) {
                complain(diagnostics, VoiceBankSource::tr("The contents of \"%1\" could not be "
                                                          "listed, so the directory was left out.")
                                          .arg(displayed(absolute)));
            }
            return directory;
        }

    }

    const char *voiceBankFileName(VoiceBankFile file) {
        switch (file) {
            case VoiceBankFile::Oto:
                return "oto.ini";
            case VoiceBankFile::PrefixMap:
                return "prefix.map";
            case VoiceBankFile::Character:
                return "character.txt";
            case VoiceBankFile::Readme:
                return "readme.txt";
            case VoiceBankFile::Config:
                return voiceBankConfigFileName;
        }
        return "";
    }

    std::optional<VoiceBankFile> voiceBankFileNamed(std::string_view foldedName) {
        for (const auto file :
             {VoiceBankFile::Oto, VoiceBankFile::PrefixMap, VoiceBankFile::Character,
              VoiceBankFile::Readme, VoiceBankFile::Config}) {
            if (foldedName == voiceBankFileName(file)) {
                return file;
            }
        }
        return std::nullopt;
    }

    bool VoiceBankDirectoryStamp::isRacy(const Entry &entry) const {
        // Two seconds is the timestamp granularity of FAT, the coarsest file system a voice
        // bank is likely to reside on.
        return entry.time + std::chrono::seconds(2) >= takenAt;
    }

    std::optional<VoiceBankDirectoryStamp>
        VoiceBankDirectoryStamp::take(const fs::path &directory) {
        std::error_code error;
        if (!fs::is_directory(directory, error)) {
            return std::nullopt;
        }

        VoiceBankDirectoryStamp stamp;
        // Taken before the listing, so that any write during the listing counts as racy.
        stamp.takenAt = fs::file_time_type::clock::now();

        for (const auto &item : fs::directory_iterator(
                 directory, fs::directory_options::skip_permission_denied, error)) {
            // As when reading, links are never followed and are not part of the voice bank.
            if (item.is_symlink(error)) {
                continue;
            }
            Entry entry;
            entry.name = item.path().filename();
            if (item.is_directory(error)) {
                entry.directory = true;
            } else if (item.is_regular_file(error)) {
                const auto name = folded(entry.name);
                if (voiceBankFileNamed(name)) {
                    entry.size = item.file_size(error);
                    entry.time = item.last_write_time(error);
                } else if (!isAudioName(name)) {
                    continue;
                }
            } else {
                continue;
            }
            stamp.entries.push_back(entry);
        }
        if (error) {
            return std::nullopt;
        }

        std::sort(stamp.entries.begin(), stamp.entries.end(),
                  [](const Entry &a, const Entry &b) { return a.name < b.name; });
        return stamp;
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
            fail(diagnostics, tr("\"%1\" is not a folder.").arg(displayed(root)));
            return std::nullopt;
        }

        VoiceBankSource source;
        source.m_root = fs::absolute(root, error);
        if (error) {
            source.m_root = root;
        }
        source.m_directories = readTree(source.m_root, fs::path(), limits, 0, diagnostics);
        return source;
    }

    QList<VoiceBankDirectorySource> VoiceBankSource::readTree(const fs::path &root,
                                                              const fs::path &relative,
                                                              const VoiceBankLimits &limits,
                                                              int alreadyRead,
                                                              DiagnosticList &diagnostics) {
        QList<VoiceBankDirectorySource> out;
        const int start = int(std::distance(relative.begin(), relative.end()));

        // Breadth-first, one level at a time, so that the depth limit equals the number of
        // rounds and the directory limit stops the scan at a level boundary rather than partway
        // down one branch.
        std::vector<fs::path> level{relative};
        bool stopped = false;
        for (int depth = start; depth <= limits.maxDepth && !level.empty(); ++depth) {
            std::vector<fs::path> next;
            for (const auto &at : level) {
                if (alreadyRead + out.size() >= limits.maxDirectories) {
                    stopped = true;
                    break;
                }
                out.push_back(readOne(root, at, next, diagnostics));
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
            complain(diagnostics, tr("This folder exceeds the size or depth limit of a voice bank, "
                                     "so only part of it was read."));
        }
        return out;
    }

    std::optional<VoiceBankDirectorySource>
        VoiceBankSource::readDirectory(const fs::path &root, const fs::path &relative,
                                       DiagnosticList &diagnostics) {
        const auto absolute = relative.empty() ? root : root / relative;
        std::error_code error;
        if (!fs::is_directory(absolute, error)) {
            fail(diagnostics, tr("\"%1\" is not a folder.").arg(displayed(absolute)));
            return std::nullopt;
        }
        std::vector<fs::path> children;
        return readOne(root, relative, children, diagnostics);
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
