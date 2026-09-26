#ifndef HELLOKIT_EDIT_VOICEBANKSESSION_H
#define HELLOKIT_EDIT_VOICEBANKSESSION_H

#include <filesystem>
#include <memory>
#include <optional>

#include <QtCore/QJsonObject>
#include <QtCore/QList>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/VoiceBank/VoiceBank.h>
#include <hellokit/VoiceBank/VoiceBankFileSystemState.h>

#include <hellokit/EditBase/EditSession.h>

#include <hellokit/Edit/HelloKitEditGlobal.h>

namespace hello::kit {

    /// The editing of one voice bank, the second document beside a project. See the section on
    /// the voice bank in docs/Editing.md.
    ///
    /// The session owns the contents of the voice bank as a tree of nodes with the slots of
    /// VoiceBankSchema.h, and the file system state from which the voice bank was read. The tree is
    /// the document while the session exists. A \c VoiceBank is a snapshot of the tree together
    /// with the audio files that the file system state lists, for saving and synthesis. The handles
    /// in VoiceBankRefs.h, obtained from a VoiceBankRef of the session, read and modify the tree.
    class HELLOKIT_EDIT_EXPORT VoiceBankSession : public edit::EditSession {
        Q_OBJECT
    public:
        /// Creates a session that edits the voice bank of \a opened and keeps its file system
        /// state.
        ///
        /// A subdirectory that was not read, because the user selected no encoding for it,
        /// cannot be saved and is left out of the tree. The root directory cannot be left out, as
        /// a file that was not read cannot be edited either.
        ///
        /// \return the session, or null if the root directory was not read, with the reason in
        ///         \a diagnostics
        /// \sa excludedDirectories()
        static std::unique_ptr<VoiceBankSession> create(VoiceBankFileSystemState::Opened opened,
                                                        DiagnosticList &diagnostics,
                                                        QObject *parent = nullptr);

        ~VoiceBankSession();

        /// Returns the root directory of the voice bank.
        const std::filesystem::path &rootPath() const;

        /// Returns the subdirectories that were not read, as last read. They are not in the tree
        /// and not in snapshot(), as if they did not exist, and are read again in an encoding to
        /// be edited.
        ///
        /// \sa reread(), VoiceBankDirectory::leftOut
        QList<VoiceBankDirectory> excludedDirectories() const;

        /// Returns the names of the audio files of the directory at \a directory, with and without
        /// an entry.
        ///
        /// \sa VoiceBankFileSystemState::audioFiles()
        inline QStringList audioFiles(const std::filesystem::path &directory) const {
            return m_files.audioFiles(directory);
        }

        /// Returns the voice bank in its current state.
        VoiceBank snapshot() const;

        /// Returns the entry of the change log for \a change, or \c std::nullopt for a change
        /// that the log omits. The entry names the slot of \a change by its field name in
        /// VoiceBankSchema.h.
        ///
        /// \sa ProjectSession::logEntry()
        std::optional<QJsonObject> logEntry(const edit::Change &change) const;

        /// \name The files on disk
        ///
        /// The session keeps the file system state of the voice bank, and forwards to it with the
        /// snapshot of the tree. Reading from disk replaces the directories read again, as one
        /// undo step. See the section on changes on disk in docs/Editing.md.
        ///
        /// \sa VoiceBankFileSystemState
        /// @{

        /// Returns whether the voice bank on disk is incomplete: its root was found missing by
        /// checkDisk(), or no longer read when read again by reloadFromDisk(). The tree is then
        /// the only intact copy, and an editor tells the user prominently. Saving writes it back,
        /// and saveAs() writes it elsewhere, either of which makes the voice bank complete again.
        /// See the section on an incomplete voice bank in docs/Editing.md.
        bool isIncomplete() const;

        /// Saves the voice bank. The directories that are not in the tree are neither written nor
        /// removed. Without the root, every text file of the tree is written again.
        ///
        /// \sa VoiceBankFileSystemState::save()
        bool save(DiagnosticList &diagnostics);

