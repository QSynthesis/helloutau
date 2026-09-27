#ifndef HELLOKIT_DOCUMENT_TEMPOMAP_H
#define HELLOKIT_DOCUMENT_TEMPOMAP_H

#include <optional>

#include <QtCore/QList>

#include <hellokit/Document/HelloKitDocumentGlobal.h>
#include <hellokit/Document/Project.h>

namespace hello::kit {

    /// The positions of the notes of a track in ticks and in milliseconds.
    ///
    /// A note has no stored position: it starts where the preceding note ends, and its tempo
    /// applies from that note until the next note that sets one, see \c Note::tempo . This class
    /// derives both from the lengths and tempos, and converts between ticks and milliseconds
    /// within the track. A quarter note is \c ticksPerQuarter ticks.
    ///
    /// Built from values rather than from a \c Project , so that an editor can build it from its
    /// own representation of the notes.
    class HELLOKIT_DOCUMENT_EXPORT TempoMap {
    public:
        /// An empty map, in which \a initialTempo applies from the start. A tempo that is not
        /// positive is replaced by the default tempo of UST.
        explicit TempoMap(double initialTempo = utau::DEFAULT_VALUE_TEMPO);

        /// The map of the first track of \a project, starting at the tempo of its settings.
        static TempoMap of(const Project &project);

        /// Adds the next note, \a length ticks long. \a tempo applies from this note onward;
        /// without it, the note keeps the tempo of the preceding note.
        ///
        /// A tempo that is not positive is ignored, as if absent, because no duration can be
        /// computed from it. A negative length is counted as zero.
        void append(int length, std::optional<double> tempo);

        int noteCount() const;

        /// The start of note \a index in ticks. \a index may equal noteCount(), which gives the
        /// end of the last note.
        qint64 startTick(int index) const;

        /// The start of note \a index in milliseconds, with the same range of \a index as
        /// startTick().
        double startTime(int index) const;

        /// The tempo in effect for note \a index.
        double tempo(int index) const;

        /// Returns the index of the note that contains \a tick, or -1 before the first note and
        /// noteCount() at or after the end of the last note. A note of zero length contains no
        /// tick.
        int noteAt(double tick) const;

        /// Converts a position in ticks to milliseconds. Before the first note the initial tempo
        /// applies, and after the last note the tempo of the last note.
        double timeOf(double tick) const;

        /// Converts a position in milliseconds to ticks, the inverse of timeOf().
        double tickOf(double time) const;

        /// The duration in milliseconds of \a ticks ticks at \a tempo.
        static double duration(double ticks, double tempo);

    private:
        double m_initialTempo;

        // One more start than notes, the last being the end of the last note
        QList<qint64> m_startTicks;
        QList<double> m_startTimes;
        QList<double> m_tempos;

        // The tempo that applies at and after the end of the last note
        double tempoAfterEnd() const;
    };

}

#endif // HELLOKIT_DOCUMENT_TEMPOMAP_H
