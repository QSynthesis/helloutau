#ifndef HELLOKIT_VOICEBANK_VOICEBANK_H
#define HELLOKIT_VOICEBANK_VOICEBANK_H

#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

#include <QtCore/QCoreApplication>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Support/Diagnostic.h>
#include <hellokit/Support/TextCodec.h>

#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>
#include <hellokit/VoiceBank/VoiceBankSource.h>

namespace hello::kit {

    /// The contents of \c character.txt .
    struct VoiceCharacter {
        /// The display name. Defaults to the folder name if the file specifies none, as in UTAU.
        QString name;

        /// The icon file, relative to the voice bank. UTAU requires a 100 by 100 bitmap.
        QString image;

        /// The audio file played as a preview of the voice bank.
        QString sample;

        QString author;
        QString web;

        /// Lines of \c character.txt that are not entries, in their original order.
        ///
        /// UTAU displays a line containing a colon as part of the character profile, so these
        /// lines are content rather than residue.
        QStringList extraLines;
    };

    /// The prefix and suffix that \c prefix.map adds to a lyric at one key.
    struct VoicePrefix {
        QString prefix;
        QString suffix;
    };

    /// One decoded directory of a voice bank, holding the data from which its files are saved.
    ///
    /// A voice bank is a tree of such directories rather than a single file, and each directory
    /// is edited as a unit: its own \c oto.ini , \c character.txt and \c prefix.map , in its own
    /// encoding. Samples are not stored here but in VoiceBank::samples() , for all directories
    /// together, each referring to its directory. A combined list and a per-directory view are
    /// therefore two views of the same data.
    struct VoiceBankDirectory {
        /// The location relative to the voice bank root. Empty for the root itself.
        std::filesystem::path path;

        /// The encoding in which the files were read and in which they are saved.
        ///
        /// Empty if the directory contained nothing to decode.
        QString charset;

        /// Whether the directory contained text to decode but no encoding was available.
        ///
        /// Its samples remain in the voice bank and are found by file name, as if there were no
        /// \c oto.ini , but none of its text is loaded.
        ///
        /// \warning Nothing in such a directory may be saved. Its files were never read, and
        ///          saving would overwrite them with empty content.
        bool leftOut = false;

        /// Whether part of the text was invalid in \a charset .
        ///
        /// Invalid text is read as empty. The remainder is loaded and usable, but save() does not
        /// write a changed file of this directory, because the empty text would replace the
        /// original.
        bool lossy = false;

        /// The contents of \c hello-config.json in this directory, if present.
        std::optional<VoiceBankConfig> config;

        /// \c character.txt as written in the file, without defaults, if present.
        ///
        /// Kept for every directory, not only the root, because a subdirectory is edited as a
        /// unit. Only the root's file describes the voice bank. See VoiceBank::character() .
        std::optional<VoiceCharacter> character;

        /// \c prefix.map by note number, where 24 is C1, if present.
        std::optional<QMap<int, VoicePrefix>> prefixMap;

        /// \c readme.txt , or empty if absent.
        QString readme;
    };

    /// One way to sing one lyric, corresponding to one line of an \c oto.ini .
    struct VoiceSample {
        /// The absolute path of the audio file.
        std::filesystem::path path;

        /// The index into VoiceBank::directories() of the containing directory.
        int directory = 0;

        /// The audio file as named in the \c oto.ini , relative to its directory. The actual
        /// file name if there is no entry.
        ///
        /// Decoded from the file rather than derived from \a path , because this is the value
        /// that is saved.
        QString fileName;

        /// The name against which a lyric is matched, which may differ from the file name.
        ///
        /// Taken verbatim from the entry, so empty if the entry specifies none, in which case
        /// UTAU uses the file name. Also empty for a sample without an entry. find() matches
        /// both by file name.
        QString alias;

        /// \name Timing parameters in milliseconds, as specified in the \c oto.ini
        ///
        /// All zero for a sample without an entry. See \c hasEntry .
        /// @{
        double offset = 0;
        double consonant = 0;
        double cutoff = 0;
        double preUtterance = 0;
        double voiceOverlap = 0;
        /// @}

        /// Whether an \c oto.ini entry specified the parameters above.
        ///
        /// A voice bank may be distributed without entries, and UTAU then sings the file whose
        /// name matches the lyric. Such a sample has no timing, which differs from timing that
        /// is deliberately zero, and an editor should indicate the difference.
        bool hasEntry = false;

        /// The original text of the five numbers above in the \c oto.ini , used to save an
        /// unchanged entry verbatim. See utau::OtoEntry::spellings .
        std::array<std::string, 5> spellings;
    };

    /// The differences between the disk and the state from which a VoiceBank was read. Every
    /// path is a directory relative to the root.
    ///
    /// Returned by VoiceBank::checkDisk() and applied by VoiceBank::reloadFromDisk() .
    struct VoiceBankChanges {
        /// Directories whose contents changed.
        QList<std::filesystem::path> changed;

