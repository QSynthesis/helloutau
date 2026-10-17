#include "ZipExtractor.h"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <future>
#include <memory>
#include <mutex>
#include <system_error>
#include <thread>
#include <vector>

#include <QtCore/QHash>
#include <QtCore/QStringDecoder>
#include <QtCore/QStringList>

#include <zip.h>

namespace fs = std::filesystem;

namespace hello::kit {

    namespace {

        constexpr qint64 blockSize = 1 << 16;

        // The file type bits of a Unix mode, which the high 16 bits of the external attributes
        // hold if the entry was written on Unix
        constexpr zip_uint32_t unixTypeMask = 0170000;
        constexpr zip_uint32_t unixSymbolicLink = 0120000;
        constexpr zip_uint32_t unixPermissionMask = 0777;

        struct ArchiveDeleter {
            void operator()(zip_t *archive) const {
                zip_discard(archive);
            }
        };

        struct FileDeleter {
            void operator()(zip_file_t *file) const {
                zip_fclose(file);
            }
        };

        using ArchivePointer = std::unique_ptr<zip_t, ArchiveDeleter>;
        using FilePointer = std::unique_ptr<zip_file_t, FileDeleter>;

        struct Entry {
            zip_uint64_t index = 0;
            QString name;
            QStringList components;
            bool directory = false;
            std::optional<fs::perms> permissions;
        };

