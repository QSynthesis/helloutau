#ifndef HELLOKIT_VOICEBANK_VOICEBANK_H
#define HELLOKIT_VOICEBANK_VOICEBANK_H

#include <filesystem>
#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QHash>
#include <QtCore/QList>
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

    /// One way to sing one lyric, which is one line of an \c oto.ini .
    struct VoiceSample {
        /// The audio file, absolute.
        std::filesystem::path path;

        /// What a lyric is matched against, which need not be the file name.
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
        VoiceCharacter m_character;
        QString m_readme;
        QList<VoiceSample> m_samples;

        // Both give an index into m_samples, and neither owns anything.
        QHash<QString, int> m_byAlias;
        QHash<QString, int> m_byStem;

        struct Affix {
            QString prefix;
            QString suffix;
        };
        QHash<int, Affix> m_prefixMap;
    };

}

#endif // HELLOKIT_VOICEBANK_VOICEBANK_H