        /// New directories. Each includes its subtree, which is not listed separately.
        QList<std::filesystem::path> added;

        /// Removed directories, each including its former subtree.
        QList<std::filesystem::path> removed;

        /// The root does not exist. No other directory was examined.
        bool rootNotFound = false;

        inline bool isEmpty() const {
            return changed.isEmpty() && added.isEmpty() && removed.isEmpty() && !rootNotFound;
        }
    };

    /// A decoded voice bank that resolves lyrics to samples.
    ///
    /// \sa VoiceBankSource for the preceding step, and for the reason the two are separate.
    class HELLOKIT_VOICEBANK_EXPORT VoiceBank {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::VoiceBank)
    public:
        /// Reads and decodes \a root , querying \a selector for each directory whose encoding
        /// is not recorded.
        ///
        /// \param selector may be null, in which case every directory without a recorded
        ///        encoding is left out with a warning rather than decoded by guesswork
        static std::optional<VoiceBank> open(const std::filesystem::path &root,
                                             VoiceBankCharsetSelector *selector,
                                             DiagnosticList &diagnostics);

        /// \overload for a source that has already been read.
        static std::optional<VoiceBank> fromSource(const VoiceBankSource &source,
                                                   VoiceBankCharsetSelector *selector,
                                                   DiagnosticList &diagnostics);

        /// Returns whether UTAU on this machine reads files encoded in \a charset correctly.
        ///
        /// UTAU reads \c oto.ini , \c prefix.map and \c character.txt in the ANSI code page of
        /// the host machine, and a voice bank has no means of specifying otherwise. A voice bank
        /// in any other encoding, including UTF-8, is therefore garbled in UTAU on that machine,
        /// and so are the file names in its \c oto.ini , which then refer to files UTAU cannot
        /// find.
        ///
        /// Intended for the warning shown before an encoding is changed. The result applies to
        /// this machine only: a Shift_JIS voice bank is readable by UTAU on a Japanese system
        /// but not on a Chinese one.
        ///
        /// \return always false on systems other than Windows, which have no ANSI code page
        /// \note UTF-8 with a byte order mark has not been tested in UTAU.
        static bool isCharsetReadableByUtau(const QString &charset);

        inline const std::filesystem::path &root() const {
            return m_root;
        }

        /// Every non-empty directory, the root first, in the order in which they were read.
        inline const QList<VoiceBankDirectory> &directories() const {
            return m_directories;
        }

        /// The name and display information of the voice bank: the root's \c character.txt ,
        /// with the folder name as the default name.
        inline const VoiceCharacter &character() const {
            return m_character;
        }

        /// \c readme.txt , or empty if the voice bank has none.
        inline const QString &readme() const {
            return m_readme;
        }

        /// All samples of the voice bank, in the order in which the directories were read.
        inline const QList<VoiceSample> &samples() const {
            return m_samples;
        }

        /// Returns \a lyric at \a noteNum with \c prefix.map applied.
        QString prefixedLyric(int noteNum, const QString &lyric) const;

        /// Returns the sample for \a lyric at \a noteNum , or null if the voice bank has none.
        ///
        /// The prefix map is applied first, and the result is then looked up as an alias and
        /// then as a sample file name. If \c prefix.map has no entry for the key, the lyric is
        /// looked up unchanged.
        ///
        /// Matching by file name is UTAU behavior, not an invention of this library: UTAU also
        /// treats a sample name as an alias, which is why voice bank authors prefix names with
        /// \c _ to exclude them.
        ///
        /// \note If a voice bank registers the same alias twice, the first one read is used.
        ///       The choice UTAU makes in this case has not been measured.
        ///
        /// \warning The result points into this object and must not outlive it.
        const VoiceSample *find(int noteNum, const QString &lyric) const;

        /// Replaces all samples. Entries are changed, added and removed through this function.
        ///
        /// A sample without an entry denotes the bare file and is not saved. A sample that
        /// refers to a nonexistent directory causes save() to fail.
        void setSamples(QList<VoiceSample> samples);

        /// Replaces directory \a index . Its \c character.txt , \c prefix.map , \c readme.txt
        /// and encoding are changed through this function. The path is retained.
        void setDirectory(int index, VoiceBankDirectory directory);

        /// Rereads directory \a index from disk in \a charset .
        ///
        /// Intended for a directory that was read in the wrong encoding, or that was left out
        /// because no encoding was specified. The files are unchanged and are decoded
        /// differently. This is the alternative to setDirectory() , which keeps the text and
        /// saves the files in another encoding.
        ///
        /// \warning Unsaved changes to the directory are discarded, because the directory is
        ///          read anew.
        ///
        /// The encoding is recorded by the next save() , unless part of the text was invalid in
        /// it (see VoiceBankDirectory::lossy), in which case it is not worth recording.
        bool reread(int index, const QString &charset, DiagnosticList &diagnostics);

