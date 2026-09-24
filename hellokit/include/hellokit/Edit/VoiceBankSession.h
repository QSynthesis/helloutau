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

        /// Returns the subdirectories that were not read, or whose text did not decode, as read.
        /// They are not in the tree and not in snapshot(), as if they did not exist, and are read
        /// again in another encoding to be edited. See VoiceBankDirectory::leftOut and
        /// VoiceBankDirectory::lossy .
        inline const QList<VoiceBankDirectory> &excludedDirectories() const {
            return m_excluded;
        }

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

    private:
        VoiceBankSession(VoiceBankDiskState::Opened opened, QObject *parent);

        VoiceBankDiskState m_disk;
        QList<VoiceBankDirectory> m_excluded;
    };

}

#endif // HELLOKIT_EDIT_VOICEBANKSESSION_H