        QString pathText(const fs::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

        bool isAscii(QByteArrayView bytes) {
            return std::all_of(bytes.begin(), bytes.end(),
                               [](char c) { return static_cast<unsigned char>(c) < 0x80; });
        }

        std::optional<QString> decodeUtf8(QByteArrayView bytes) {
            QStringDecoder decoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
            QString text = decoder.decode(bytes);
            if (decoder.hasError()) {
                return std::nullopt;
            }
            return text;
        }

#ifdef Q_OS_WIN
        // CON, PRN, AUX, NUL, COM1 to COM9 and LPT1 to LPT9, also with the superscript digits, in
        // any case and before any extension, open a device instead of a file.
        bool isReservedDeviceName(const QString &component) {
            QStringView stem = component;
            if (const auto dot = stem.indexOf(u'.'); dot >= 0) {
                stem = stem.first(dot);
            }
            while (stem.endsWith(u' ')) {
                stem.chop(1);
            }
            static const QStringList names = {
                QStringLiteral("CON"), QStringLiteral("PRN"),    QStringLiteral("AUX"),
                QStringLiteral("NUL"), QStringLiteral("CONIN$"), QStringLiteral("CONOUT$"),
            };
            for (const auto &name : names) {
                if (stem.compare(name, Qt::CaseInsensitive) == 0) {
                    return true;
                }
            }
            if (stem.size() != 4) {
                return false;
            }
            const auto prefix = stem.first(3);
            if (prefix.compare(u"COM", Qt::CaseInsensitive) != 0 &&
                prefix.compare(u"LPT", Qt::CaseInsensitive) != 0) {
                return false;
            }
            const auto digit = stem[3];
            return (digit >= u'1' && digit <= u'9') || digit == QChar(0x00B9) ||
                   digit == QChar(0x00B2) || digit == QChar(0x00B3);
        }
#endif

        bool isSafeComponent(const QString &component) {
            if (component.isEmpty() || component == u"." || component == u"..") {
                return false;
            }
            for (const auto c : component) {
                if (c.unicode() < 0x20 || c.unicode() == 0x7F || c == u':') {
                    return false;
                }
            }
#ifdef Q_OS_WIN
            // Windows removes a trailing dot or space from a component, so that the component
            // names another file.
            if (component.endsWith(u'.') || component.endsWith(u' ') ||
                isReservedDeviceName(component)) {
                return false;
            }
#endif
            return true;
        }

        // Splits the decoded name of an entry into its components, and returns std::nullopt if
        // the name fails a check. A trailing separator marks a directory. A leading separator
        // yields an empty first component, which is rejected.
        std::optional<QStringList> componentsOf(QString name, bool *directory) {
            name.replace(u'\\', u'/');
            *directory = name.endsWith(u'/');
            if (*directory) {
                name.chop(1);
            }
            auto components = name.split(u'/');
            if (!std::all_of(components.begin(), components.end(), isSafeComponent)) {
                return std::nullopt;
            }
            return components;
        }

        ArchivePointer openArchive(const fs::path &path, int flags, QString *reason) {
            zip_error_t error;
            zip_error_init(&error);
#ifdef Q_OS_WIN
            // zip_open() accepts only a narrow path, which cannot represent every path on Windows.
            const auto source = zip_source_win32w_create(path.c_str(), 0, -1, &error);
#else
            const auto source = zip_source_file_create(path.c_str(), 0, -1, &error);
#endif
            ArchivePointer zip;
            if (source) {
                zip.reset(zip_open_from_source(source, ZIP_RDONLY | flags, &error));
                if (!zip) {
                    zip_source_free(source);
                }
            }
            if (!zip) {
                *reason = QString::fromUtf8(zip_error_strerror(&error));
            }
            zip_error_fini(&error);
            return zip;
        }

        // Returns whether target lies strictly within root. Both are canonical.
        bool isWithin(const fs::path &root, const fs::path &target) {
            const auto [r, t] =
                std::mismatch(root.begin(), root.end(), target.begin(), target.end());
            return r == root.end() && t != target.end();
        }

        // Returns whether the name of the entry at index, whose raw bytes are raw, is ASCII or
        // marked as UTF-8. With ZIP_FL_ENC_STRICT, libzip converts a name that is not marked as
        // UTF-8 from code page 437 to UTF-8, and returns a name marked as UTF-8 unchanged. The
        // conversion lengthens every byte above 0x7F, so that a non-ASCII name equals its
        // strict form only if it is marked as UTF-8.
        bool isUtf8Name(zip_t *zip, zip_uint64_t index, const QByteArray &raw) {
            if (isAscii(raw)) {
                return true;
            }
            const char *strict = zip_get_name(zip, index, ZIP_FL_ENC_STRICT);
            return strict && raw == strict;
        }

        bool isSymbolicLink(zip_t *zip, zip_uint64_t index) {
            zip_uint8_t system = 0;
            zip_uint32_t attributes = 0;
            return zip_file_get_external_attributes(zip, index, 0, &system, &attributes) == 0 &&
                   system == ZIP_OPSYS_UNIX &&
                   ((attributes >> 16) & unixTypeMask) == unixSymbolicLink;
        }

        // Returns the names of two entries whose paths are equal apart from case, or of a file
        // and an entry whose folder has the path of the file, or std::nullopt if there are none
        std::optional<std::pair<QString, QString>> collisionOf(const std::vector<Entry> &entries) {
            struct Seen {
                bool file = false;
                QString name;
            };
            QHash<QString, Seen> seen;
            for (const auto &entry : entries) {
                QString path;
                for (qsizetype k = 0; k < entry.components.size(); ++k) {
                    if (k > 0) {
                        path += u'/';
                    }
                    path += entry.components[k].toCaseFolded();
                    const bool file = k == entry.components.size() - 1 && !entry.directory;
                    const auto it = seen.constFind(path);
                    if (it == seen.cend()) {
                        seen.insert(path, {file, entry.name});
                    } else if (file || it->file) {
                        return std::pair(it->name, entry.name);
                    }
                }
            }
            return std::nullopt;
        }

        // Returns whether components begin with prefix, compared apart from case as Windows
        // compares the names of files
        bool beginsWith(const QStringList &components, const QStringList &prefix) {
            if (components.size() < prefix.size()) {
                return false;
            }
            for (qsizetype k = 0; k < prefix.size(); ++k) {
                if (components[k].compare(prefix[k], Qt::CaseInsensitive) != 0) {
                    return false;
                }
            }
            return true;
        }

    }

    ZipExtractor::ZipExtractor() = default;

    ZipExtractor::~ZipExtractor() = default;

    void ZipExtractor::setNameDecoder(NameDecoder decoder) {
        m_nameDecoder = std::move(decoder);
    }

    qint64 ZipExtractor::maximumEntryCount() const {
        return m_maximumEntryCount;
    }

    void ZipExtractor::setMaximumEntryCount(qint64 count) {
        m_maximumEntryCount = count;
    }

    qint64 ZipExtractor::maximumSize() const {
        return m_maximumSize;
    }

    void ZipExtractor::setMaximumSize(qint64 size) {
        m_maximumSize = size;
    }

