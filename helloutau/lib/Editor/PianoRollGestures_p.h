#ifndef HELLOUTAU_EDITOR_PIANOROLLGESTURES_P_H
#define HELLOUTAU_EDITOR_PIANOROLLGESTURES_P_H

#include <memory>
#include <optional>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QPointF>
#include <QtCore/QRect>
#include <QtGui/QPainter>
#include <QtGui/QPolygonF>

#include <helloutau/Widgets/SceneLayer.h>

#include "PianoRollState_p.h"

namespace hello::daw {

    /// A Ctrl+Alt drag that zooms one axis around the point where it began.
    class PianoRollState::ZoomGesture : public SceneGesture {
    public:
        ZoomGesture(PianoRollState *state, QPointF position, bool lockAxis)
            : m_state(state), m_origin(position), m_last(position), m_lockAxis(lockAxis) {
        }

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override;
        void release(QPointF position, Qt::KeyboardModifiers modifiers) override;
        void cancel() override;

    private:
        PianoRollState *m_state;
        QPointF m_origin;
        QPointF m_last;
        bool m_lockAxis;
        std::optional<Qt::Orientation> m_orientation;
    };

    /// A drag of the selected notes: vertically transposes them, horizontally moves them in the
    /// sequence to the boundary between notes nearest to where they are dragged. A selection with
    /// gaps is first extended to the run of notes it spans, see step 4 in docs/Widgets.md.
    class PianoRollState::MoveGesture : public SceneGesture {
    public:
        MoveGesture(PianoRollState *state, int pressed, QPointF position, bool wasSelected)
            : m_state(state), m_pressed(pressed), m_origin(position), m_wasSelected(wasSelected),
              m_previous(state->selection) {
        }

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override;

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override;

        void cancel() override;

    private:
        PianoRollState *m_state;
        int m_pressed;
        QPointF m_origin;
        bool m_wasSelected;
        QSet<kit::edit::NodeId> m_previous;
        bool m_dragging = false;
        int m_first = 0;
        int m_last = 0;
        int m_semitones = 0;
        int m_destination = 0;

        void start();
    };

    /// A drag of the right edge of a note, which changes its length
    class PianoRollState::LengthGesture : public SceneGesture {
    public:
        LengthGesture(PianoRollState *state, int index, Qt::KeyboardModifiers modifiers)
            : m_state(state), m_index(index), m_start(state->timeline->note(index).start),
              m_original(state->timeline->note(index).length), m_length(m_original),
              m_modifiers(modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) {
        }

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override;

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override;

        void cancel() override;

    private:
        PianoRollState *m_state;
        int m_index;
        qint64 m_start;
        int m_original;
        int m_length;
        Qt::KeyboardModifiers m_modifiers;

        int lengthAt(QPointF position, Qt::KeyboardModifiers modifiers) const;
    };

    /// A drag on the background that selects the notes in a rectangle, added to the selection
    /// with Ctrl
    class PianoRollState::BandGesture : public SceneGesture {
    public:
        BandGesture(PianoRollState *state, QPointF position, Qt::KeyboardModifiers modifiers);

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override;

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override;

        void cancel() override;

        bool wantsAutoScroll() const override {
            return true;
        }

    private:
        PianoRollState *m_state;
        QPointF m_origin;
        QSet<kit::edit::NodeId> m_base;
        QSet<kit::edit::NodeId> m_previous;
        QSet<kit::edit::NodeId> m_basePoints;
        QSet<kit::edit::NodeId> m_previousPoints;
    };

    /// A drag with the right button, which selects every note in the time it spans whatever its
    /// key, as a drag selects in UTAU and a right drag in QSynthesis; with Ctrl held, in
    /// addition to the notes selected before.
    class PianoRollState::SpanGesture : public SceneGesture {
    public:
        SpanGesture(PianoRollState *state, QPointF position, Qt::KeyboardModifiers modifiers);

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override;

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override;

        void cancel() override;

        bool wantsAutoScroll() const override {
            return true;
        }

    private:
        PianoRollState *m_state;
        double m_origin;
        QSet<kit::edit::NodeId> m_base;
        QSet<kit::edit::NodeId> m_previous;
        QSet<kit::edit::NodeId> m_previousPoints;
    };

