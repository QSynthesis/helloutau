#ifndef HELLOUTAU_EDITOR_PIANOROLLLAYERS_P_H
#define HELLOUTAU_EDITOR_PIANOROLLLAYERS_P_H

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

    /// The rows of the keys and the lines of the bars and beats, and the background that a press
    /// selects in or draws on
    class PianoRollState::GridLayer : public SceneLayer {
    public:
        explicit GridLayer(PianoRollState *state) : m_state(state) {
        }

        void paint(QPainter &painter, const QRect &exposed) override;

        std::optional<SceneHit> hitTest(QPointF position) const override;

        std::unique_ptr<SceneGesture> press(const SceneHit &hit, QPointF position,
                                            Qt::MouseButton button,
                                            Qt::KeyboardModifiers modifiers) override;

        /// On the portamento a double click inserts a point.
        bool doubleClick(const SceneHit &hit, QPointF position) override;

    private:
        PianoRollState *m_state;
    };

    /// The notes, each a bar in the row of its key with its lyric
    class PianoRollState::NoteLayer : public SceneLayer {
    public:
        explicit NoteLayer(PianoRollState *state) : m_state(state) {
        }

        void paint(QPainter &painter, const QRect &exposed) override;

        std::optional<SceneHit> hitTest(QPointF position) const override;

        std::unique_ptr<SceneGesture> press(const SceneHit &hit, QPointF position,
                                            Qt::MouseButton button,
                                            Qt::KeyboardModifiers modifiers) override;

        bool doubleClick(const SceneHit &hit, QPointF position) override;

    private:
        PianoRollState *m_state;

        void paintNote(QPainter &painter, const QRect &exposed, const Placement &placement);
    };

    /// The envelope of each sung note above its bar, as UTAU draws it: over the fragment of its
    /// sample, which starts the pre-utterance before the note, from the top of the row of the note
    /// upward, a volume of 100 one row high; its intensity at the start of the fragment.
    class PianoRollState::NoteEnvelopeLayer : public SceneLayer {
    public:
        explicit NoteEnvelopeLayer(PianoRollState *state) : m_state(state) {
        }

        void paint(QPainter &painter, const QRect &exposed) override;

        std::optional<SceneHit> hitTest(QPointF position) const override;

    private:
        PianoRollState *m_state;
    };

    /// The parameters of each sung note below its bar, as UTAU shows them: the modulation, as
    /// "mod 100", in the row one key below the note, and the flags as written in the row two
    /// keys below.
    class PianoRollState::NoteParameterLayer : public SceneLayer {
    public:
        explicit NoteParameterLayer(PianoRollState *state) : m_state(state) {
        }

        void paint(QPainter &painter, const QRect &exposed) override;

        std::optional<SceneHit> hitTest(QPointF position) const override;

    private:
        PianoRollState *m_state;
    };

    /// The pitch that the resampler receives for each sung note, as a dashed line over the readings
    /// of its sample, from its pre-utterance to the overlap of the next note: in Mode2 the sum of
    /// the portamento and the vibrato of the note and its neighbours, otherwise the Mode1 curve
    class PianoRollState::RenderedPitchLayer : public SceneLayer {
    public:
        explicit RenderedPitchLayer(PianoRollState *state) : m_state(state) {
        }

        void paint(QPainter &painter, const QRect &exposed) override;

        std::optional<SceneHit> hitTest(QPointF position) const override;

    private:
        PianoRollState *m_state;
    };

    /// The pitch of each sung note: the portamento of its own points as a line through the rows,
    /// and apart from it its vibrato around the middle of its row
    class PianoRollState::PitchLayer : public SceneLayer {
    public:
        explicit PitchLayer(PianoRollState *state) : m_state(state) {
        }

        void paint(QPainter &painter, const QRect &exposed) override;

        /// The handle of a vibrato at position, if any: its points first, then its top edge,
        /// the right edge of its period box and the inside of the box
        std::optional<SceneHit> vibratoHitAt(QPointF position, int begin, int end) const;

        std::optional<SceneHit> hitTest(QPointF position) const override;

        std::unique_ptr<SceneGesture> press(const SceneHit &hit, QPointF position,
                                            Qt::MouseButton button,
                                            Qt::KeyboardModifiers modifiers) override;

        /// On a Mode2 point a double click removes it, unless its note would keep fewer than
        /// two points, which is reported instead.
        bool doubleClick(const SceneHit &hit, QPointF position) override;

    private:
        PianoRollState *m_state;

        // The Mode1 pitch of the sung notes from begin to end, of which notes holds those from
        // first on, drawn as the portamento is: each note over its own span, and a note after
        // a rest or none from its first value on
        void paintBends(QPainter &painter, const QRect &exposed, const QList<kit::Note> &notes,
                        int first, int begin, int end);

        // The context menu of point j of note index: its shape, and its removal
        void showMenu(int index, int j, QPointF position);
    };

    /// What is drawn over everything: the selection rectangle and the playhead
    class PianoRollState::OverlayLayer : public SceneLayer {
    public:
        explicit OverlayLayer(PianoRollState *state) : m_state(state) {
        }

        void paint(QPainter &painter, const QRect &exposed) override;

    private:
        PianoRollState *m_state;
    };

}

#endif // HELLOUTAU_EDITOR_PIANOROLLLAYERS_P_H
