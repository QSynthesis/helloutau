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

#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>
#include <hellokit/VoiceBank/VoiceBankSource.h>

namespace hello::kit {

    /// What \c character.txt says about a bank.
    struct VoiceCharacter {
        /// The folder name where the file says nothing, which is what UTAU shows then.
        QString name;

        /// Icon file, relative to the bank. UTAU wants a 100 by 100 bitmap.
        QString image;

        /// Audio played to preview the bank.
        QString sample;

        QString author;
        QString web;

        /// Lines of \c character.txt that are not entries, in the order they were written.
        ///
        /// UTAU shows a line holding a colon as part of the character's profile, so these are
        /// content rather than leftovers.
        QStringList extraLines;
    };

    /// What \c prefix.map adds around a lyric at one key.
    struct VoicePrefix {
        QString prefix;
        QString suffix;
    };

    /// One directory of a bank, decoded, holding what its files will be written back from.
    ///
    /// A bank is a folder of these rather than one file, and each is edited as what it is: its
    /// own \c oto.ini , \c character.txt and \c prefix.map , in its own encoding. The samples
    /// are not here but in VoiceBank::samples() , all directories together, each saying which
    /// directory it belongs to, so that both a list of everything and one directory's files are
    /// a way of looking at the same thing.
    struct VoiceBankDirectory {
        /// Where it is, relative to the bank root, and empty for the root itself.
        std::filesystem::path path;

        /// The encoding its files were read in, which is the one they are written back in.
        ///
        /// Empty where there was nothing to decode.
        QString charset;

        /// Whether there was something to decode and no encoding to decode it with.
        ///
        /// Its samples are still in the bank, reached by file name as if there were no
        /// \c oto.ini , but none of its text is. \warning Nothing of such a directory may be
        /// written back: its files were never read, and writing would replace them with nothing.
        bool leftOut = false;

        /// Whether some of its text was not valid in \a charset .
        ///
        /// What did not decode reads as empty. The rest is here and usable, but a save refuses
        /// to write any file of it that changed, since the empty text would be written back in
        /// place of the original.
        bool lossy = false;

        /// What \c hello-config.json here says, absent where there is none.
        std::optional<VoiceBankConfig> config;

        /// \c character.txt as the file says it, with nothing filled in, and absent where there
        /// is none.
        ///
        /// Kept for every directory and not only the root, because a subdirectory is edited as
        /// what it holds. It is still only the root's that describes the bank, see
        /// VoiceBank::character() .
        std::optional<VoiceCharacter> character;

        /// \c prefix.map by note number, where 24 is C1, and absent where there is none.
        std::optional<QMap<int, VoicePrefix>> prefixMap;

        /// \c readme.txt , empty where there is none.
        QString readme;
    };

    /// One way to sing one lyric, which is one line of an \c oto.ini .
    struct VoiceSample {
        /// The audio file, absolute.
        std::filesystem::path path;

        /// Which of VoiceBank::directories() it belongs to.
        int directory = 0;

        /// The audio file as the \c oto.ini names it, relative to its directory. The file's own
        /// name where there is no entry.
        ///
        /// Decoded from what was written rather than worked out from \a path , since this is
        /// what is written back.
        QString fileName;

        /// What a lyric is matched against, which need not be the file name.
        ///
        /// As the entry gives it, so empty where it gives none, which UTAU reads as the file
        /// name. Empty as well for a sample with no entry. find() reaches both by file name.
        QString alias;

        /// \name The cut, in milliseconds, as the \c oto.ini gave it
        ///
        /// All zero for a sample the bank has no entry for. \c hasEntry says which.
        /// @{
        double offset = 0;
        double consonant = 0;
        double cutoff = 0;
        double preUtterance = 0;
        double voiceOverlap = 0;
        /// @}

        /// Whether an \c oto.ini entry said any of the above.
        ///
        /// A bank may ship without one, and UTAU then sings the file whose name is the lyric.
        /// Such a sample has no timing at all, which is not the same as timing that is zero by
        /// choice, and an editor showing it should say so.
        bool hasEntry = false;

        /// How the \c oto.ini wrote the five numbers above, which is what an unchanged entry is
        /// written back with. See utau::OtoEntry::spellings .
        std::array<std::string, 5> spellings;
    };

    /// A voice bank, decoded and ready to be asked what sings what.
    ///
    /// \sa VoiceBankSource for the step before this one, and for why there are two.
    class HELLOKIT_VOICEBANK_EXPORT VoiceBank {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::VoiceBank)
    public:
        /// Reads \a root and decodes it, asking \a selector about any directory whose encoding
        /// is not recorded.
        ///
        /// \param selector may be null, and every unrecorded directory is then left out with a
        ///        warning rather than guessed at
        static std::optional<VoiceBank> open(const std::filesystem::path &root,
                                             VoiceBankCharsetSelector *selector,
                                             DiagnosticList &diagnostics);

        /// \overload for a source that has already been read.
        static std::optional<VoiceBank> fromSource(const VoiceBankSource &source,
                                                   VoiceBankCharsetSelector *selector,
                                                   DiagnosticList &diagnostics);

        /// Whether UTAU on this machine reads the files of a directory in \a charset as they are.
        ///
        /// UTAU reads \c oto.ini , \c prefix.map and \c character.txt in the code page of the
        /// machine it runs on, and nothing in a bank says otherwise. So a bank in any other
        /// encoding, UTF-8 included, is mojibake to UTAU there, and so are the file names in its
        /// \c oto.ini , which then name files UTAU cannot find.
        ///
        /// This is what a warning before changing an encoding asks. It answers for this machine
        /// only: a Shift_JIS bank reads in UTAU on a Japanese machine and not on a Chinese one.
        ///
        /// \return always false off Windows, where there is no code page UTAU would use
        /// \note UTF-8 with a byte order mark has not been tried in UTAU.
        static bool isCharsetReadableByUtau(const QString &charset);

