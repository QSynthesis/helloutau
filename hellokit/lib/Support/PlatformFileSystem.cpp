#include "PlatformFileSystem.h"

#include <algorithm>
#include <atomic>

#include <QtCore/QMutex>
#include <QtCore/QTemporaryFile>

namespace fs = std::filesystem;

namespace hello::kit {

    namespace {

        std::atomic<FolderAccessRecord *> currentRecord = nullptr;

    }

    std::filesystem::path PlatformFileSystem::listedFileName(const std::filesystem::path &name) {
#ifdef Q_OS_MACOS
        // The same conversion as QFile::decodeName() on macOS
        return QString::fromStdU16String(name.u16string())
            .normalized(QString::NormalizationForm_C)
            .toStdU16String();
#else
        return name;
#endif
    }

    std::optional<QString>
        PlatformFileSystem::unreadableFolderReason(const std::filesystem::path &directory) {
        const auto entry = FolderAccessRecord::checked(directory);
        if (const auto record = FolderAccessRecord::instance()) {
            record->update(entry);
        }
        if (entry.state == FolderAccessRecord::Unreadable) {
            return entry.reason;
        }
        return std::nullopt;
    }

    QString PlatformFileSystem::accessErrorReason(const std::error_code &error) {
#ifdef Q_OS_MACOS
        if (error == std::errc::operation_not_permitted) {
            return tr("macOS denies %1 the access. Allow the access to the folder in System "
                      "Settings, Privacy & Security, Files and Folders.")
                .arg(QCoreApplication::applicationName());
        }
#endif
        // The message of the system, in the code page of the system on Windows
        return QString::fromLocal8Bit(error.message());
    }

    class FolderAccessRecord::Impl {
    public:
        mutable QMutex mutex;
        std::vector<Entry> entries;

        // Returns the entry of folder, compared by the lexically normal path.
        std::vector<Entry>::iterator find(const fs::path &folder) {
            const auto key = folder.lexically_normal();
            return std::find_if(entries.begin(), entries.end(), [&key](const Entry &entry) {
                return entry.folder.lexically_normal() == key;
            });
        }
    };

    FolderAccessRecord::FolderAccessRecord() : _impl(std::make_unique<Impl>()) {
        FolderAccessRecord *expected = nullptr;
        const bool first = currentRecord.compare_exchange_strong(expected, this);
        Q_ASSERT(first);
        Q_UNUSED(first);
    }

    FolderAccessRecord::~FolderAccessRecord() {
        FolderAccessRecord *expected = this;
        currentRecord.compare_exchange_strong(expected, nullptr);
    }

    FolderAccessRecord *FolderAccessRecord::instance() {
        return currentRecord.load();
    }

    FolderAccessRecord::Entry FolderAccessRecord::checked(const fs::path &folder, bool write) {
        Entry entry{folder, Missing, {}};
        std::error_code error;
        fs::directory_iterator listing(folder, error);
        if (error) {
            // A file in place of the folder is missing as well, as the callers treat it. Windows
            // reports the listing of a file as ERROR_DIRECTORY rather than as not_a_directory.
            std::error_code statusError;
            const auto status = fs::status(folder, statusError);
            if (error == std::errc::no_such_file_or_directory ||
                error == std::errc::not_a_directory ||
                (!statusError && status.type() != fs::file_type::directory)) {
                return entry;
            }
            entry.state = Unreadable;
            entry.reason = PlatformFileSystem::accessErrorReason(error);
            return entry;
        }
        entry.state = Readable;
        if (write) {
            // Removed when the probe is destroyed
            QTemporaryFile probe(
                QString::fromStdU16String((folder / u".hello-access-XXXXXX").u16string()));
            if (probe.open()) {
                entry.state = Writable;
            } else {
                entry.state = Unwritable;
                entry.reason = probe.errorString();
            }
        }
        return entry;
    }

    void FolderAccessRecord::check(const std::vector<fs::path> &folders, bool write) {
        for (const auto &folder : folders) {
            record(checked(folder, write));
        }
    }

    void FolderAccessRecord::record(const Entry &entry) {
        store(entry, true);
    }

    void FolderAccessRecord::update(const Entry &entry) {
        store(entry, false);
    }

    void FolderAccessRecord::store(const Entry &entry, bool add) {
        auto &impl = *_impl;
        QMutexLocker locker(&impl.mutex);
        const auto it = impl.find(entry.folder);
        if (it == impl.entries.end()) {
            if (add) {
                impl.entries.push_back(entry);
            }
            return;
        }
        if (entry.state == Readable && (it->state == Writable || it->state == Unwritable)) {
            return;
        }
        *it = entry;
    }

    std::optional<FolderAccessRecord::Entry>
        FolderAccessRecord::entryOf(const fs::path &folder) const {
        auto &impl = *_impl;
        QMutexLocker locker(&impl.mutex);
        const auto it = impl.find(folder);
        if (it == impl.entries.end()) {
            return std::nullopt;
        }
        return *it;
    }

    std::vector<FolderAccessRecord::Entry> FolderAccessRecord::entries() const {
        auto &impl = *_impl;
        QMutexLocker locker(&impl.mutex);
        return impl.entries;
    }

    std::vector<FolderAccessRecord::Entry> FolderAccessRecord::problems() const {
        std::vector<Entry> result;
        for (auto &entry : entries()) {
            if (entry.state == Unreadable || entry.state == Unwritable) {
                result.push_back(std::move(entry));
            }
        }
        return result;
    }

}