    bool ZipExtractor::isTopDirectoryStripped() const {
        return m_topDirectoryStripped;
    }

    void ZipExtractor::setTopDirectoryStripped(bool stripped) {
        m_topDirectoryStripped = stripped;
    }

    QString ZipExtractor::subdirectory() const {
        return m_subdirectory;
    }

    void ZipExtractor::setSubdirectory(const QString &path) {
        m_subdirectory = path;
    }

    int ZipExtractor::threadCount() const {
        return m_threadCount;
    }

    void ZipExtractor::setThreadCount(int count) {
        m_threadCount = count;
    }

    void ZipExtractor::setProgressHandler(ProgressHandler handler) {
        m_progressHandler = std::move(handler);
    }

    std::optional<QList<ZipExtractor::RawEntry>>
        ZipExtractor::listEntries(const fs::path &archive) {
        m_error = NoError;
        m_errorString.clear();
        QString reason;
        const auto zip = openArchive(archive, ZIP_CHECKCONS, &reason);
        if (!zip) {
            fail(OpenError,
                 tr("The archive %1 cannot be opened: %2").arg(pathText(archive), reason));
            return std::nullopt;
        }
        const auto count = zip_get_num_entries(zip.get(), 0);
        if (count < 0) {
            fail(ReadError,
                 tr("The archive %1 cannot be read: %2")
                     .arg(pathText(archive), QString::fromUtf8(zip_strerror(zip.get()))));
            return std::nullopt;
        }
        if (count > m_maximumEntryCount) {
            fail(TooManyEntries,
                 tr("The archive holds more than %1 entries.").arg(m_maximumEntryCount));
            return std::nullopt;
        }
        QList<RawEntry> entries;
        entries.reserve(count);
        for (zip_uint64_t i = 0; i < zip_uint64_t(count); ++i) {
            const char *rawName = zip_get_name(zip.get(), i, ZIP_FL_ENC_RAW);
            zip_stat_t stat;
            if (!rawName || zip_stat_index(zip.get(), i, 0, &stat) != 0) {
                fail(ReadError, tr("The entry %1 cannot be read: %2")
                                    .arg(i + 1)
                                    .arg(QString::fromUtf8(zip_strerror(zip.get()))));
                return std::nullopt;
            }
            RawEntry entry;
            entry.index = qint64(i);
            entry.name = QByteArray(rawName);
            entry.utf8 = isUtf8Name(zip.get(), i, entry.name);
            entry.directory = entry.name.endsWith('/') || entry.name.endsWith('\\');
            entry.size = (stat.valid & ZIP_STAT_SIZE) ? qint64(stat.size) : 0;
            entries.push_back(std::move(entry));
        }
        return entries;
    }

    std::optional<QByteArray> ZipExtractor::readEntry(const fs::path &archive, qint64 index,
                                                      qint64 maximumSize) {
        m_error = NoError;
        m_errorString.clear();
        QString reason;
        const auto zip = openArchive(archive, ZIP_CHECKCONS, &reason);
        if (!zip) {
            fail(OpenError,
                 tr("The archive %1 cannot be opened: %2").arg(pathText(archive), reason));
            return std::nullopt;
        }
        const auto count = zip_get_num_entries(zip.get(), 0);
        zip_stat_t stat;
        if (index < 0 || index >= count || zip_stat_index(zip.get(), index, 0, &stat) != 0) {
            fail(ReadError, tr("The entry %1 cannot be read.").arg(index + 1));
            return std::nullopt;
        }
        if (isSymbolicLink(zip.get(), index)) {
            fail(SymbolicLink, tr("The entry %1 is a symbolic link.").arg(index + 1));
            return std::nullopt;
        }
        if ((stat.valid & ZIP_STAT_ENCRYPTION_METHOD) && stat.encryption_method != ZIP_EM_NONE) {
            fail(EncryptedEntry, tr("The entry %1 is encrypted.").arg(index + 1));
            return std::nullopt;
        }
        const auto tooLarge = [&] {
            fail(TooLarge, tr("The entry %1 exceeds %2 bytes.").arg(index + 1).arg(maximumSize));
            return std::nullopt;
        };
        if ((stat.valid & ZIP_STAT_SIZE) && stat.size > zip_uint64_t(maximumSize)) {
            return tooLarge();
        }
        FilePointer file(zip_fopen_index(zip.get(), index, 0));
        if (!file) {
            fail(ReadError, tr("The entry %1 cannot be read: %2")
                                .arg(index + 1)
                                .arg(QString::fromUtf8(zip_strerror(zip.get()))));
            return std::nullopt;
        }
        QByteArray content;
        std::vector<char> buffer(blockSize);
        for (;;) {
            const auto n = zip_fread(file.get(), buffer.data(), buffer.size());
            if (n < 0) {
                fail(ReadError, tr("The entry %1 cannot be read: %2")
                                    .arg(index + 1)
                                    .arg(QString::fromUtf8(zip_file_strerror(file.get()))));
                return std::nullopt;
            }
            if (n == 0) {
                break;
            }
            // The declared size can be forged, so the bytes actually read are counted as well.
            if (content.size() + n > maximumSize) {
                return tooLarge();
            }
            content.append(buffer.data(), qsizetype(n));
        }
        return content;
    }