        const std::filesystem::path &root() const {
            return m_root;
        }

        /// Every directory that holds anything, the root first, in the order they were read.
        const QList<VoiceBankDirectory> &directories() const {
            return m_directories;
        }

        /// What the bank is called and shown as, which is the root's \c character.txt with the
        /// folder name where that says nothing.
        const VoiceCharacter &character() const {
            return m_character;
        }

        /// \c readme.txt , empty where the bank has none.
        const QString &readme() const {
            return m_readme;
        }

        /// Every way this bank can sing something, in the order the directories were read.
        const QList<VoiceSample> &samples() const {
            return m_samples;
        }

        /// What \a lyric becomes at \a noteNum, which is \c prefix.map applied to it.
        QString prefixedLyric(int noteNum, const QString &lyric) const;

        /// The sample to sing \a lyric at \a noteNum with, or null where the bank has none.
        ///
        /// The prefix map is applied first, then the result is looked for as an alias, then as a
        /// sample's file name. A bank whose \c prefix.map has no entry for that key looks the
        /// plain lyric up, since the map leaves it alone.
        ///
        /// Matching a file name is not a fallback this library invented: UTAU reads a sample's
        /// name as an alias as well, which is why bank authors put a \c _ in front of a name
        /// they do not want sung by it.
        ///
        /// \note Where a bank registers one alias twice, the first read wins. Which one UTAU
        ///       picks has not been measured.
        ///
        /// \warning The result points into this object and does not outlive it.
        const VoiceSample *find(int noteNum, const QString &lyric) const;

        /// Replaces every sample, which is how an entry is changed, added or removed.
        ///
        /// A sample with no entry is the bare file and is not written. One naming a directory
        /// this bank does not have makes save() refuse.
        void setSamples(QList<VoiceSample> samples);

        /// Replaces directory \a index , which is how its \c character.txt , \c prefix.map ,
        /// \c readme.txt or encoding is changed. Its path stays what it was.
        void setDirectory(int index, VoiceBankDirectory directory);

        /// Reads directory \a index again from disk, in \a charset .
        ///
        /// Where the encoding a directory was read in was the wrong one, or where nobody named
        /// one and it was left out. The files stay as they are and are read differently, which
        /// is the other way to set an encoding from setDirectory() : that one keeps the text
        /// and writes the files in another encoding.
        ///
        /// \warning Whatever was changed in the directory and not saved is gone, since the
        ///          point is to read it afresh.
        ///
        /// The encoding is written down by the next save() , unless some of the text did not
        /// read in it, see VoiceBankDirectory::lossy : that one is not worth remembering.
        bool reread(int index, const QString &charset, DiagnosticList &diagnostics);

        /// Has the next save() write down the encoding of directory \a index , even where
        /// nothing else there changed.
        ///
        /// For an encoding a user chose when the bank was opened, which open() does not write
        /// down by itself. Without it the question comes back every time the bank is opened.
        void rememberCharset(int index);

        /// Writes back every file that is no longer what was read, each in its directory's
        /// encoding, and nothing else.
        ///
        /// Everything is checked before anything is written, and one failure leaves every file
        /// as it was. It refuses:
        ///
        /// - **text the encoding cannot hold.** It is never written as a question mark.
        /// - **a file that changed on disk since it was read**, since that is somebody else's
        ///   work. Opening the bank again is the way on.
        /// - **a directory that was never read**, or text that did not decode, see
        ///   VoiceBankDirectory::leftOut and VoiceBankDirectory::lossy .
        ///
        /// A directory that has anything written gets its encoding written down beside it in
        /// \c hello-config.json , since a save is the user asking for files to be written there.
        /// Each file is written beside itself and moved over, so nothing reading the bank ever
        /// finds half of one.
        ///
        /// \warning All or nothing holds up to the first write. A disk that fails half way
        ///          through leaves the files written so far written, and says which one failed.
        bool save(DiagnosticList &diagnostics);

    private:
        VoiceBank() = default;

        /// Brings the lookups and what the root describes up to date with the samples and
        /// directories.
        void reindex();

        /// Takes what directory \a index would be written as now as what save() compares with.
        void takeBaseline(int index);

        /// What save() needs to know about one directory's files and nobody else does.
        struct Book {
            /// What each file held on disk when it was read, or last written.
            std::map<VoiceBankFile, VoiceBankFileRecord> files;
            /// What each file would be written as with nothing changed. A file whose bytes
            /// still come out as this is left alone.
            std::map<VoiceBankFile, QByteArray> baseline;
            /// Whether the encoding is to be written down whatever else is saved.
            bool remember = false;
        };
        QList<Book> m_books; // one per directory

        std::filesystem::path m_root;
        QList<VoiceBankDirectory> m_directories;
        VoiceCharacter m_character;
        QString m_readme;
        QList<VoiceSample> m_samples;

        // Both give an index into m_samples, and neither owns anything.
        QHash<QString, int> m_byAlias;
        QHash<QString, int> m_byStem;

        // The root's, which is the one a lyric goes through.
        QMap<int, VoicePrefix> m_prefixMap;
    };

}

#endif // HELLOKIT_VOICEBANK_VOICEBANK_H
