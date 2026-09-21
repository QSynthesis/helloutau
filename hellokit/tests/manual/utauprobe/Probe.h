#ifndef UTAUPROBE_PROBE_H
#define UTAUPROBE_PROBE_H

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace utauprobe {

    /// One note of a probe, and what it is there to ask.
    struct ProbeNote {
        int length = 480;
        QString lyric;

        /// The entries after \c NoteNum , spelled the way a UST spells them, in the order UTAU
        /// writes them. An entry that is absent is absent on purpose: half the questions a probe
        /// asks are what UTAU does when a note does not carry something.
        QStringList entries;

        QString asks;   ///< which question this note is part of, or empty for a separator
        QString detail; ///< what this note varies, in one phrase
    };

    /// A project built to ask UTAU questions, one per note.
    ///
    /// A probe is written as text rather than through \c UstDocument because what it has to
    /// control is exactly what \c UstDocument settles for you: whether an entry is there at all.
    /// \c Intensity absent and \c Intensity=100 are the same project and not the same question.
    class Probe {
    public:
        struct Settings {
            QString name;     ///< ProjectName
            QString voiceDir; ///< VoiceDir, which UTAU writes with its own %VOICE% in front
            QString outFile;  ///< OutFile
            QString cacheDir; ///< CacheDir
            QString flags;    ///< the project's own flags
            QString tempo = QStringLiteral("120.00");
        };

        void note(ProbeNote note);

        /// A rest, which is what keeps one question from being read as another: the first
        /// readings of a note's pitch curve carry the note before it.
        void rest(int length = 480);

        const QList<ProbeNote> &notes() const {
            return m_notes;
        }

        /// The project, ready to be written out and opened in UTAU.
        QString toUst(const Settings &settings) const;

        /// One line per note saying what it asks, so that reading the answers back is something
        /// a program can do. Tab separated, because that is enough.
        QString toManifest() const;

    private:
        QList<ProbeNote> m_notes;
    };

    /// Everything about an argument that was once open: the flags, the levels, the envelope, the
    /// timing, the length that decides realLength, the pitch line, the vibrato, the cache name,
    /// the tempo and the rests. 455 notes.
    Probe argumentProbe();

    /// Everything about the pitch curve that the argument probe could not settle, because there
    /// every note sat next to another note carrying a curve of its own. Here every note stands
    /// between rests: when UTAU stops drawing a vibrato, what the rule drops and what it leaves,
    /// what reaches the next note, how far a note's tail is bent by the note after it, and when
    /// UTAU sends no curve at all.
    Probe vibratoProbe();

}

#endif // UTAUPROBE_PROBE_H