    /// A drag of the pen on the background, which draws a note (step 4 in docs/Widgets.md). The
    /// note goes before the note at the pointer, or after the last note, and starts where the note
    /// before it ends; the notes after it start later by its length. With Shift held on the press,
    /// a rest fills the gap from there to the pointer and the note starts at the pointer; within
    /// a rest, the two take its place, and the notes after it start later only as far as the note
    /// passes its end. The drag sets the length, snapped to the quantization. The first note
    /// inserted where a note that sets a tempo started takes that tempo, so that the tempo there
    /// stays.
    class PianoRollState::DrawGesture : public SceneGesture {
    public:
        DrawGesture(PianoRollState *state, QPointF position, Qt::KeyboardModifiers modifiers);

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override;

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override;

        void cancel() override;

    private:
        PianoRollState *m_state;
        // The note before which the note goes, or the number of notes, and where it starts
        int m_index = 0;
        qint64 m_from = 0;
        // Whether a rest fills the gap up to the pointer, and whether it splits a rest
        bool m_fills = false;
        bool m_splits = false;
        qint64 m_start = 0;
        int m_key = 0;
        int m_length = 0;

        // Shows the note, and the notes after it where they move to
        void update(int length);
    };

    /// A drag of the selected points, all by the same time and height. The points of a note are
    /// kept in time order, a moving point passing the others, and the heights that are fixed stay
    /// (heightFixed(), by the place of a point before the drag). Shift snaps the
    /// pressed point to the time of another point of its note, Ctrl its height to PitchSnap.
    class PianoRollState::PointGesture : public SceneGesture {
    public:
        PointGesture(PianoRollState *state, int index, int point, QPointF position)
            : m_state(state), m_index(index), m_point(point), m_origin(position) {
        }

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override;

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override;

        void cancel() override;

    private:
        PianoRollState *m_state;
        int m_index;
        int m_point;
        QPointF m_origin;
        bool m_dragging = false;
        // The points that move, by note index, and the points of those notes before the drag
        QHash<int, QSet<int>> m_moving;
        QHash<int, QList<kit::PortamentoPoint>> m_original;
        // Where the moving points are in the points shown, by note index
        QHash<int, QSet<int>> m_moved;

        void start();
    };

    /// A drag of a handle of the vibrato of a note (see Part). The values are whole numbers, as
    /// UTAU shows them, and the percentages lie between 0 and 100.
    class PianoRollState::VibratoGesture : public SceneGesture {
    public:
        VibratoGesture(PianoRollState *state, int index, int part, QPointF position)
            : m_state(state), m_index(index), m_part(part), m_origin(position),
              m_original(state->vibratoOf(index).value_or(kit::Vibrato())) {
        }

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override;

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override;

        void cancel() override;

    private:
        PianoRollState *m_state;
        int m_index;
        int m_part;
        QPointF m_origin;
        kit::Vibrato m_original;
    };

    /// A stroke of the Mode1 pitch (step 5 in docs/Tuning.md). Each sung note along it takes, at
    /// the places of its values from its first reading to its end, the pitch of the path of the
    /// pointer there, a later part of the path over an earlier one, in whole cents; or 0 for a
    /// stroke that erases, which changes only the values a note has. A note without values
    /// starts them at its first reading, as UTAU does, those before the stroke taking the curve
    /// as it was. Written when released, in one step.
    class PianoRollState::BendGesture : public SceneGesture {
    public:
        BendGesture(PianoRollState *state, QPointF position, bool erases);

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override;

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override;

        void cancel() override;

    private:
        PianoRollState *m_state;
        bool m_erases;
        // The path of the pointer, in ticks and keys
        QList<QPointF> m_path;
        // What is written, by note index: the tick of the first value, and the values
        QHash<int, std::pair<double, QList<double>>> m_drawn;

        void add(QPointF position);

        // The key of the path at tick, where the latest part of the path that spans it lies
        std::optional<double> keyAt(double tick) const;

        void update();
    };

}

#endif // HELLOUTAU_EDITOR_PIANOROLLGESTURES_P_H
