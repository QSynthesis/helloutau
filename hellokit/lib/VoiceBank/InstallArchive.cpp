#include "InstallArchive.h"

#include <algorithm>
#include <functional>
#include <set>
#include <system_error>

#include <QtCore/QDir>
#include <QtCore/QTemporaryDir>

#include <stdcorelib/path.h>

#include <hellokit/Support/Diagnostic.h>
#include <hellokit/Support/TextCodec.h>

#include "VoiceBankConfiguration.h"

namespace fs = std::filesystem;

namespace hello::kit {

    namespace {

        using Decoder = std::function<std::optional<QString>(const ZipExtractor::RawEntry &)>;

        QString pathText(const fs::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

        fs::path pathOf(const QString &text) {
            return fs::path(text.toStdU16String());
        }

        bool isAscii(QByteArrayView bytes) {
            return std::all_of(bytes.begin(), bytes.end(),
                               [](char c) { return static_cast<unsigned char>(c) < 0x80; });
        }

        // Returns the decoder that reads the names of the entries as ZipExtractor does: a name
        // that is ASCII or marked as UTF-8 as UTF-8, and any other name in codec
        Decoder decoderOf(const TextCodec &codec) {
            return [codec](const ZipExtractor::RawEntry &entry) -> std::optional<QString> {
                if (entry.utf8) {
                    return QString::fromUtf8(entry.name);
                }
                return codec.decode(entry.name);
            };
        }

        QStringList splitName(QString name) {
            name.replace(u'\\', u'/');
            if (name.endsWith(u'/')) {
                name.chop(1);
            }
            return name.split(u'/');
        }

        bool isManifest(const QStringList &components) {
            return components.size() == 1 &&
                   components.front().compare(QLatin1String(InstallArchive::manifestName),
                                              Qt::CaseInsensitive) == 0;
        }

        // Returns the components of the path of a file relative to the folder of the content, or
        // std::nullopt if the file is outside the content. An empty folder denotes the root of
        // the archive without install.txt, as UTAU installs an archive without contentsdir.
        std::optional<QStringList> contentPathOf(const QStringList &components,
                                                 const QString &folder) {
            if (folder.isEmpty()) {
                return isManifest(components) ? std::nullopt : std::optional(components);
            }
            if (components.size() > 1 &&
                components.front().compare(folder, Qt::CaseInsensitive) == 0) {
                return components.mid(1);
            }
            return std::nullopt;
        }

    }

    InstallArchive::InstallArchive() = default;

    InstallArchive::~InstallArchive() = default;

    bool InstallArchive::open(const fs::path &archive) {
        m_archive = archive;
        m_entries.clear();
        m_manifest.clear();
        m_nameEncoding.clear();
        m_folder.clear();
        m_contentsDirectory.clear();
        m_description.clear();
        m_errorString.clear();

        ZipExtractor extractor;
        const auto entries = extractor.listEntries(archive);
        if (!entries) {
            return fail(extractor.errorString());
        }
        std::optional<qint64> manifest;
        for (const auto &raw : *entries) {
            if (!raw.directory && raw.name.compare(manifestName, Qt::CaseInsensitive) == 0) {
                manifest = raw.index;
            }
            m_entries.push_back({raw, {}});
        }
        // UTAU refuses an archive without install.txt as well.
        if (!manifest) {
            return fail(tr("The archive has no install.txt at its root, so it is not an "
                           "installation archive of UTAU."));
        }
        const auto content = extractor.readEntry(archive, *manifest, manifestMaximumSize);
        if (!content) {
            return fail(extractor.errorString());
        }
        m_manifest = *content;
        return true;
    }

    bool InstallArchive::needsNameEncoding() const {
        return !isAscii(m_manifest) ||
               std::any_of(m_entries.cbegin(), m_entries.cend(),
                           [](const Entry &entry) { return !entry.raw.utf8; });
    }

    bool InstallArchive::canDecode(const QString &encoding) const {
        const TextCodec codec(encoding);
        if (!codec.isValid() || !codec.decode(m_manifest)) {
            return false;
        }
        const auto decode = decoderOf(codec);
        return std::all_of(m_entries.cbegin(), m_entries.cend(),
                           [&](const Entry &entry) { return decode(entry.raw).has_value(); });
    }

