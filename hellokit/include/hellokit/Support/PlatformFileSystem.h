#ifndef HELLOKIT_SUPPORT_PLATFORMFILESYSTEM_H
#define HELLOKIT_SUPPORT_PLATFORMFILESYSTEM_H

#include <filesystem>
#include <memory>
#include <optional>
#include <system_error>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>

#include <hellokit/Support/HelloKitSupportGlobal.h>

namespace hello::kit {

    /// The differences between the file systems of the platforms that the model and the messages
    /// to the user depend on: the form of listed file names, and the reasons for which a folder
    /// cannot be read.
    class HELLOKIT_SUPPORT_EXPORT PlatformFileSystem {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::PlatformFileSystem)
    public:
        /// Returns \a name, a file name of a directory listing, in the form that the model holds.
        /// On macOS the name is in Normalization Form C, as Qt reads it, because Qt writes names
        /// in Normalization Form D and the file systems of macOS find a file by either form. On
        /// other systems \a name is returned unchanged, because their file systems distinguish
        /// the two forms. See the section on the normalization of file names in docs/note.md.
        static std::filesystem::path listedFileName(const std::filesystem::path &name);

        /// Returns the reason for which \a directory cannot be listed, or \c std::nullopt if it
        /// can be listed or does not exist. A directory that cannot be reached at all, as in the
        /// Documents folder that macOS denies, may exist, and is reported. The result updates the
        /// entry of \a directory in FolderAccessRecord, see FolderAccessRecord::update().
        static std::optional<QString>
            unreadableFolderReason(const std::filesystem::path &directory);

        /// Returns the reason that \a error of an access to a file or a folder states, in the
        /// words that a message to the user requires. macOS denies an application the access to
        /// the Documents folder until the user allows it, and the access then fails with EPERM,
        /// for which the reason names the setting that allows the access.
        static QString accessErrorReason(const std::error_code &error);

    private:
        PlatformFileSystem() = delete;
    };

    /// The recorded access of the folders that the application reads, checked at startup and on
    /// request, so that the prompt of macOS for a protected folder appears at startup and the
    /// folders that cannot be read are reported in one place. The record guides the reports only:
    /// every access still handles its own errors, and PlatformFileSystem::unreadableFolderReason()
    /// updates the record with its result. At most one instance exists at a time, which the
    /// application owns. See docs/FolderAccess.md.
    class HELLOKIT_SUPPORT_EXPORT FolderAccessRecord {
    public:
        enum State {
            Missing,    ///< The folder does not exist
            Readable,   ///< The folder can be listed, and its writing was not checked
            Writable,   ///< The folder can be listed, and a file can be created in it
            Unreadable, ///< The folder cannot be listed
            Unwritable, ///< The folder can be listed, and no file can be created in it
        };

        struct Entry {
            std::filesystem::path folder;
            State state = Missing;

            /// The reason of Unreadable and Unwritable, or an empty string
            QString reason;
        };

        /// Creates the record. At most one instance exists at a time.
        FolderAccessRecord();
        ~FolderAccessRecord();

        /// Returns the instance, or \c nullptr if none exists.
        static FolderAccessRecord *instance();

        /// Returns the access of \a folder, checked now. If \a write is true and the folder can
        /// be listed, a temporary file is created in the folder and removed.
        static Entry checked(const std::filesystem::path &folder, bool write = false);

        /// Checks each of \a folders as checked() does, and records the results.
        void check(const std::vector<std::filesystem::path> &folders, bool write = false);

        /// Records \a entry in place of the entry of its folder. A Readable entry keeps the result
        /// of an earlier check of the writing.
        void record(const Entry &entry);

        /// Records \a entry as record() does if its folder is recorded already, so that the
        /// accesses to other folders, such as to each voice bank, do not grow the record.
        void update(const Entry &entry);

        /// Returns the entry of \a folder, or \c std::nullopt if the folder was not checked.
        std::optional<Entry> entryOf(const std::filesystem::path &folder) const;

        /// Returns the entries in the order in which their folders were first recorded.
        std::vector<Entry> entries() const;

        /// Returns the entries that are Unreadable or Unwritable, in the order of entries().
        std::vector<Entry> problems() const;

    private:
        // Replaces the entry of the folder of entry, and adds one if add is true
        void store(const Entry &entry, bool add);

        class Impl;
        std::unique_ptr<Impl> _impl;

        Q_DISABLE_COPY(FolderAccessRecord)
    };

}

#endif // HELLOKIT_SUPPORT_PLATFORMFILESYSTEM_H
