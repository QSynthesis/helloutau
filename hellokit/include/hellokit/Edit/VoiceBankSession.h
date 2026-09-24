#ifndef HELLOKIT_EDIT_VOICEBANKSESSION_H
#define HELLOKIT_EDIT_VOICEBANKSESSION_H

#include <filesystem>
#include <memory>
#include <optional>

#include <QtCore/QJsonObject>
#include <QtCore/QList>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/VoiceBank/VoiceBank.h>
#include <hellokit/VoiceBank/VoiceBankDiskState.h>

#include <hellokit/EditBase/EditSession.h>

#include <hellokit/Edit/HelloKitEditGlobal.h>

namespace hello::kit {

    /// The editing of one voice bank, the second document beside a project. See the section on
    /// the voice bank in docs/Editing.md.
    ///
    /// The session owns the contents of the voice bank as a tree of nodes with the slots of
    /// VoiceBankSchema.h, and the disk state from which the voice bank was read. The tree is the
    /// document while the session exists. A \c VoiceBank is a snapshot of the tree together with
    /// the audio files that the disk state lists, for saving and synthesis. The handles in
    /// VoiceBankRefs.h, obtained from a VoiceBankRef of the session, read and modify the tree.
    class HELLOKIT_EDIT_EXPORT VoiceBankSession : public edit::EditSession {
        Q_OBJECT
    public:
        /// Creates a session that edits the voice bank of \a opened and keeps its disk state.
        ///
        /// A subdirectory that was not read, or whose text did not decode, cannot be saved and is
        /// left out of the tree, see excludedDirectories(). The root directory cannot be left out,
        /// as a file that does not decode cannot be edited either.
        ///
        /// \return the session, or null if the root directory was not read or did not decode,
        ///         with the reason in \a diagnostics
        static std::unique_ptr<VoiceBankSession> create(VoiceBankDiskState::Opened opened,
                                                        DiagnosticList &diagnostics,
                                                        QObject *parent = nullptr);

        ~VoiceBankSession();

        /// Returns the root directory of the voice bank.
        const std::filesystem::path &rootPath() const;

        /// Returns the subdirectories that were not read, or whose text did not decode, as last
        /// read. They are not in the tree and not in snapshot(), as if they did not exist, and are
        /// read again in another encoding to be edited, see reread(). See
        /// VoiceBankDirectory::leftOut and VoiceBankDirectory::lossy .
        QList<VoiceBankDirectory> excludedDirectories() const;

        /// Returns the names of the audio files of the directory at \a directory, with and without
        /// an entry. See VoiceBankDiskState::audioFiles().
        inline QStringList audioFiles(const std::filesystem::path &directory) const {
            return m_disk.audioFiles(directory);
        }

        /// Returns the voice bank in its current state.
        VoiceBank snapshot() const;

        /// Returns the entry of the change log for \a change, or \c std::nullopt for a change
        /// that the log omits. See ProjectSession::logEntry(). The entry names the slot of
        /// \a change by its field name in VoiceBankSchema.h.
        std::optional<QJsonObject> logEntry(const edit::Change &change) const;

        /// \name The files on disk
        ///
        /// The session keeps the disk state of the voice bank, and forwards to it with the
        /// snapshot of the tree, see VoiceBankDiskState. Reading from disk replaces the directories
        /// read again, as one undo step, see the section on changes on disk in docs/Editing.md.
        /// @{

        /// Returns whether saving would write a file, see VoiceBankDiskState::isModified().
        bool isModified() const;

        /// Saves the voice bank, see VoiceBankDiskState::save(). The directories that are not in
        /// the tree are neither written nor removed.
        bool save(DiagnosticList &diagnostics);

        /// See VoiceBankDiskState::rememberCharset().
        void rememberCharset(const std::filesystem::path &directory);

        /// See VoiceBankDiskState::checkDisk().
        VoiceBankChanges checkDisk(const QList<std::filesystem::path> &places);

        /// \overload for the entire voice bank.
        VoiceBankChanges checkDisk();

        /// Applies \a changes , see VoiceBankDiskState::reloadFromDisk().
        ///
        /// The directories read again, added or removed are replaced in the tree in one undo
        /// step, which takes the files as read without checking the constraints. Undoing it
        /// restores the tree and not the disk state, so the restored contents are saved over the
        /// files, and a restored directory is created again. A directory that now does not read
        /// or decode leaves the tree for excludedDirectories(), and one that now does enters it.
        /// A change of the audio files alone creates no undo step.
        ///
        /// \return the changes applied
        VoiceBankChanges reloadFromDisk(const VoiceBankChanges &changes,
                                        VoiceBankCharsetSelector *selector,
                                        DiagnosticList &diagnostics);

        /// Reads every directory again, see VoiceBankDiskState::reloadAllFromDisk() and
        /// reloadFromDisk().
        VoiceBankChanges reloadAllFromDisk(VoiceBankCharsetSelector *selector,
                                           DiagnosticList &diagnostics);

        /// Reads the directory at \a directory again in \a charset , one in the tree or one of
        /// excludedDirectories(), as one undo step. See VoiceBankDiskState::reread() and
        /// reloadFromDisk().
        bool reread(const std::filesystem::path &directory, const QString &charset,
                    DiagnosticList &diagnostics);

        /// @}

    private:
        VoiceBankSession(VoiceBankDiskState::Opened opened, QObject *parent);

        /// Returns the snapshot together with the excluded directories, which the disk state
        /// works on.
        VoiceBank fullBank() const;

        /// Makes the tree hold the editable directories of \a bank in one transaction with
        /// \a message : the directories at \a replaced are replaced, the directories missing
        /// from the tree are added, and the directories no longer editable or no longer in
        /// \a bank are removed. The other directories are unchanged.
        bool takeFrom(const VoiceBank &bank, const QList<std::filesystem::path> &replaced,
                      const QString &message, DiagnosticList &diagnostics);

        VoiceBankDiskState m_disk;
        QList<VoiceBankDirectory> m_excluded;
    };

}

#endif // HELLOKIT_EDIT_VOICEBANKSESSION_H