    bool InstallArchive::setNameEncoding(const QString &encoding) {
        m_errorString.clear();
        m_nameEncoding = needsNameEncoding() ? encoding : QString();
        const TextCodec codec(m_nameEncoding.isEmpty() ? QStringLiteral("UTF-8") : m_nameEncoding);
        if (!codec.isValid()) {
            return fail(tr("The encoding %1 is not available.").arg(m_nameEncoding));
        }
        const auto decode = decoderOf(codec);
        for (auto &entry : m_entries) {
            const auto name = decode(entry.raw);
            if (!name) {
                return fail(tr("The name %1 of an entry is invalid in %2.")
                                .arg(QString::fromLatin1(entry.raw.name.toPercentEncoding()),
                                     codec.name()));
            }
            entry.name = *name;
        }
        const auto manifest = codec.decode(m_manifest);
        if (!manifest) {
            return fail(tr("install.txt is invalid in %1.").arg(codec.name()));
        }

        // Lines of key=value, in which the keys are ASCII. An unknown key is ignored.
        QString type;
        m_folder.clear();
        m_contentsDirectory.clear();
        m_description.clear();
        for (auto line : manifest->split(u'\n')) {
            if (line.endsWith(u'\r')) {
                line.chop(1);
            }
            const auto equal = line.indexOf(u'=');
            if (equal < 0) {
                continue;
            }
            const auto key = line.first(equal).trimmed().toLower();
            const auto value = line.mid(equal + 1);
            if (key == u"type") {
                type = value.trimmed();
            } else if (key == u"folder") {
                m_folder = value;
            } else if (key == u"contentsdir") {
                m_contentsDirectory = value;
            } else if (key == u"description") {
                m_description = value;
            }
        }
        if (type == u"voiceset") {
            m_kind = VoiceBank;
        } else if (type == u"editplugin") {
            m_kind = Plugin;
        } else {
            return fail(
                tr("The type %1 of install.txt is neither voiceset nor editplugin.").arg(type));
        }
        // The values are untrusted input as the names of the entries are.
        if (!ZipExtractor::isSafeComponent(m_folder)) {
            return fail(
                tr("The folder %1 of install.txt is not a safe folder name.").arg(m_folder));
        }
        if (!m_contentsDirectory.isEmpty() && !ZipExtractor::isSafeComponent(m_contentsDirectory)) {
            return fail(tr("The folder %1 of install.txt is not a safe folder name.")
                            .arg(m_contentsDirectory));
        }

        ZipExtractor extractor;
        extractor.setNameDecoder([codec](QByteArrayView name) { return codec.decode(name); });
        const auto checked = QDir(QDir::tempPath()).filePath(QStringLiteral("hello-install-check"));
        if (!extractor.check(m_archive, pathOf(checked))) {
            return fail(extractor.errorString());
        }
        // UTAU reads the name of the voice bank or of the plugin from this file, and reports an
        // error in the installation information without it.
        const auto required =
            m_kind == VoiceBank ? QStringLiteral("character.txt") : QStringLiteral("plugin.txt");
        const auto paths = contents();
        if (std::none_of(paths.cbegin(), paths.cend(), [&](const QString &path) {
                return path.compare(required, Qt::CaseInsensitive) == 0;
            })) {
            return fail(m_contentsDirectory.isEmpty()
                            ? tr("The archive holds no %1 at its root.").arg(required)
                            : tr("The archive holds no %1 in the folder %2 that install.txt "
                                 "names.")
                                  .arg(required, m_contentsDirectory));
        }
        return true;
    }

    QString InstallArchive::nameEncoding() const {
        return m_nameEncoding;
    }

    InstallArchive::Kind InstallArchive::kind() const {
        return m_kind;
    }

    QString InstallArchive::folder() const {
        return m_folder;
    }

    QString InstallArchive::contentsDirectory() const {
        return m_contentsDirectory;
    }

    QString InstallArchive::description() const {
        return m_description;
    }

    QStringList InstallArchive::contents() const {
        QStringList paths;
        for (const auto &entry : m_entries) {
            if (entry.raw.directory) {
                continue;
            }
            if (const auto path = contentPathOf(splitName(entry.name), m_contentsDirectory)) {
                paths.push_back(path->join(u'/'));
            }
        }
        return paths;
    }

    std::optional<QByteArray> InstallArchive::readContent(const QString &path, qint64 maximumSize) {
        for (const auto &entry : m_entries) {
            const auto content = entry.raw.directory
                                     ? std::nullopt
                                     : contentPathOf(splitName(entry.name), m_contentsDirectory);
            if (content && content->join(u'/') == path) {
                ZipExtractor extractor;
                return extractor.readEntry(m_archive, entry.raw.index, maximumSize);
            }
        }
        return std::nullopt;
    }

    bool InstallArchive::canWriteMisdecoded() const {
        const auto system = TextCodec::systemName();
        return needsNameEncoding() && system.compare(m_nameEncoding, Qt::CaseInsensitive) != 0 &&
               canDecode(system);
    }

    QStringList InstallArchive::unrepresentableNames() const {
        const TextCodec system(TextCodec::systemName());
        QStringList names;
        for (const auto &path : contents()) {
            if (!system.canEncode(path)) {
                names.push_back(path);
            }
        }
        return names;
    }

    QString InstallArchive::freeFolderName(const fs::path &root, const QString &folder) {
        std::error_code error;
        if (!fs::exists(root / pathOf(folder), error)) {
            return folder;
        }
        for (int number = 0;; ++number) {
            const auto name = folder + QString::number(number);
            if (!fs::exists(root / pathOf(name), error)) {
                return name;
            }
        }
    }

