#ifndef UTAUPROBE_PROBE_H
#define UTAUPROBE_PROBE_H

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace utauprobe {

    /// One note of a probe, together with the question it tests.
    struct ProbeNote {
        int length = 480;
        QString lyric;

        /// The entries after \c NoteNum , in UST syntax and in the order UTAU writes them. An
        /// absent entry is deliberately absent, because many probe questions concern the
        /// behavior of UTAU when a note lacks an entry.
        QStringList entries;

        QString asks;   ///< the question this note belongs to, or empty for a separator
        QString detail; ///< the varied property, in one phrase
    };

    /// A project designed to determine UTAU behavior, with one question per note.
    ///
    /// A probe is written as text rather than through \c UstDocument , because it must control
    /// exactly what \c UstDocument normalizes: the presence of an entry. An absent \c Intensity
    /// and \c Intensity=100 describe the same project but test different questions.
    class Probe {
    public:
        struct Settings {
            QString name;     ///< ProjectName
            QString voiceDir; ///< VoiceDir, which UTAU writes with its %VOICE% prefix
            QString outFile;  ///< OutFile
            QString cacheDir; ///< CacheDir
            QString flags;    ///< the project flags
            QString tempo = QStringLiteral("120.00");
        };

        void note(ProbeNote note);

        /// A rest, which separates questions from one another, because the first values of the
        /// pitch curve of a note are influenced by the preceding note.
        void rest(int length = 480);

        inline const QList<ProbeNote> &notes() const {
            return m_notes;
        }

        /// The project, ready to be written and opened in UTAU.
        QString toUst(const Settings &settings) const;

        /// One line per note stating its question, so that the answers can be evaluated by a
        /// program. Tab-separated, which is sufficient.
        QString toManifest() const;

    private:
        QList<ProbeNote> m_notes;
    };

    /// All previously unresolved questions about arguments: the flags, the levels, the envelope,
    /// the timing, the length that determines realLength, the pitch line, the vibrato, the cache
    /// name, the tempo and the rests. 455 notes.
    Probe argumentProbe();

    /// The pitch curve questions the argument probe could not resolve, because there every note
    /// was adjacent to another note with its own curve. Here every note is surrounded by rests.
    /// The questions: the threshold below which UTAU omits a vibrato, what the omission rule
    /// removes and what it retains, what carries over into the next note, how far the tail of a
    /// note is bent by the following note, and when UTAU sends no curve at all.
    Probe vibratoProbe();

}

#endif // UTAUPROBE_PROBE_H