        /// Returns the differences between the disk and the state from which the voice bank was
        /// read, at and under \a places , which are absolute paths that may have changed. **The
        /// voice bank itself is not modified.**
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

        /// Applies the result of checkDisk(): rereads the directories in \a changes marked as
        /// changed, drops removed directories and reads new ones.
        ///
        /// **Unsaved changes in a reread directory are discarded**, because the user requested
        /// the version on disk. Each directory is examined again during the reload, because the
        /// disk may have changed since the check. A directory removed in the meantime is not
        /// read, and one restored in the meantime is not dropped.
        ///
        /// A previously read directory keeps its encoding, or takes the one its configuration
        /// now records, without querying the selector again.
        ///
        /// \param selector queried only for a new directory, or for one that previously needed
        ///        no encoding and now does
        /// \return the changes applied
        VoiceBankChanges reloadFromDisk(const VoiceBankChanges &changes,
                                        VoiceBankCharsetSelector *selector,
                                        DiagnosticList &diagnostics);

        /// Rereads every directory regardless of the result of checkDisk(), and applies added
        /// and removed directories.
        ///
        /// Intended for an explicit user request, the equivalent of the refresh button in UTAU
        /// for cases where something appears wrong. No stamp is trusted, so the result is
        /// correct even if stamps are unreliable, for example on a network share with
        /// unreliable timestamps, at the cost of reading every file. Unsaved changes are handled
        /// as in reloadFromDisk() .
        VoiceBankChanges reloadAllFromDisk(VoiceBankCharsetSelector *selector,
                                           DiagnosticList &diagnostics);

        /// Returns whether directory \a index has changes that save() would write.
        bool isModified(int index) const;

        /// Makes the next save() record the encoding of directory \a index , even if nothing
        /// else in the directory changed.
        ///
        /// Intended for an encoding the user selected when the voice bank was opened, which
        /// open() does not record by itself. Without this call, the user is asked again every
        /// time the voice bank is opened.
        void rememberCharset(int index);

        /// Saves every file that differs from the state in which it was read, each in the
        /// encoding of its directory, and nothing else.
        ///
        /// All checks precede the first write, and a single failure leaves every file
        /// unchanged. The following are refused:
        ///
        /// - **Text that the encoding cannot represent.** It is never written as question marks.
        /// - **A file that changed on disk since it was read**, because it contains changes made
        ///   elsewhere. The voice bank must be reopened.
        /// - **A directory that was never read**, or text that was invalid. See
        ///   VoiceBankDirectory::leftOut and VoiceBankDirectory::lossy .
        ///
        /// For each directory in which a file is written, the encoding is recorded beside it in
        /// \c hello-config.json , because saving is an explicit request to write files there.
        /// Each file is written to a temporary file beside it and then renamed, so that a reader
        /// never observes a partially written file.
        ///
        /// \warning Atomicity holds only until the first write. If the disk fails partway
        ///          through, files written so far remain written, and the failing file is
        ///          reported.
        bool save(DiagnosticList &diagnostics);

    private:
        VoiceBank() = default;

        /// Updates the lookup tables and the root-derived data from the samples and
        /// directories.
        void reindex();

        /// Records the current serialization of directory \a index as the baseline for save().
        void takeBaseline(int index);

        /// \name Changing the set of directories
        ///
        /// None of these functions reindexes or takes a baseline, so that a reload affecting
        /// several directories performs each once.
        /// @{
        void replaceDirectory(int index, const VoiceBankDirectorySource &source,
                              const std::optional<TextCodec> &codec, DiagnosticList &diagnostics);
        void appendDirectory(const VoiceBankDirectorySource &source,
                             const std::optional<TextCodec> &codec, DiagnosticList &diagnostics);
        void removeDirectory(int index);
        /// @}

        /// Per-directory file state required only by save().
        struct Book {
            /// The state of each file on disk when it was read or last written.
            std::map<VoiceBankFile, VoiceBankFileRecord> files;
            /// The serialization of each file without changes. A file whose serialization still
            /// equals this is not written.
            std::map<VoiceBankFile, QByteArray> baseline;
            /// Whether the encoding is to be recorded regardless of other changes.
            bool remember = false;
            /// The directory state when last read, or when last checked without differences.
            VoiceBankDirectoryStamp stamp;
        };
        QList<Book> m_books; // one per directory

        std::filesystem::path m_root;
        QList<VoiceBankDirectory> m_directories;
        VoiceCharacter m_character;
        QString m_readme;
        QList<VoiceSample> m_samples;

        // Both map to indices into m_samples and own nothing.
        QHash<QString, int> m_byAlias;
        QHash<QString, int> m_byStem;

        // The prefix map of the root, which is the one applied to lyrics.
        QMap<int, VoicePrefix> m_prefixMap;
    };

}

#endif // HELLOKIT_VOICEBANK_VOICEBANK_H
