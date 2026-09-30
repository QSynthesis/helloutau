#ifndef HELLOUTAU_EDITOR_PIANOROLLPARAMETERLAYERS_P_H
#define HELLOUTAU_EDITOR_PIANOROLLPARAMETERLAYERS_P_H

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

    /// The background of the parameter area with a line at the value of UTAU, and on the lane of
    /// the envelopes the envelope of each sung note, over the fragment of its sample: a filled
    /// outline from the start of the fragment through its anchors to its end. On that lane it
    /// answers every position, so that a double click reaches it anywhere.
    class PianoRollState::EnvelopeLayer : public SceneLayer {
    public:
        explicit EnvelopeLayer(PianoRollState *state) : m_state(state) {
        }

        void paint(QPainter &painter, const QRect &exposed) override;

        std::optional<SceneHit> hitTest(QPointF position) const override;

        std::unique_ptr<SceneGesture> press(const SceneHit &hit, QPointF position,
                                            Qt::MouseButton button,
                                            Qt::KeyboardModifiers modifiers) override;

        /// On the middle anchor a double click removes it; on the envelope between the end of
        /// the attack and the start of the release it inserts one there.
        bool doubleClick(const SceneHit &hit, QPointF position) override;

        void write(int index, const kit::Envelope &envelope);

    private:
        PianoRollState *m_state;

        // The notes whose fragments may reach into rect: those at its time and one on either
        // side, since a fragment starts before its note and ends after it
        std::pair<int, int> visibleNotes(const QRect &rect) const;

        // The outline of the envelope of note index: the start of its fragment, its anchors in
        // time order, and the end of its fragment
        QPolygonF outlineOf(int index) const;
    };

    /// A drag of an anchor of an envelope: in time between its neighbours, the others staying
    /// where they are, and in volume from 0 to EnvelopeRange. Times are kept to a tenth of a
    /// millisecond, volumes to a percent.
    class PianoRollState::EnvelopeGesture : public SceneGesture {
    public:
        EnvelopeGesture(PianoRollState *state, int index, int anchor, QPointF position);

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override;

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override;

        void cancel() override;

    private:
        PianoRollState *m_state;
        int m_index;
        int m_anchor;
        QPointF m_origin;
        kit::Envelope m_original;
        double m_length = 0;
    };

    /// The value of the lane of each sung note in the parameter area, as QSynthesis draws it: a
    /// point at the start of the note, a line across the note at the value, and a stem down to
    /// the bottom of the area. A note that leaves the value to the default of UTAU is drawn in
    /// faintPointColor, a selected one filled.
    class PianoRollState::ValueLayer : public SceneLayer {
    public:
        explicit ValueLayer(PianoRollState *state) : m_state(state) {
        }

        void paint(QPainter &painter, const QRect &exposed) override;

        /// The point of a handle, or anywhere on the line across its note
        std::optional<SceneHit> hitTest(QPointF position) const override;

        std::unique_ptr<SceneGesture> press(const SceneHit &hit, QPointF position,
                                            Qt::MouseButton button,
                                            Qt::KeyboardModifiers modifiers) override;

    private:
        PianoRollState *m_state;

        std::pair<int, int> visibleNotes(const QRect &rect) const;

        // The point of the handle of note index, and the right end of its line. A value beyond
        // the lane is drawn at its edge.
        std::pair<QPointF, double> handleOf(int index) const;
    };

    /// A drag of the handle of a value: every note it changes takes the value it is dragged to,
    /// a whole number within the lane.
    class PianoRollState::ValueGesture : public SceneGesture {
    public:
        ValueGesture(PianoRollState *state, int index, QPointF position)
            : m_state(state), m_targets(state->valueTargets(index)), m_origin(position),
              m_start(state->valueOf(index)) {
        }

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override;

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override;

        void cancel() override;

    private:
        PianoRollState *m_state;
        QList<int> m_targets;
        QPointF m_origin;
        double m_start;
    };

}

#endif // HELLOUTAU_EDITOR_PIANOROLLPARAMETERLAYERS_P_H
