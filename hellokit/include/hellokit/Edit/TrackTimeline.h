#ifndef HELLOKIT_EDIT_TRACKTIMELINE_H
#define HELLOKIT_EDIT_TRACKTIMELINE_H

#include <memory>
#include <optional>
#include <utility>

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <hellokit/Document/TempoMap.h>

#include <hellokit/EditBase/Slot.h>

#include <hellokit/Edit/HelloKitEditGlobal.h>

namespace hello::kit {

    class ProjectSession;

    /// The notes of a track of a session with their positions, for views that draw the track
    /// and find what lies at a position, such as a piano roll.
    ///
    /// The positions are derived data: every note starts where the one before it ends. They are
    /// computed from the tree of the session when first needed after a change, not on each
    /// change, so that a transaction of many changes is followed by one computation. invalidated()
    /// reports that the next access computes them again, once per series of changes.
    class HELLOKIT_EDIT_EXPORT TrackTimeline : public QObject {
        Q_OBJECT
    public:
        /// What a view needs of one note.
        struct Note {
            edit::NodeId id = 0;
            qint64 start = 0;
            int length = 0;
            int key = 0;
            QString lyric;
            bool rest = false;

            /// The tempo that the note sets, or none if it inherits the tempo before it.
            std::optional<double> tempo;
        };

        /// Follows the track at \a trackIndex of \a session.
        explicit TrackTimeline(ProjectSession *session, int trackIndex = 0,
                               QObject *parent = nullptr);
        ~TrackTimeline();

        int noteCount() const;
        const Note &note(int index) const;

        /// The positions of the notes in ticks and milliseconds, starting at the tempo of the
        /// project.
        const TempoMap &tempoMap() const;

        /// The end of the last note, in ticks.
        qint64 length() const;

        /// Returns the index of the note that contains \a tick, or -1 before the first note and
        /// noteCount() after the last, as TempoMap::noteAt().
        int noteAt(double tick) const;

        /// Returns the indices of the notes that overlap the ticks from \a first to \a last, as
        /// the first index and one past the last.
        std::pair<int, int> notesBetween(double first, double last) const;

    Q_SIGNALS:
        /// The track has changed since the positions were last computed.
        void invalidated();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOKIT_EDIT_TRACKTIMELINE_H
