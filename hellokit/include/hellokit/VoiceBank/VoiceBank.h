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
    /// is edited as a unit: its own \c oto.ini in its own encoding, and in the root also
    /// \c character.txt , \c prefix.map and \c readme.txt . Samples are not stored here but in
    /// VoiceBank::samples() , for all directories together, each referring to its directory. A
    /// combined list and a per-directory view are therefore two views of the same data.
    struct VoiceBankDirectory {
        /// The location relative to the voice bank root. Empty for the root itself.
        std::filesystem::path path;

        /// The encoding in which the files were read and in which they are saved.
        ///
        /// Empty if the directory contained nothing to decode.
        QString charset;

        /// The encoding that the \c oto.ini declares for itself, as written, or empty if it
        /// declares none. See utau::OtoIni::charset .
        ///
        /// If available, the \c oto.ini is read and written in it instead of \a charset , which
        /// then applies to the other files only. The declaration takes precedence because a
        /// program that honors it reads the file in the declared encoding regardless of any
        /// record of this library. It is written back as the first line.
        ///
        /// An \c oto.ini written in UTF-8 without a declaration receives one, so that such a
        /// program does not read it in the code page of the machine. An unmodified file is not
        /// rewritten for this alone.
        QString otoCharset;

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
        /// Invalid text is read as empty. The remainder is loaded and usable, but
        /// VoiceBankDiskState::save() does not write a changed file of this directory, because the
        /// empty text would replace the original.
        bool lossy = false;

        /// \name Files of the root only
        ///
        /// Absent or empty in a subdirectory, whose files of these names are neither read nor
        /// written. See VoiceBankDirectorySource::fileNamed() and VoiceBank::character() .
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
        /// unchanged entry verbatim, or \c std::nullopt for an entry not read from a file. See
        /// utau::OtoEntry::spellings .
        std::array<std::optional<std::string>, 5> spellings;
    };

    /// The differences between the disk and the state from which a VoiceBank was read. Every
    /// path is a directory relative to the root.
    ///
    /// Returned by VoiceBankDiskState::checkDisk() and applied by
    /// VoiceBankDiskState::reloadFromDisk() .
    ///
    /// The lists fall into two kinds, which an editor presents differently. A changed text file
    /// and a removed directory concern the edited contents and require the user's decision, so
    /// \a changed and \a removed call for a prominent prompt. A changed set of audio files and a
    /// new directory add or remove only what the user has not edited, so \a audio and \a added
    /// call for an unobtrusive notice. See docs/Editing.md.
    struct VoiceBankChanges {
        /// Directories in which a text file changed: \c oto.ini , \c hello-config.json , or a
        /// text file of the root. Applying the change rereads the directory and discards its
        /// unsaved changes.
        QList<std::filesystem::path> changed;

        /// Directories in which only audio files were added or removed. Applying the change
        /// updates the samples without an entry and nothing else: an entry whose audio file was
        /// removed is kept, and no unsaved change is discarded.
        QList<std::filesystem::path> audio;

        /// New directories. Each includes its subtree, which is not listed separately.
        QList<std::filesystem::path> added;

        /// Removed directories, each including its former subtree.
        QList<std::filesystem::path> removed;

        /// The root does not exist. No other directory was examined.
        bool rootNotFound = false;

        inline bool isEmpty() const {
            return changed.isEmpty() && audio.isEmpty() && added.isEmpty() && removed.isEmpty() &&
                   !rootNotFound;
        }
    };

    /// A decoded voice bank that resolves lyrics to samples.
    ///
    /// A value: the contents of the voice bank only, copyable like Project . The state of the
    /// files on disk, which saving and checking the disk require, is kept by VoiceBankDiskState .
    ///
    /// \sa VoiceBankSource for the preceding step, and for the reason the two are separate.
    class HELLOKIT_VOICEBANK_EXPORT VoiceBank {
    public:
        /// Reads and decodes \a root , querying \a selector for each directory whose encoding
        /// is not recorded.
        ///
        /// For a voice bank that is only read, as for synthesis. To save it or to check the disk,
        /// open it through VoiceBankDiskState::open() instead.
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

        /// Returns the index into directories() of the directory at \a directory relative to the
        /// root, or -1 if there is none.
        int indexOf(const std::filesystem::path &directory) const;

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
        /// refers to a nonexistent directory causes VoiceBankDiskState::save() to fail.
        void setSamples(QList<VoiceSample> samples);

        /// Replaces directory \a index . Its \c character.txt , \c prefix.map , \c readme.txt
        /// and encoding are changed through this function. The path is retained.
        ///
        /// A change of encoding also removes the declaration of the \c oto.ini , because the file
        /// is then written in the new encoding. See VoiceBankDirectory::otoCharset .
        void setDirectory(int index, VoiceBankDirectory directory);

    private:
        friend class VoiceBankDiskState;

        VoiceBank() = default;

        /// Returns the canonical name of \a charset , or empty if \a charset is empty.
        ///
        /// TextCodec takes an empty name as the system encoding, which is UTF-8 on most systems
        /// other than Windows and would equal a new UTF-8.
        static QString canonicalCharset(const QString &charset);

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
