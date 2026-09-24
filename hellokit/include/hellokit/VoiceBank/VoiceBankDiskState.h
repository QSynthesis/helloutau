#ifndef HELLOKIT_VOICEBANK_VOICEBANKDISKSTATE_H
#define HELLOKIT_VOICEBANK_VOICEBANKDISKSTATE_H

#include <filesystem>
#include <map>
#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>
#include <hellokit/VoiceBank/VoiceBank.h>
#include <hellokit/VoiceBank/VoiceBankConfig.h>
#include <hellokit/VoiceBank/VoiceBankSource.h>

namespace hello::kit {

    /// The state of the files of a voice bank on disk, which saving and checking the disk
    /// require.
    ///
    /// Kept apart from VoiceBank , which holds the contents only, so that the contents can be
    /// held elsewhere, such as in the tree of an editing session, and materialized into a
    /// VoiceBank for each save. The two are paired by directory path, not by position: a
    /// directory of the VoiceBank without state here was not read and is saved as a new one, and
    /// state here without a directory of the VoiceBank is skipped.
    class HELLOKIT_VOICEBANK_EXPORT VoiceBankDiskState {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::VoiceBankDiskState)
    public:
        struct Opened;

        /// Reads and decodes \a root , querying \a selector for each directory whose encoding
        /// is not recorded.
        ///
        /// \param selector may be null, in which case every directory without a recorded
        ///        encoding is left out with a warning rather than decoded by guesswork
        static std::optional<Opened> open(const std::filesystem::path &root,
                                          VoiceBankCharsetSelector *selector,
                                          DiagnosticList &diagnostics);

        /// \overload for a source that has already been read.
        static std::optional<Opened> fromSource(const VoiceBankSource &source,
                                                VoiceBankCharsetSelector *selector,
                                                DiagnosticList &diagnostics);

        inline const std::filesystem::path &root() const {
            return m_root;
        }

        /// Returns the directories whose state is kept, the root first, in the order of their
        /// paths.
        QList<std::filesystem::path> directories() const;

        /// Returns whether the directory at \a directory of \a bank has changes that save()
        /// would write.
        bool isModified(const VoiceBank &bank, const std::filesystem::path &directory) const;

        /// Returns the names of the audio files of the directory at \a directory as last read,
        /// with and without an entry, in the order in which they were listed, or an empty list
        /// if the directory has not been read. A reload that applies a change of the audio files
        /// updates them.
        ///
        /// Intended for a holder of the contents that keeps the entries only, such as the tree of
        /// an editing session: the samples without an entry are the files that no entry names.
        QStringList audioFiles(const std::filesystem::path &directory) const;

        /// Makes the next save() record the encoding of the directory at \a directory , even if
        /// nothing else in it changed.
        ///
        /// Intended for an encoding the user selected when the voice bank was opened, which
        /// open() does not record by itself. Without this call, the user is asked again every
        /// time the voice bank is opened.
        void rememberCharset(const std::filesystem::path &directory);

        /// Saves every file of \a bank that differs from the state in which it was read, each in
        /// the encoding of its directory, and nothing else.
        ///
        /// All checks precede the first write, and a single failure leaves every file
        /// unchanged. The following are refused:
        ///
        /// - **Text that the encoding cannot represent.** It is never written as question marks.
        /// - **A file that changed on disk since it was read**, because it contains changes made
        ///   elsewhere. The voice bank must be reopened.
        /// - **A directory whose files were not read**, or text that was invalid. See
        ///   VoiceBankDirectory::leftOut and VoiceBankDirectory::lossy .
        ///
        /// A directory of \a bank without state here, for example one that a reload removed
        /// and an undo in an editing session restored, is saved as a new directory: it is
        /// created if missing, unless its path is a file, and every file of it is written. A file
        /// already there was not read and is refused as a file that changed on disk.
        ///
        /// For each directory in which a file is written, and for each directory whose encoding
        /// differs from the one it was read in, the encoding is recorded beside it in
        /// \c hello-config.json , because saving is an explicit request to write files there.
        /// Each file is written to a temporary file beside it and then renamed, so that a reader
        /// never observes a partially written file.
        ///
        /// \warning Atomicity holds only until the first write. If the disk fails partway
        ///          through, files written so far remain written, and the failing file is
        ///          reported.
        bool save(const VoiceBank &bank, DiagnosticList &diagnostics);

        /// Rereads the directory at \a directory of \a bank from disk in \a charset .
        ///
        /// Intended for a directory that was read in the wrong encoding, or that was left out
        /// because no encoding was specified. The files are unchanged and are decoded
        /// differently. This is the alternative to VoiceBank::setDirectory() , which keeps the
        /// text and saves the files in another encoding.
        ///
        /// \warning Unsaved changes to the directory are discarded, because the directory is
        ///          read anew.
        ///
        /// The encoding is recorded by the next save() , unless part of the text was invalid in
        /// it (see VoiceBankDirectory::lossy), in which case it is not worth recording.
        bool reread(VoiceBank &bank, const std::filesystem::path &directory, const QString &charset,
                    DiagnosticList &diagnostics);

