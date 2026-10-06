#ifndef HELLOKIT_VOICEBANK_VOICEBANK_H
#define HELLOKIT_VOICEBANK_VOICEBANK_H

#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

#include <QtCore/QCoreApplication>
#include <QtCore/QDataStream>
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

        inline bool operator==(const VoiceCharacter &RHS) const {
            return name == RHS.name && image == RHS.image && sample == RHS.sample &&
                   author == RHS.author && web == RHS.web && extraLines == RHS.extraLines;
        }

        inline bool operator!=(const VoiceCharacter &RHS) const {
            return !(*this == RHS);
        }
    };

    /// The prefix and suffix that \c prefix.map adds to a lyric at one key.
    struct VoicePrefix {
        /// \name The keys of \c prefix.map , the note numbers from C1 to B7
        /// @{
        static constexpr int minimumKey = 24;
        static constexpr int maximumKey = 107;
        /// @}

        QString prefix;
        QString suffix;

        inline bool operator==(const VoicePrefix &RHS) const {
            return prefix == RHS.prefix && suffix == RHS.suffix;
        }

        inline bool operator!=(const VoicePrefix &RHS) const {
            return !(*this == RHS);
        }
    };

    // The stream operators of the value stored as a whole in the edit history. They are declared
    // with the type, because Qt records the stream operators of a type where its meta-type is
    // first instantiated. The format is part of the history format and must not change.

    inline QDataStream &operator<<(QDataStream &out, const VoicePrefix &prefix) {
        return out << prefix.prefix << prefix.suffix;
    }

    inline QDataStream &operator>>(QDataStream &in, VoicePrefix &prefix) {
        return in >> prefix.prefix >> prefix.suffix;
    }

    /// One decoded directory of a voice bank, holding the data from which its files are saved.
    ///
    /// A voice bank is a tree of such directories rather than a single file, and each directory
    /// is edited as a unit: its own \c oto.ini , and in the root also \c character.txt ,
    /// \c prefix.map and \c readme.txt , all in the encoding of the directory. Samples are not
    /// stored here but in VoiceBank::samples() , for all directories together, each referring to
    /// its directory. A combined list and a per-directory view are therefore two views of the
    /// same data.
    ///
    /// Bytes that are invalid in the encoding are read as U+FFFD, and the rest of the file is
    /// loaded as usual. VoiceBankFileSystemState::save() refuses to write a changed file whose text
    /// contains U+FFFD, because the original bytes would be lost.
    struct VoiceBankDirectory {
        /// The location relative to the voice bank root. Empty for the root itself.
        std::filesystem::path path;

        /// The encoding in which the files were read and in which they are saved, every text
        /// file of the directory alike. Empty if the directory contained nothing to decode.
        ///
        /// UTF-8 if the \c oto.ini declares it, whatever the configuration records, because a
        /// program that honors the declaration reads the file in UTF-8 regardless of any record
        /// of this library. An \c oto.ini written in UTF-8 carries the declaration on its first
        /// line, and one written in any other encoding carries none. An unmodified file is not
        /// rewritten for this alone.
        ///
        /// \sa VoiceBankDirectorySource::otoDeclaresUtf8()
        QString charset;

        /// Whether the directory contained text to decode but the user selected no encoding.
        ///
        /// Its samples remain in the voice bank and are found by file name, as if there were no
        /// \c oto.ini , but none of its text is loaded.
        ///
        /// \warning Nothing in such a directory may be saved. Its files were never read, and
        ///          saving would overwrite them with empty content.
        bool leftOut = false;

        /// \name Files of the root only
        ///
        /// Absent or empty in a subdirectory, whose files of these names are neither read nor
        /// written.
        ///
        /// \sa VoiceBankDirectorySource::fileNamed(), VoiceBank::character()
        /// @{

        /// \c character.txt as written in the file, without defaults, if present.
        std::optional<VoiceCharacter> character;

        /// \c prefix.map by note number, where 24 is C1, if present.
        std::optional<QMap<int, VoicePrefix>> prefixMap;

        /// \c readme.txt , or empty if absent.
        QString readme;

        /// @}
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
        /// All zero for a sample without an entry.
        ///
        /// \sa hasEntry
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
        /// unchanged entry verbatim, or \c std::nullopt for an entry not read from a file.
        ///
        /// \sa utau::OtoEntry::spellings
        std::array<std::optional<std::string>, 5> spellings;
    };

    /// One entry of an \c oto.ini as an editor inserts it: the fields of a VoiceSample that the
    /// file specifies, without the location that the containing directory determines.
    struct VoiceOtoEntry {
        QString fileName;
        QString alias;
        double offset = 0;
        double consonant = 0;
        double cutoff = 0;
        double preUtterance = 0;
        double voiceOverlap = 0;

        /// The original text of the five numbers. Left empty for a new entry.
        ///
        /// \sa VoiceSample::spellings
        std::array<std::optional<std::string>, 5> spellings;

        inline bool operator==(const VoiceOtoEntry &RHS) const {
            return fileName == RHS.fileName && alias == RHS.alias && offset == RHS.offset &&
                   consonant == RHS.consonant && cutoff == RHS.cutoff &&
                   preUtterance == RHS.preUtterance && voiceOverlap == RHS.voiceOverlap &&
                   spellings == RHS.spellings;
        }

        inline bool operator!=(const VoiceOtoEntry &RHS) const {
            return !(*this == RHS);
        }
    };

    /// The differences between the disk and the state from which a VoiceBank was read. Every
    /// path is a directory relative to the root.
    ///
    /// Returned by VoiceBankFileSystemState::checkDisk() and applied by
    /// VoiceBankFileSystemState::reloadFromDisk() .
    ///
    /// The lists fall into two kinds, which an editor presents differently. A changed text file
    /// and a removed directory concern the edited contents and require the user's decision, so
    /// \a changed and \a removed call for a prominent prompt. A changed set of audio files and a
    /// new directory add or remove only what the user has not edited, so \a audio and \a added
    /// call for an unobtrusive notice. A changed \c hello-config.json is neither: it is reported
    /// in \a config , and only makes the voice bank unsaved.
    ///
    /// \sa docs/Editing.md
    struct VoiceBankChanges {
        /// Directories in which a text file changed: \c oto.ini , or a text file of the root.
        /// Applying the change rereads the directory and discards its unsaved changes.
        QList<std::filesystem::path> changed;

        /// Directories in which only audio files were added or removed. Applying the change
        /// updates the samples without an entry and nothing else: an entry whose audio file was
        /// removed is kept, and no unsaved change is discarded.
        QList<std::filesystem::path> audio;

        /// New directories. Each includes its subtree, which is not listed separately.
        QList<std::filesystem::path> added;

        /// Removed directories, each including its former subtree.
        QList<std::filesystem::path> removed;

        /// Directories whose \c hello-config.json was modified, removed or created by another
        /// program. Nothing is applied: the configuration belongs to HelloUtau, and the next
        /// save writes it again. An editor only marks the voice bank unsaved.
        QList<std::filesystem::path> config;

        /// The root does not exist. No other directory was examined.
        bool rootNotFound = false;

        inline bool isEmpty() const {
            return changed.isEmpty() && audio.isEmpty() && added.isEmpty() && removed.isEmpty() &&
                   config.isEmpty() && !rootNotFound;
        }
    };

    /// A decoded voice bank that resolves lyrics to samples.
    ///
    /// A value: the contents of the voice bank only, copyable like Project . The state of the
    /// files on disk, which saving and checking the disk require, is kept by
    /// VoiceBankFileSystemState .
    ///
    /// See VoiceBankSource for the preceding step, and for the reason the two are separate.
    class HELLOKIT_VOICEBANK_EXPORT VoiceBank {
    public:
        /// Creates the voice bank at \a root from its directories and samples, each sample
        /// referring to a directory by index.
        ///
        /// Intended for a holder of the contents other than a VoiceBank, such as the tree of an
        /// editing session, which assembles a VoiceBank for each save and for synthesis. The
        /// contents are taken as given: nothing is read, and the path of each sample is not
        /// derived from its file name.
        VoiceBank(std::filesystem::path root, QList<VoiceBankDirectory> directories,
                  QList<VoiceSample> samples);

        /// Reads and decodes \a root , querying \a selector for each directory whose encoding
        /// is not recorded.
        ///
        /// For a voice bank that is only read, as for synthesis. To save it or to check the disk,
        /// open it through VoiceBankFileSystemState::open() instead.
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

        inline const std::filesystem::path &root() const {
            return m_root;
        }

        /// Every non-empty directory, the root first, in the order in which they were read.
        inline const QList<VoiceBankDirectory> &directories() const {
            return m_directories;
        }

        /// Returns the index into directories() of the directory at \a directory relative to the
        /// root, or -1 if there is none.
        int indexOf(const std::filesystem::path &directory) const;

        /// The name and display information of the voice bank: the root's \c character.txt ,
        /// with the folder name as the default name.
        inline const VoiceCharacter &character() const {
            return m_character;
        }

        /// Returns the file that the \c image entry \a image of a \c character.txt names in the
        /// voice bank at \a root, or \c std::nullopt if \a image is empty or absolute or resolves
        /// outside \a root, so that a voice bank cannot make the editor read another file.
        static std::optional<std::filesystem::path> imagePathOf(const std::filesystem::path &root,
                                                                const QString &image);

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
        /// \note Which sample is used for an alias that several audio files register is
        ///       undefined. This implementation returns the first one read, but no caller may
        ///       rely on it, and no constraint forbids such an alias.
        ///
        /// \warning The result points into this object and must not outlive it.
        const VoiceSample *find(int noteNum, const QString &lyric) const;

        /// Replaces all samples. Entries are changed, added and removed through this function.
        ///
        /// A sample without an entry denotes the bare file and is not saved. A sample that
        /// refers to a nonexistent directory causes VoiceBankFileSystemState::save() to fail.
        void setSamples(QList<VoiceSample> samples);

        /// Replaces directory \a index . Its \c character.txt , \c prefix.map , \c readme.txt
        /// and encoding are changed through this function. The path is retained.
        void setDirectory(int index, VoiceBankDirectory directory);

    private:
        friend class VoiceBankFileSystemState;

        VoiceBank() = default;

        /// Updates the lookup tables and the root-derived data from the samples and
        /// directories.
        void reindex();

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
