#ifndef HELLOKIT_VOICEBANK_VOICEBANKFILESYSTEMSTATE_H
#define HELLOKIT_VOICEBANK_VOICEBANKFILESYSTEMSTATE_H

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <vector>

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QJsonObject>
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
    class HELLOKIT_VOICEBANK_EXPORT VoiceBankFileSystemState {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::VoiceBankFileSystemState)
    public:
        struct Opened;

        /// Files of a voice bank by directory, which is relative to the root.
        using Files = std::map<std::filesystem::path, std::set<VoiceBankDirectorySource::File>>;

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

        /// Returns the names of the audio files of the directory at \a directory as last read,
        /// with and without an entry, in the order in which they were listed, or an empty list
        /// if the directory has not been read. A reload that applies a change of the audio files
        /// updates them.
        ///
        /// Intended for a holder of the contents that keeps the entries only, such as the tree of
        /// an editing session: the samples without an entry are the files that no entry names.
        ///
        /// The names are logical names under \a record , the record of file names of the directory
        /// in the contents: a name that \a record determines as misdecoded is restored.
        /// See the section on misdecoded file names in docs/Editing.md.
        ///
        /// \sa audioPathOf(), VoiceBankDirectory::fileNameEncoding
        QStringList
            audioFiles(const std::filesystem::path &directory,
                       const std::optional<VoiceBankConfig::FileNameEncoding> &record) const;

        /// Returns the absolute path of the audio file with the logical name \a fileName under
        /// \a record in the directory at \a directory , under its name on disk. A name of no
        /// audio file is joined as it is.
        std::filesystem::path
            audioPathOf(const std::filesystem::path &directory,
                        const std::optional<VoiceBankConfig::FileNameEncoding> &record,
                        const QString &fileName) const;

        /// Returns the names on disk of the audio files and the subdirectories of the directory at
        /// \a directory that are not ASCII and that \a record lists in neither list, or an empty
        /// list if \a record has no lists or is absent. These names are used as they are until
        /// the user determines their state.
        QStringList
            undeterminedNames(const std::filesystem::path &directory,
                              const std::optional<VoiceBankConfig::FileNameEncoding> &record) const;

        /// Saves \a files of \a bank , each in the encoding of its directory, and nothing else.
        ///
        /// The state does not determine which files changed. The caller names them, for example
        /// an editing session from its undo history, see the section on the files a save writes
        /// in docs/Editing.md. A named file that \a bank does not have, such as an \c oto.ini
        /// without entries that did not exist, is not created.
        ///
        /// All checks precede the first write, and a single failure leaves every file
        /// unchanged. The following are refused, each listed in \a diagnostics :
        ///
        /// - **Text that the encoding cannot represent.** It is never written as question marks.
        /// - **Text containing U+FFFD**, which stands for bytes that were invalid in the encoding
        ///   when read. Writing it would lose them. A file that is not named is not written and
        ///   is not refused.
        /// - **A file that changed on disk since it was read**, because it contains changes made
        ///   elsewhere. The voice bank must be reopened. A file whose size or modification time
        ///   differs from the stamp is taken as changed, and only a racy file is compared by
        ///   content, as git compares the working tree with its index.
        /// - **A directory whose files were not read.**
        ///
        /// A directory of \a bank without state here, for example one that a reload removed
        /// and an undo in an editing session restored, is saved as a new directory: it is
        /// created if missing, unless its path is a file, and every file of it is written. A file
        /// already there was not read and is refused as a file that changed on disk.
        ///
        /// \c hello-config.json , named as VoiceBankDirectorySource::Config , records the encoding
        /// and the record of misdecoded file names of the directory, with the fields read on
        /// opening that this version does not recognize. The configuration belongs to HelloUtau,
        /// is read on opening only and is exempt from the refusal of a file that changed on disk:
        /// the file on disk is neither read nor checked, and is replaced whenever it is written.
        /// Only a configuration that cannot be written at all, for a folder of its name or a file
        /// that does not open for writing, is refused.
        ///
        /// Each file is written to a temporary file beside it and then renamed, so that a reader
        /// never observes a partially written file.
        ///
        /// \warning Atomicity holds only until the first write. If the disk fails partway
        ///          through, files written so far remain written, and the failing file is
        ///          reported.
        ///
        /// \sa VoiceBankDirectory::leftOut
        bool save(const VoiceBank &bank, const Files &files, DiagnosticList &diagnostics);

        /// Writes \a bank into \a folder as a new voice bank, and returns it as read from there.
        ///
        /// \a folder must not exist or be empty. The text files of every directory of \a bank
        /// are written as by save() into a directory without state, each in the encoding of its
        /// directory. If \a copyOtherFiles is true, every other file under the root of \a bank is
        /// then copied, audio files, images and documents alike, and the files of the directories
        /// that \a bank does not hold as they are. No symbolic link is followed or copied. If the
        /// root no longer exists, only the text files are written, with a warning.
        ///
        /// \return the voice bank read from \a folder , or \c std::nullopt with the reason in
        ///         \a diagnostics
        static std::optional<Opened> saveAs(const VoiceBank &bank,
                                            const std::filesystem::path &folder,
                                            bool copyOtherFiles, DiagnosticList &diagnostics);

        /// Rereads the directory at \a directory of \a bank from disk in \a charset .
        ///
        /// Intended for a directory that was read in the wrong encoding, or that was left out
        /// because no encoding was specified. The files are unchanged and are decoded
        /// differently. This is the alternative to VoiceBank::setDirectory() , which keeps the
        /// text and saves the files in another encoding.
        ///
        /// A directory whose \c oto.ini declares UTF-8 is read in UTF-8 only, and another
        /// encoding is refused.
        ///
        /// \sa VoiceBankDirectorySource::otoDeclaresUtf8()
        ///
        /// \warning Unsaved changes to the directory are discarded, because the directory is
        ///          read anew.
        ///
        /// The record of misdecoded file names of \a bank is kept. Recording the encoding is the
        /// caller's decision, by naming the configuration of the directory to save().
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
        /// user's decision: a directory that changed on disk while it has unsaved changes exists
        /// in two versions, and only the user can choose between them.
        ///
        /// \c hello-config.json is not compared, because it is read on opening only.
        ///
        /// A place is a hint and not the only means of detecting a change. The overload without
        /// places examines the entire voice bank. A caller that passes only the reports of a
        /// watcher misses every change the watcher misses.
        ///
        /// \sa VoiceBankCheckScheduler
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
        /// A previously read directory keeps its encoding without querying the selector again,
        /// and its record of misdecoded file names, whatever its configuration now records. Only
        /// a directory whose encoding is not known takes the one its configuration records.
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
        VoiceBankFileSystemState() = default;

        /// The audio files of one directory under the names that a record of misdecoded file
        /// names determines.
        struct Names {
            std::optional<VoiceBankConfig::FileNameEncoding> record;
            /// The logical names in listing order.
            QStringList audioFiles;
            /// The name on disk of each audio file whose logical name differs.
            std::map<QString, std::filesystem::path> diskNames;
            /// \sa undeterminedNames()
            QStringList undetermined;
        };

        /// The state of one directory.
        struct Book {
            /// The state of each file on disk when it was read or last written.
            std::map<VoiceBankDirectorySource::File, VoiceBankFileRecord> files;
            /// The fields of \c hello-config.json read on opening that this version does not
            /// recognize, which every write of the configuration writes back. The encoding and
            /// the record of misdecoded file names are those of the VoiceBank.
            QJsonObject unknownFields;
            /// The directory state when last read, or when last checked without differences.
            VoiceBankDirectoryStamp stamp;
            /// The names on disk of the audio files when last read or refreshed, in the order in
            /// which they were listed.
            std::vector<std::filesystem::path> audio;
            /// The names under the record last queried, computed on demand.
            mutable std::optional<Names> names;
        };

        /// Returns the names of \a audio , the audio files of a directory whose listing is
        /// \a stamp , under \a record . A name that does not restore, and a restored name that
        /// equals another name of the directory, is used as it is and reported in
        /// \a diagnostics if not null.
        static Names namesOf(const std::vector<std::filesystem::path> &audio,
                             const VoiceBankDirectoryStamp &stamp,
                             const std::optional<VoiceBankConfig::FileNameEncoding> &record,
                             DiagnosticList *diagnostics);

        /// Returns the names of the directory at \a directory under \a record , from the cache
        /// of its state if the record is the same.
        const Names &namesOf(const std::filesystem::path &directory,
                             const std::optional<VoiceBankConfig::FileNameEncoding> &record) const;

        /// Returns the state of a directory as read from \a source .
        static Book bookOf(const VoiceBankDirectorySource &source);

        /// \name Changing the set of directories of a VoiceBank
        ///
        /// None of these functions reindexes \a bank , so that a reload affecting several
        /// directories reindexes it once.
        /// @{
        void replaceDirectory(VoiceBank &bank, int index, const VoiceBankDirectorySource &source,
                              const std::optional<QString> &charset, DiagnosticList &diagnostics);
        /// Appends \a source to \a bank with the record of misdecoded file names \a record .
        void appendDirectory(VoiceBank &bank, const VoiceBankDirectorySource &source,
                             const std::optional<QString> &charset,
                             const std::optional<VoiceBankConfig::FileNameEncoding> &record,
                             DiagnosticList &diagnostics);
        void removeDirectory(VoiceBank &bank, int index);

        /// Replaces the samples without an entry of the directory at \a index of \a bank with
        /// the audio files of \a source , keeping every entry and every unsaved change.
        void refreshAudio(VoiceBank &bank, int index, const VoiceBankDirectorySource &source);
        /// @}

        std::filesystem::path m_root;
        std::map<std::filesystem::path, Book> m_books;
    };

    /// The result of VoiceBankFileSystemState::open() : the contents and the state of their files.
    struct VoiceBankFileSystemState::Opened {
        VoiceBank bank;
        VoiceBankFileSystemState files;
    };

}

#endif // HELLOKIT_VOICEBANK_VOICEBANKFILESYSTEMSTATE_H
