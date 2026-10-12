#ifndef HELLOKIT_EDIT_VOICEBANKSESSION_H
#define HELLOKIT_EDIT_VOICEBANKSESSION_H

#include <filesystem>
#include <memory>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include <QtCore/QJsonObject>
#include <QtCore/QList>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/VoiceBank/FrequencyFormats.h>
#include <hellokit/VoiceBank/MisdecodedNameConversion.h>
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

        /// \name The names of the files
        ///
        /// Each function applies the record of misdecoded file names of the directory in the
        /// tree, or of the excluded directory. See the section on misdecoded file names in
        /// docs/Editing.md.
        /// @{

        /// Returns the logical names of the audio files of the directory at \a directory, with
        /// and without an entry.
        ///
        /// \sa VoiceBankFileSystemState::audioFiles()
        QStringList audioFiles(const std::filesystem::path &directory) const;

        /// \sa VoiceBankFileSystemState::audioPathOf()
        std::filesystem::path audioPathOf(const std::filesystem::path &directory,
                                          const QString &fileName) const;

        /// \sa VoiceBankFileSystemState::undeterminedNames()
        QStringList undeterminedNames(const std::filesystem::path &directory) const;

        /// \sa VoiceBank::logicalPathOf()
        std::filesystem::path logicalPathOf(const std::filesystem::path &directory) const;

        /// Returns the listings of the directories of the tree and of the excluded directories,
        /// in the order of their paths, for a conversion of their names. The listing of an
        /// excluded directory has no entries, because its text was not read.
        ///
        /// \sa VoiceBankFileSystemState::nameListingOf()
        QList<MisdecodedNameConversion::Listing> nameListings() const;

        /// @}

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
        /// the only intact copy, and an editor indicates this prominently. Saving writes it back,
        /// and saveAs() writes it elsewhere, either of which makes the voice bank complete again.
        /// See the section on an incomplete voice bank in docs/Editing.md.
        bool isIncomplete() const;

        /// Saves the voice bank. The directories that are not in the tree are neither written nor
        /// removed. Without the root, every text file of the tree is written again.
        ///
        /// The files written are those that the undo history determines, without comparing
        /// contents: a file whose synchronization point is invalid, and a file that a step
        /// between its synchronization point and the current step touched. See the section on
        /// the files a save writes in docs/Editing.md.
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

        /// One directory of a conversion of misdecoded names into correct names on disk.
        struct NameConversion {
            /// The directory, relative to the root, under its path on disk
            std::filesystem::path directory;
            /// The record of the names of the directory as the user determined them, whose
            /// encodings restore the renamed names
            VoiceBankConfiguration::FileNameEncoding record;
            /// The names on disk of the audio files and the subdirectories that are renamed to
            /// their restored names
            QStringList renamed;
        };

        /// Renames the names of \a conversions on disk to their restored names, together with
        /// the frequency maps of \a formats that follow the renamed audio files, and writes the
        /// record of each directory with the renamed names as correct. A name that the record of
        /// its directory lists in neither list is recorded as correct as well. See the section on
        /// the conversion of names in docs/Editing.md.
        ///
        /// A format that does not follow renamed audio files, which an audio file of the
        /// conversion has a map of, is reported as a warning. The tree and the undo history no
        /// longer hold the disk afterwards, and the caller reads the voice bank again.
        ///
        /// \return whether the conversion was made, as VoiceBankFileSystemState::renameOnDisk()
        ///         makes it
        bool renameOnDisk(const QList<NameConversion> &conversions, const FrequencyFormats *formats,
                          DiagnosticList &diagnostics);

        /// Makes the next save() record the encoding of the directory at \a directory , even if
        /// nothing else in it changed, by invalidating the synchronization point of its
        /// configuration.
        ///
        /// Intended for an encoding the user selected when the voice bank was opened, which
        /// opening does not record by itself. Without this call, the user is asked again every
        /// time the voice bank is opened.
        void rememberCharset(const std::filesystem::path &directory);

        /// Returns whether the next save() writes the configuration of a directory whose contents
        /// are unchanged: a directory marked by rememberCharset() , or a directory read again by
        /// reread() .
        ///
        /// Such a voice bank is unsaved even though nothing in it was edited. An editor that
        /// holds the saved state marks it unsaved after opening. See the section on the saved
        /// state in docs/Editing.md.
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

        /// Reads each directory of \a charsets again in its encoding, as one undo step.
        ///
        /// A directory that cannot be read again is reported in \a diagnostics , and the others
        /// are read regardless.
        ///
        /// \return whether every directory was read again
        bool reread(const QList<std::pair<std::filesystem::path, QString>> &charsets,
                    DiagnosticList &diagnostics);

        /// @}

    private:
        VoiceBankSession(VoiceBankFileSystemState::Opened opened, QObject *parent);

        /// Returns the record of misdecoded file names of the directory at \a directory in the
        /// tree or among the excluded directories.
        std::optional<VoiceBankConfiguration::FileNameEncoding>
            fileNameEncodingOf(const std::filesystem::path &directory) const;

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
        ///
        /// The directories replaced or added are then synchronized with the disk at the new
        /// step, except their configurations if \a recordCharsets is true, because the user
        /// chose their encodings.
        bool takeFrom(const VoiceBank &bank, const QList<std::filesystem::path> &replaced,
                      const QString &message, bool recordCharsets, DiagnosticList &diagnostics);

        /// \name The files that a save writes
        ///
        /// See the section on the files a save writes in docs/Editing.md.
        /// @{

        /// A file of the voice bank: its directory relative to the root, and the file in it.
        using FileKey = std::pair<std::filesystem::path, VoiceBankDirectorySource::File>;

        /// Records the files that the changes of the transaction committed as \a step touched.
        void takeCommit(int step, const std::vector<edit::ChangePtr> &changes);

        /// Adds the files that \a change touched to \a touched .
        void addTouched(const edit::Change &change, std::set<FileKey> &touched) const;

        /// Forgets the steps from \a first to \a last , which left the undo history, and
        /// invalidates the synchronization points among them.
        void discardSteps(int first, int last);

        /// Returns the synchronization point of \a key .
        int syncedStep(const FileKey &key) const;

        /// Returns the files that the next save() writes.
        VoiceBankFileSystemState::Files unsavedFiles() const;

        /// The files that each step touched, by step.
        std::map<int, std::set<FileKey>> m_touched;

        /// The synchronization point of each file whose point differs from \a m_baseStep .
        std::map<FileKey, int> m_synced;

        /// The files whose synchronization point is invalid.
        std::set<FileKey> m_unsynced;

        /// The synchronization point of every file not in \a m_synced : the step at which the
        /// session was created, or the step of the last saveAs().
        int m_baseStep = 0;

        /// The directories that the transaction in progress reads from disk, set by takeFrom().
        std::set<std::filesystem::path> m_reading;

        /// @}

        VoiceBankFileSystemState m_files;
        QList<VoiceBankDirectory> m_excluded;

        /// \sa isIncomplete()
        bool m_rootMissing = false;
        bool m_rootUnreadable = false;
    };

}

#endif // HELLOKIT_EDIT_VOICEBANKSESSION_H