        /// Returns the differences between the disk and the state from which the voice bank was
        /// read, at and under \a places , which are absolute paths that may have changed.
        /// **Nothing is read or applied.**
        ///
        /// Each directory is compared with its VoiceBankDirectoryStamp , which costs a directory
        /// listing and no file reads. If a place is not a known directory, the nearest known
        /// ancestor is examined as well, because a new directory appears in the listing of its
        /// parent.
        ///
        /// **A detected difference is reported again** by every check until reloadFromDisk()
        /// applies it, so that a check whose result was missed loses nothing. Applying it is the
        /// user's decision: a directory that changed on disk while isModified() holds exists in
        /// two versions, and only the user can choose between them.
        ///
        /// A place is a hint and not the only means of detecting a change. The overload without
        /// places examines the entire voice bank. A caller that passes only the reports of a
        /// watcher misses every change the watcher misses. See VoiceBankCheckScheduler .
        VoiceBankChanges checkDisk(const QList<std::filesystem::path> &places);

        /// \overload for the entire voice bank.
        VoiceBankChanges checkDisk();

        /// Applies the result of checkDisk() to \a bank : rereads the directories in \a changes
        /// marked as changed, updates the samples without an entry of those in which only audio
        /// files changed, drops removed directories and reads new ones.
        ///
        /// **Unsaved changes in a reread directory are discarded**, because the user requested
        /// the version on disk. A change of the audio files alone discards nothing. Each directory
        /// is examined again during the reload, because the disk may have changed since the check.
        /// A directory removed in the meantime is not read, and one restored in the meantime is not
        /// dropped.
        ///
        /// A previously read directory keeps its encoding, or takes the one its configuration
        /// now records, without querying the selector again.
        ///
        /// \param selector queried only for a new directory, or for one that previously needed
        ///        no encoding and now does
        /// \return the changes applied
        VoiceBankChanges reloadFromDisk(VoiceBank &bank, const VoiceBankChanges &changes,
                                        VoiceBankCharsetSelector *selector,
                                        DiagnosticList &diagnostics);

        /// Rereads every directory of \a bank regardless of the result of checkDisk(), and
        /// applies added and removed directories.
        ///
        /// Intended for an explicit user request, the equivalent of the refresh button in UTAU
        /// for cases where something appears wrong. No stamp is trusted, so the result is
        /// correct even if stamps are unreliable, for example on a network share with
        /// unreliable timestamps, at the cost of reading every file. Unsaved changes are handled
        /// as in reloadFromDisk() .
        VoiceBankChanges reloadAllFromDisk(VoiceBank &bank, VoiceBankCharsetSelector *selector,
                                           DiagnosticList &diagnostics);

    private:
        VoiceBankDiskState() = default;

        /// The state of one directory.
        struct Book {
            /// The state of each file on disk when it was read or last written.
            std::map<VoiceBankDirectorySource::File, VoiceBankFileRecord> files;
            /// The serialization of each file without changes. A file whose serialization still
            /// equals this is not written.
            std::map<VoiceBankDirectorySource::File, QByteArray> baseline;
            /// The contents of \c hello-config.json when read or last written, if present.
            std::optional<VoiceBankConfig> config;
            /// The canonical encoding the directory was read or last saved in. A different
            /// encoding in the VoiceBank is recorded by the next save.
            QString charset;
            /// Whether the encoding is to be recorded regardless of other changes.
            bool remember = false;
            /// The directory state when last read, or when last checked without differences.
            VoiceBankDirectoryStamp stamp;
            /// The names of the audio files when last read or refreshed. See audioFiles().
            QStringList audioFiles;
        };

        /// Returns the state of a directory as read from \a source and decoded into \a decoded .
        static Book bookOf(const VoiceBankDirectorySource &source,
                           const VoiceBankDirectory &decoded);

        /// Records the current serialization of the directory at \a index of \a bank as the
        /// baseline of \a book .
        static void takeBaseline(const VoiceBank &bank, int index, Book &book);

        /// \name Changing the set of directories of a VoiceBank
        ///
        /// None of these functions reindexes \a bank or takes a baseline, so that a reload
        /// affecting several directories performs each once.
        /// @{
        void replaceDirectory(VoiceBank &bank, int index, const VoiceBankDirectorySource &source,
                              const std::optional<TextCodec> &codec, DiagnosticList &diagnostics);
        void appendDirectory(VoiceBank &bank, const VoiceBankDirectorySource &source,
                             const std::optional<TextCodec> &codec, DiagnosticList &diagnostics);
        void removeDirectory(VoiceBank &bank, int index);

        /// Replaces the samples without an entry of the directory at \a index of \a bank with
        /// the audio files of \a source , keeping every entry and every unsaved change.
        void refreshAudio(VoiceBank &bank, int index, const VoiceBankDirectorySource &source);
        /// @}

        std::filesystem::path m_root;
        std::map<std::filesystem::path, Book> m_books;
    };

    /// The result of VoiceBankDiskState::open() : the contents and the state of their files.
    struct VoiceBankDiskState::Opened {
        VoiceBank bank;
        VoiceBankDiskState disk;
    };

}

#endif // HELLOKIT_VOICEBANK_VOICEBANKDISKSTATE_H