        /// The files that saveAs() writes.
        enum SaveAsFiles {
            /// The text files of the tree, and a copy of every other file of the voice bank.
            AllFiles,
            /// The text files of the tree only.
            TextFiles,
        };

        /// Saves the voice bank into \a folder , which must not exist or be empty, and edits it
        /// there from now on.
        ///
        /// The tree and the undo history are kept, the file system state becomes that of \a folder
        /// , in which the voice bank is unmodified, and the original folder is no longer written.
        ///
        /// \sa VoiceBankFileSystemState::saveAs()
        bool saveAs(const std::filesystem::path &folder, SaveAsFiles files,
                    DiagnosticList &diagnostics);

        /// \sa VoiceBankFileSystemState::rememberCharset()
        void rememberCharset(const std::filesystem::path &directory);

        /// \sa VoiceBankFileSystemState::hasUnrecordedCharsets()
        bool hasUnrecordedCharsets() const;

        /// Checks the disk for changes. A directory on disk that the tree does not hold, because
        /// an undo took out a directory that a reload had added, is reported as added by every
        /// check, so that it can be taken in again.
        ///
        /// \sa VoiceBankFileSystemState::checkDisk()
        VoiceBankChanges checkDisk(const QList<std::filesystem::path> &places);

        /// \overload for the entire voice bank.
        VoiceBankChanges checkDisk();

        /// Applies \a changes .
        ///
        /// The directories read again, added or removed are replaced in the tree in one undo
        /// step, which takes the files as read without checking the constraints. Undoing it
        /// restores the tree and not the file system state, so the restored contents are saved over
        /// the files, and a restored directory is created again. A directory that now does not read
        /// or decode leaves the tree for excludedDirectories(), and one that now does enters it.
        /// A change of the audio files alone creates no undo step.
        ///
        /// \return the changes applied
        /// \sa VoiceBankFileSystemState::reloadFromDisk()
        VoiceBankChanges reloadFromDisk(const VoiceBankChanges &changes,
                                        VoiceBankCharsetSelector *selector,
                                        DiagnosticList &diagnostics);

        /// Reads every directory again.
        ///
        /// \sa VoiceBankFileSystemState::reloadAllFromDisk(), reloadFromDisk()
        VoiceBankChanges reloadAllFromDisk(VoiceBankCharsetSelector *selector,
                                           DiagnosticList &diagnostics);

        /// Reads the directory at \a directory again in \a charset , one in the tree or one of
        /// excludedDirectories(), as one undo step.
        ///
        /// \sa VoiceBankFileSystemState::reread(), reloadFromDisk()
        bool reread(const std::filesystem::path &directory, const QString &charset,
                    DiagnosticList &diagnostics);

        /// @}

    private:
        VoiceBankSession(VoiceBankFileSystemState::Opened opened, QObject *parent);

        /// Returns the snapshot together with the excluded directories, which the file system state
        /// works on.
        VoiceBank fullBank() const;

        /// Returns \a changes with each directory of the file system state that the session holds
        /// neither in the tree nor as excluded added to the new directories, and records whether
        /// the root was found.
        ///
        /// \sa isIncomplete()
        VoiceBankChanges withUntaken(VoiceBankChanges changes);

        /// Makes the tree hold the editable directories of \a bank in one transaction with
        /// \a message : the directories at \a replaced are replaced, the directories missing
        /// from the tree are added, and the directories no longer editable or no longer in
        /// \a bank are removed. The other directories are unchanged.
        bool takeFrom(const VoiceBank &bank, const QList<std::filesystem::path> &replaced,
                      const QString &message, DiagnosticList &diagnostics);

        VoiceBankFileSystemState m_files;
        QList<VoiceBankDirectory> m_excluded;

        /// \sa isIncomplete()
        bool m_rootMissing = false;
        bool m_rootUnreadable = false;
    };

}

#endif // HELLOKIT_EDIT_VOICEBANKSESSION_H
