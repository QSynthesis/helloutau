#ifndef HELLOKIT_VOICEBANK_VOICEBANK_H
#define HELLOKIT_VOICEBANK_VOICEBANK_H

#include <array>
#include <filesystem>
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

    private:
        VoiceBank() = default;

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