    bool InstallArchive::install(const fs::path &root, const QString &folderName, NameMode mode,
                                 const QString &charset,
                                 const ZipExtractor::ProgressHandler &progress) {
        m_errorString.clear();
        if (!ZipExtractor::isSafeComponent(folderName)) {
            return fail(tr("%1 is not a safe folder name.").arg(folderName));
        }
        const auto system = TextCodec::systemName();
        const bool misdecoded = mode == Misdecoded && canWriteMisdecoded();
        const TextCodec codec(
            misdecoded ? system
                       : (m_nameEncoding.isEmpty() ? QStringLiteral("UTF-8") : m_nameEncoding));
        const auto decode = decoderOf(codec);

        // The folder of contentsdir under the names that are written, which differ from the
        // names of setNameEncoding() if the names are written as misdecoded, and the
        // install.txt of a content at the root of the archive, which is not installed
        QString subdirectory;
        std::optional<QString> manifest;
        std::vector<QStringList> written;
        for (const auto &entry : m_entries) {
            if (entry.raw.directory) {
                continue;
            }
            const auto components = splitName(entry.name);
            const bool content = contentPathOf(components, m_contentsDirectory).has_value();
            if (!content && !(m_contentsDirectory.isEmpty() && isManifest(components))) {
                continue;
            }
            const auto name = decode(entry.raw);
            if (!name) {
                return fail(
                    tr("The name %1 of an entry is invalid in %2.").arg(entry.name, codec.name()));
            }
            if (!content) {
                manifest = *name;
                continue;
            }
            auto path = splitName(*name);
            if (!m_contentsDirectory.isEmpty()) {
                subdirectory = path.front();
                path.removeFirst();
            }
            written.push_back(path);
        }
        if (written.empty()) {
            return fail(tr("The archive holds no files to install."));
        }

        std::error_code error;
        fs::create_directories(root, error);
        QTemporaryDir temporary(
            QDir(pathText(root))
                .filePath(QStringLiteral(".") + folderName + QStringLiteral(".installing-XXXXXX")));
        if (!temporary.isValid()) {
            return fail(tr("The folder %1 cannot be written.").arg(pathText(root)));
        }
        const auto staging = pathOf(temporary.path());

        ZipExtractor extractor;
        extractor.setNameDecoder([codec](QByteArrayView name) { return codec.decode(name); });
        extractor.setSubdirectory(subdirectory);
        extractor.setProgressHandler(progress);
        if (!extractor.extract(m_archive, staging)) {
            return fail(extractor.errorString());
        }
        if (manifest) {
            fs::remove(staging / pathOf(*manifest), error);
        }

        const auto target = root / pathOf(folderName);
        if (!fs::exists(target, error)) {
            // The renaming is immediate, because the temporary folder is in root.
            fs::rename(staging, target, error);
            if (error) {
                return fail(tr("The folder %1 cannot be created.").arg(pathText(target)));
            }
            temporary.setAutoRemove(false);
        } else {
            // Merged as UTAU merges: a file of the same name is replaced, the others are kept.
            for (auto it = fs::recursive_directory_iterator(staging, error);
                 !error && it != fs::recursive_directory_iterator(); it.increment(error)) {
                const auto destination =
                    target / stdc::path::lexically_relative(it->path(), staging);
                if (it->is_directory(error)) {
                    fs::create_directories(destination, error);
                    continue;
                }
                fs::remove(destination, error);
                fs::rename(it->path(), destination, error);
                if (error) {
                    return fail(tr("The file %1 cannot be written.").arg(pathText(destination)));
                }
            }
            if (error) {
                return fail(tr("The folder %1 cannot be read.").arg(pathText(staging)));
            }
        }

        if (m_kind != VoiceBank) {
            return true;
        }
        // The configuration of each folder that holds an oto.ini of the archive, in which the
        // fields of a configuration already there are kept
        std::set<QString> folders;
        for (const auto &components : written) {
            if (components.back().compare(QStringLiteral("oto.ini"), Qt::CaseInsensitive) == 0) {
                folders.insert(components.mid(0, components.size() - 1).join(u'/'));
            }
        }
        for (const auto &folder : folders) {
            const auto path = (folder.isEmpty() ? target : target / pathOf(folder)) /
                              VoiceBankConfiguration::fileName;
            DiagnosticList diagnostics;
            auto configuration = fs::exists(path, error)
                                     ? VoiceBankConfiguration::open(path, diagnostics)
                                     : std::optional(VoiceBankConfiguration());
            if (!configuration) {
                configuration = VoiceBankConfiguration();
            }
            if (!charset.isEmpty()) {
                configuration->charset = charset;
            }
            VoiceBankConfiguration::FileNameEncoding names;
            if (misdecoded) {
                names.original = m_nameEncoding;
                names.misdecodedAs = system;
            }
            configuration->fileNameEncoding = names;
            if (!configuration->save(path, diagnostics)) {
                return fail(tr("The file %1 cannot be written.").arg(pathText(path)));
            }
        }
        return true;
    }

    QString InstallArchive::errorString() const {
        return m_errorString;
    }

    bool InstallArchive::fail(const QString &message) {
        m_errorString = message;
        return false;
    }

}