    bool ZipExtractor::check(const fs::path &archive, const fs::path &directory) {
        m_canceled = false;
        return run(archive, directory, false);
    }

    bool ZipExtractor::extract(const fs::path &archive, const fs::path &directory) {
        m_canceled = false;
        return run(archive, directory, true);
    }

    std::future<bool> ZipExtractor::extractAsync(const fs::path &archive,
                                                 const fs::path &directory) {
        // Cleared here rather than on the new thread, so that cancel() immediately after this
        // call is not discarded.
        m_canceled = false;
        return std::async(std::launch::async,
                          [this, archive, directory] { return run(archive, directory, true); });
    }

    void ZipExtractor::cancel() {
        m_canceled = true;
    }

    bool ZipExtractor::run(const fs::path &archive, const fs::path &directory, bool write) {
        m_error = NoError;
        m_errorString.clear();

        // ZIP_CHECKCONS compares each local header with the central directory, which detects an
        // archive whose two copies of a name or a size differ.
        QString reason;
        auto zip = openArchive(archive, ZIP_CHECKCONS, &reason);
        if (!zip) {
            return fail(OpenError,
                        tr("The archive %1 cannot be opened: %2").arg(pathText(archive), reason));
        }

        const auto count = zip_get_num_entries(zip.get(), 0);
        if (count < 0) {
            return fail(ReadError,
                        tr("The archive %1 cannot be read: %2")
                            .arg(pathText(archive), QString::fromUtf8(zip_strerror(zip.get()))));
        }
        if (count > m_maximumEntryCount) {
            return fail(TooManyEntries,
                        tr("The archive holds more than %1 entries.").arg(m_maximumEntryCount));
        }

        std::vector<Entry> entries;
        entries.reserve(count);
        qint64 total = 0;
        for (zip_uint64_t i = 0; i < zip_uint64_t(count); ++i) {
            if (m_canceled) {
                return fail(Canceled, tr("The extraction was canceled."));
            }
            const char *rawName = zip_get_name(zip.get(), i, ZIP_FL_ENC_RAW);
            if (!rawName) {
                return fail(ReadError, tr("The entry %1 cannot be read: %2")
                                           .arg(i + 1)
                                           .arg(QString::fromUtf8(zip_strerror(zip.get()))));
            }
            const QByteArray raw(rawName);

            std::optional<QString> name;
            if (isAscii(raw)) {
                name = QString::fromLatin1(raw);
            } else if (isUtf8Name(zip.get(), i, raw)) {
                name = decodeUtf8(raw);
            } else if (m_nameDecoder) {
                name = m_nameDecoder(raw);
            }
            if (!name) {
                return fail(
                    UndecodableName,
                    tr("The name %1 of an entry cannot be decoded.").arg(QString::fromUtf8(raw)));
            }

            Entry entry;
            entry.index = i;
            entry.name = *name;
            const auto components = componentsOf(*name, &entry.directory);
            if (!components) {
                return fail(UnsafePath, tr("The entry %1 has an unsafe path.").arg(*name));
            }
            entry.components = *components;

            zip_uint8_t system = 0;
            zip_uint32_t attributes = 0;
            if (zip_file_get_external_attributes(zip.get(), i, 0, &system, &attributes) == 0 &&
                system == ZIP_OPSYS_UNIX) {
                const auto mode = attributes >> 16;
                if ((mode & unixTypeMask) == unixSymbolicLink) {
                    return fail(SymbolicLink, tr("The entry %1 is a symbolic link.").arg(*name));
                }
                if (mode & unixPermissionMask) {
                    entry.permissions = fs::perms(mode & unixPermissionMask);
                }
            }

            zip_stat_t stat;
            if (zip_stat_index(zip.get(), i, 0, &stat) != 0) {
                return fail(ReadError, tr("The entry %1 cannot be read: %2")
                                           .arg(*name, QString::fromUtf8(zip_strerror(zip.get()))));
            }
            if ((stat.valid & ZIP_STAT_ENCRYPTION_METHOD) &&
                stat.encryption_method != ZIP_EM_NONE) {
                return fail(EncryptedEntry, tr("The entry %1 is encrypted.").arg(*name));
            }
            if (stat.valid & ZIP_STAT_SIZE) {
                if (stat.size > zip_uint64_t(m_maximumSize - total)) {
                    return fail(TooLarge,
                                tr("The extracted files exceed %1 bytes.").arg(m_maximumSize));
                }
                total += qint64(stat.size);
            }
            entries.push_back(std::move(entry));
        }

        if (const auto collision = collisionOf(entries)) {
            return fail(DuplicateName,
                        tr("The entries %1 and %2 have the same name apart from case.")
                            .arg(collision->first, collision->second));
        }

        if (!m_subdirectory.isEmpty()) {
            bool directory = false;
            const auto prefix = componentsOf(m_subdirectory, &directory);
            if (!prefix) {
                return fail(UnsafePath, tr("The folder %1 is an unsafe path.").arg(m_subdirectory));
            }
            entries.erase(std::remove_if(entries.begin(), entries.end(),
                                         [&](const Entry &e) {
                                             return !beginsWith(e.components, *prefix) ||
                                                    e.components.size() == prefix->size();
                                         }),
                          entries.end());
            for (auto &entry : entries) {
                entry.components.erase(entry.components.begin(),
                                       entry.components.begin() + prefix->size());
            }
        }

        if (m_topDirectoryStripped && !entries.empty()) {
            const auto top = entries.front().components.front();
            const bool single = std::all_of(entries.begin(), entries.end(), [&](const Entry &e) {
                return e.components.front() == top && (e.components.size() > 1 || e.directory);
            });
            if (single) {
                for (auto &entry : entries) {
                    entry.components.removeFirst();
                }
                entries.erase(std::remove_if(entries.begin(), entries.end(),
                                             [](const Entry &e) { return e.components.isEmpty(); }),
                              entries.end());
            }
        }

        std::error_code ec;
        auto root = fs::absolute(directory, ec);
        if (!ec) {
            root = fs::weakly_canonical(root, ec);
        }
        if (ec) {
            return fail(WriteError,
                        tr("The folder %1 cannot be accessed.").arg(pathText(directory)));
        }
        if (!root.has_filename()) {
            root = root.parent_path();
        }
        std::vector<fs::path> targets;
        targets.reserve(entries.size());
        for (const auto &entry : entries) {
            fs::path target = root;
            for (const auto &component : entry.components) {
                target /= fs::path(component.toStdU16String());
            }
            target = fs::weakly_canonical(target, ec);
            if (ec || !isWithin(root, target)) {
                return fail(UnsafePath, tr("The entry %1 has an unsafe path.").arg(entry.name));
            }
            targets.push_back(std::move(target));
        }

        if (m_canceled) {
            return fail(Canceled, tr("The extraction was canceled."));
        }
        if (!write) {
            return true;
        }

        // Every entry has passed the checks. Nothing has been written before this point. The
        // directories are created first, so that the threads only create files.
        fs::create_directories(root, ec);
        if (ec) {
            return fail(WriteError, tr("The folder %1 cannot be created.").arg(pathText(root)));
        }
        std::vector<size_t> files;
        for (size_t i = 0; i < entries.size(); ++i) {
            const auto &folder = entries[i].directory ? targets[i] : targets[i].parent_path();
            fs::create_directories(folder, ec);
            if (ec) {
                return fail(WriteError,
                            tr("The folder %1 cannot be created.").arg(pathText(folder)));
            }
            if (!entries[i].directory) {
                files.push_back(i);
            }
        }

        // libzip requires that a zip_t is used by one thread at a time, and permits separate
        // zip_t objects on separate threads. Each thread therefore opens the archive once.
        const auto threadCount = std::clamp<size_t>(size_t(std::max(m_threadCount, 1)), 1,
                                                    std::max<size_t>(files.size(), 1));
        std::vector<ArchivePointer> archives;
        archives.push_back(std::move(zip));
        while (archives.size() < threadCount) {
            auto other = openArchive(archive, 0, &reason);
            if (!other) {
                return fail(
                    OpenError,
                    tr("The archive %1 cannot be opened: %2").arg(pathText(archive), reason));
            }
            archives.push_back(std::move(other));
        }

        std::atomic<size_t> next = 0;
        std::atomic<qint64> written = 0;
        std::atomic<bool> stopped = false;
        std::mutex failureMutex;
        std::mutex progressMutex;
        Error failure = NoError;
        QString failureString;
        const auto stop = [&](Error error, const QString &message) {
            std::lock_guard lock(failureMutex);
            if (failure == NoError) {
                failure = error;
                failureString = message;
            }
            stopped = true;
        };
        const auto writeFile = [&](zip_t *zip, std::vector<char> &buffer, size_t i) {
            const auto &entry = entries[i];
            const auto &target = targets[i];
            FilePointer file(zip_fopen_index(zip, entry.index, 0));
            if (!file) {
                return stop(ReadError, tr("The entry %1 cannot be read: %2")
                                           .arg(entry.name, QString::fromUtf8(zip_strerror(zip))));
            }
            std::ofstream out(target, std::ios::binary | std::ios::trunc);
            if (!out) {
                return stop(WriteError, tr("The file %1 cannot be written.").arg(pathText(target)));
            }
            while (!stopped) {
                if (m_canceled) {
                    return stop(Canceled, tr("The extraction was canceled."));
                }
                const auto n = zip_fread(file.get(), buffer.data(), buffer.size());
                if (n < 0) {
                    return stop(
                        ReadError,
                        tr("The entry %1 cannot be read: %2")
                            .arg(entry.name, QString::fromUtf8(zip_file_strerror(file.get()))));
                }
                if (n == 0) {
                    break;
                }
                // The bytes are counted before they are written, so that the threads together
                // never write more than the limit.
                if (written.fetch_add(n) + n > m_maximumSize) {
                    return stop(TooLarge,
                                tr("The extracted files exceed %1 bytes.").arg(m_maximumSize));
                }
                out.write(buffer.data(), std::streamsize(n));
                if (!out) {
                    return stop(WriteError,
                                tr("The file %1 cannot be written.").arg(pathText(target)));
                }
                if (m_progressHandler) {
                    std::lock_guard lock(progressMutex);
                    if (!stopped && !m_progressHandler(written, total)) {
                        return stop(Canceled, tr("The extraction was canceled."));
                    }
                }
            }
            out.close();
            if (!out) {
                return stop(WriteError, tr("The file %1 cannot be written.").arg(pathText(target)));
            }
#ifndef Q_OS_WIN
            if (entry.permissions) {
                std::error_code permissionsError;
                fs::permissions(target, *entry.permissions, permissionsError);
            }
#endif
        };
        const auto work = [&](zip_t *zip) {
            std::vector<char> buffer(blockSize);
            while (!stopped) {
                const auto k = next++;
                if (k >= files.size()) {
                    break;
                }
                writeFile(zip, buffer, files[k]);
            }
        };

        std::vector<std::thread> threads;
        for (size_t i = 1; i < archives.size(); ++i) {
            threads.emplace_back(work, archives[i].get());
        }
        work(archives.front().get());
        for (auto &thread : threads) {
            thread.join();
        }
        if (failure != NoError) {
            return fail(failure, failureString);
        }
        return true;
    }

    ZipExtractor::Error ZipExtractor::error() const {
        return m_error;
    }

    QString ZipExtractor::errorString() const {
        return m_errorString;
    }

    bool ZipExtractor::fail(Error error, const QString &message) {
        m_error = error;
        m_errorString = message;
        return false;
    }

}
