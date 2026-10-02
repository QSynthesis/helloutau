#ifndef HELLOUTAU_EDITOR_PIANOROLLSTATE_P_H
#define HELLOUTAU_EDITOR_PIANOROLLSTATE_P_H

#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QPointF>
#include <QtCore/QRectF>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtGui/QColor>
#include <QtWidgets/QLineEdit>

#include <hellokit/Document/DocumentConstants.h>
#include <hellokit/Document/Note.h>
#include <hellokit/Edit/ProjectEdits.h>
#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Edit/ProjectSession.h>
#include <hellokit/Edit/TrackTimeline.h>
#include <hellokit/Synth/SampleTiming.h>
#include <hellokit/VoiceBank/VoiceBank.h>

#include <helloutau/Widgets/SceneLayer.h>
#include <helloutau/Widgets/SceneView.h>

#include "PianoRoll.h"

class QButtonGroup;
class QComboBox;
class QToolButton;
class QWidget;

namespace hello::daw {

    class PianoKeyboard;
    class TimelineRuler;

    /// The data of a piano roll that the roll, its layers and its gestures share, with the
    /// functions that read and edit it. PianoRoll::Impl derives from it. The layers and the
    /// gestures are nested in it and declared by group in the private headers
    /// PianoRollLayers_p.h, PianoRollParameterLayers_p.h and PianoRollGestures_p.h.
    class PianoRollState {
    public:
        /// UST has no time signature, and UTAU shows 4/4.
        static constexpr int BeatsPerBar = 4;
        static constexpr int BarTicks = kit::ticksPerQuarter * BeatsPerBar;

        /// The scene extends this many bars past the last note, and has at least this many.
        static constexpr int TrailingBars = 8;
        static constexpr int MinimumBars = 32;

        /// Beat lines are drawn only this far apart at least, in pixels.
        static constexpr double MinimumBeatSpacing = 8;

        static constexpr int LyricPadding = 3;
        static constexpr double NoteRadius = 2;

        /// The right edge of a note is dragged to change its length within this many pixels, on a
        /// note at least this wide, so that a narrow note can still be moved.
        static constexpr double EndGrip = 4;
        static constexpr double MinimumGripWidth = 12;

        /// The lyric editor is at least this wide, in pixels.
        static constexpr int MinimumEditorWidth = 80;

        static constexpr int DefaultQuantization = kit::ticksPerQuarter / 4;

        /// The type of the notes that PianoRoll::copySelected() puts on the clipboard
        static constexpr char NotesMimeType[] = "application/x-helloutau-notes+json";

        /// The part of the view to the left of the playhead after the view follows it
        static constexpr double FollowMargin = 0.1;

        /// The pitch curves are sampled this many pixels apart, and at least a tick apart.
        static constexpr double CurveStep = 2;

        /// A Mode2 point is drawn with this radius, and by default hit within this distance, in
        /// pixels; by default a double click inserts one within this distance of the portamento.
        /// See PianoRoll::pointGrip() and PianoRoll::curveGrip().
        static constexpr double PointRadius = 3.5;
        static constexpr double DefaultPointGrip = 6;
        static constexpr double DefaultCurveGrip = 5;

        /// The points of the notes whose portamento is not under the pointer have this radius.
        static constexpr double FaintPointRadius = 3;

        /// Ctrl snaps the height of a point to this many cents.
        static constexpr double PitchSnap = 50;

        /// A note without points receives these, at this many milliseconds on either side of
        /// its start, when a point is inserted.
        static constexpr double DefaultPortamento = 15;

        /// The Mode1 values lie this many ticks apart (kit::PitchBend).
        static constexpr double BendInterval = 5;

        /// The parameter area below the roll is this high, spans the volumes of an envelope from 0
        /// to this many percent, and leaves this many pixels above and below them.
        static constexpr int ParameterHeight = 120;
        static constexpr double EnvelopeRange = 200;
        static constexpr double ParameterMargin = 6;

        /// The values that the parameter area offers, as QSynthesis does: intensity from 0,
        /// modulation from its negative, and velocity from VelocityMinimum, whose half below
        /// the default is drawn at half the scale of the half above it
        static constexpr double IntensityRange = 200;
        static constexpr double ModulationRange = 200;
        static constexpr double VelocityRange = 200;
        static constexpr double VelocityMinimum = -100;

        /// The envelope of a note that gives none, as UTAU applies it: 0 5 35 0 100 100 0
        static kit::Envelope defaultEnvelope();

        /// The trapezoid of a vibrato stands on a line this many rows below the pitch of its note,
        /// this many cents to a row high, and its period box hangs this many rows below that line
        /// (the form of OpenUtau).
        static constexpr double VibratoBaseline = 3;
        static constexpr double VibratoCentsPerRow = 50;
        static constexpr double VibratoBoxHeight = 0.5;

        static QString tempoText(double tempo);

        /// One note where a gesture shows it. index is the index in the timeline, or -1 for a note
        /// being drawn.
        struct Placement {
            int index = -1;
            qint64 start = 0;
            int length = 0;
            int key = 0;
        };

        /// The editor of a lyric, which reports the keys that end the editing.
        class LyricEditor : public QLineEdit {
        public:
            using QLineEdit::QLineEdit;

            std::function<void()> committed;
            std::function<void()> cancelled;
            std::function<void(bool forward)> tabbed;

        protected:
            bool event(QEvent *event) override;

            void keyPressEvent(QKeyEvent *event) override;

            void focusOutEvent(QFocusEvent *event) override;
        };

        /// The piano roll, whose signals and colors the state uses
        PianoRoll *widget = nullptr;

        class GridLayer;
        class NoteLayer;
        class NoteEnvelopeLayer;
        class NoteParameterLayer;
        class RenderedPitchLayer;
        class PitchLayer;
        class OverlayLayer;
        class EnvelopeLayer;
        class EnvelopeGesture;
        class DragLabelLayer;
        class ValueLayer;
        class ValueGesture;
        class MoveGesture;
        class LengthGesture;
        class BandGesture;
        class SpanGesture;
        class DrawGesture;
        class PointGesture;
        class VibratoGesture;
        class BendGesture;

        kit::ProjectSession *session = nullptr;
        kit::TrackTimeline *timeline = nullptr;
        SceneView *view = nullptr;
        TimelineRuler *ruler = nullptr;
        PianoKeyboard *keyboard = nullptr;
        QToolButton *voiceBankButton = nullptr;
        LyricEditor *editor = nullptr;
        bool refreshPending = false;
        std::shared_ptr<const kit::VoiceBank> voiceBank;
        PianoRoll::Tool tool = PianoRoll::SelectTool;
        int quantization = DefaultQuantization;

        QSet<kit::edit::NodeId> selection;
        /// The selected Mode2 points, by the identifiers of their nodes
        QSet<kit::edit::NodeId> selectedPoints;
        /// What a gesture shows instead of the points of some notes, by note index
        QHash<int, QList<kit::PortamentoPoint>> pointPreview;
        /// What a gesture shows instead of the vibrato of a note, by note index
        QHash<int, kit::Vibrato> vibratoPreview;
        /// What a gesture shows instead of the envelope of a note, by note index
        QHash<int, kit::Envelope> envelopePreview;
        /// What a gesture shows instead of the Mode1 values of a note, by note index
        QHash<int, kit::PitchBend> bendPreview;

        /// The parameter area, what it shows, and the buttons that choose it
        SceneView *parameters = nullptr;
        PianoRoll::Lane lane = PianoRoll::EnvelopeLane;
        QButtonGroup *laneButtons = nullptr;
        QWidget *laneBar = nullptr;
        QColor envelopeColor;
        QColor parameterColor;
        /// What a gesture shows instead of the value of some notes, by note index
        QHash<int, double> valuePreview;

        /// The text that a drag in the parameter area shows beside the dragged handle
        struct DragLabel {
            QPointF handle;
            QString text;
        };
        std::optional<DragLabel> dragLabel;

        /// The timing of the sample of every note, computed again after a change
        QList<kit::SampleTiming> timings;
        bool timingsStale = true;
        /// The note from which Shift extends the selection
        kit::edit::NodeId anchor = 0;

        /// What a gesture shows instead of the timeline: every note if placements is not empty,
        /// a note being drawn, and a selection rectangle, in view coordinates.
        QList<Placement> placements;
        std::optional<Placement> drawn;
        std::optional<QRectF> band;

        /// The note whose lyric is edited, or 0
        kit::edit::NodeId editing = 0;

        bool pitchVisible = true;
        bool renderedPitchVisible = false;
        bool envelopesVisible = false;
        bool parametersVisible = false;
        double pointGrip = DefaultPointGrip;
        double curveGrip = DefaultCurveGrip;
        QColor pitchColor;
        QColor renderedPitchColor;
        QColor vibratoColor;
        QColor faintPointColor;

        std::optional<double> playhead;
        /// The playhead at rest, and whether it is drawn and moved
        double cursor = 0;
        bool cursorEnabled = true;

        /// The note of each mark of the ruler
        QList<int> markNotes;

        /// The notes of each section of the ruler, first and last, and whether it is a label
        struct SectionNotes {
            int first = 0;
            int last = 0;
            bool label = false;
        };
        QList<SectionNotes> sectionNotes;

        /// Inserts a note of lyric before the first selected note or after the last, with the
        /// key of that note and the quantized length, and selects it.
        bool insert(const QString &lyric, kit::DiagnosticList &diagnostics);

        /// The menu of the ruler at tick: the tempo of the note there
        void showRulerMenu(double tick, const QPoint &globalPosition);

        kit::TextSearch lyricSearch;

        /// How far each note is rendered, and the colors of the states from RenderWaiting on
        QList<PianoRoll::RenderState> renderStates;
        QColor renderColors[4];
        QColor playheadColor;

        QColor noteColor;
        QColor restColor;
        QColor lyricColor;
        QColor unsampledColor;
        QColor unsampledLyricColor;
        QColor selectionColor;
        QColor findMatchColor;
        QColor findMatchTextColor;
        QColor whiteRowColor;
        QColor blackRowColor;
        QColor lineColor;
        QColor barLineColor;

        kit::NoteListRef notes() const;

        /// Whether the project turns Mode2 off, so that the pitch is that of the Mode1 values
        bool mode1() const;

        /// Whether the Mode2 points and the vibratos are drawn and edited
        bool pointsShown() const;

        /// Whether the Mode1 pitch is drawn and edited
        bool bendShown() const;

        /// Whether a press with button draws the Mode1 pitch, which the pitch tool does with
        /// either button; the right button selects a span of time with the other tools
        bool drawsBend(Qt::MouseButton button) const;

        /// A stroke of the Mode1 pitch from position, see BendGesture
        std::unique_ptr<SceneGesture> bendGesture(QPointF position, Qt::MouseButton button);

        int indexOf(kit::edit::NodeId id) const;

        bool isSelected(int index) const;

        /// Selects the notes ids; selecting a note clears the selected points.
        void setSelection(const QSet<kit::edit::NodeId> &ids);
        void updateRulerSelection();

        /// The note whose points are drawn plainly, the others' faintly: the one whose
        /// portamento or point is under the pointer, or -1
        int hovered = -1;

        /// Follows the pointer: the note of the point under it, or else the note whose
        /// portamento it is on. A gesture keeps the note it began on, so that no other note
        /// stands out while it lasts.
        void hover(std::optional<QPointF> position);

        /// Selects the points ids; selecting a point clears the selected notes.
        void selectPoints(const QSet<kit::edit::NodeId> &ids);

        double ticksOf(double milliseconds, int index) const;

        double millisecondsOf(double ticks, int index) const;

        /// Whether the first point of note index starts at the pitch of the previous note, as
        /// the resampler curve has it (kit::PitchCurve)
        bool startsAtPrevious(int index) const;

        /// Whether the height of point j of the count points of note index stays as it is: the
        /// first where it starts at the previous note, and the last
        bool heightFixed(int index, int j, int count) const;

        /// The points of note index, as a gesture shows them if it does
        QList<kit::PortamentoPoint> pointsOf(int index) const;

        /// Where point j of note index is drawn: at the pitch of the previous note if it starts
        /// there
        QPointF positionOf(int index, int j, const kit::PortamentoPoint &point) const;

        /// Shows every value of the lane in the height of the parameter area
        void fitParameters();

        /// The keys that a lane spans, and the value it draws a line at: that of UTAU where a
        /// note gives none
        struct LaneRange {
            double minimum;
            double maximum;
            double fallback;
        };

        /// Where a value of the lane is drawn, in the keys of the parameter area, which span
        /// the minimum and maximum of rangeOf(). Velocity from VelocityMinimum to the default
        /// occupies the keys from 0 to the default, and the values above it their own keys
        /// (QSynthesis).
        static double keyOf(PianoRoll::Lane lane, double value);

        /// The value of the lane drawn at key, the inverse of keyOf()
        static double valueAt(PianoRoll::Lane lane, double key);

        /// The keys that the lane spans, see keyOf()
        static LaneRange rangeOf(PianoRoll::Lane lane);

        /// The quarter of the keys of the lane nearest to key: the minimum, the maximum, or one
        /// of the three lines between them at equal distances
        static double quarterNearest(PianoRoll::Lane lane, double key);

        static kit::ProjectEdits::NoteParameter parameterOf(PianoRoll::Lane lane);

        /// The value of the lane that note index gives, or none
        std::optional<double> storedValueOf(int index) const;

        /// The value of the lane for note index, as a gesture shows it if it does, or that of
        /// UTAU
        double valueOf(int index) const;

        /// The sung notes that an edit of the value of note index changes: the selected ones if
        /// it is selected, or else itself
        QList<int> valueTargets(int index) const;

        /// Sets the value of the lane of the notes indices, or removes it
        void writeValue(const QList<int> &indices, std::optional<double> value);

        /// The timing of the sample of every note, with the voice bank if there is one
        const QList<kit::SampleTiming> &sampleTimings();

        /// The notes from first to last, last excluded, with the points, vibratos and Mode1
        /// values that a gesture previews
        QList<kit::Note> previewedNotes(int first, int last) const;

        /// The envelope of note index, as a gesture shows it if it does, or that of UTAU
        kit::Envelope envelopeOf(int index) const;

        /// Where the fragment of note index lies in the track: its start in milliseconds, and
        /// its length as the wavtool appends it
        std::pair<double, double> fragmentOf(int index);

        /// The anchors of envelope in time order, in milliseconds from the start of a fragment
        /// of length: p1, p2 and p5 count forward from the start, p3 and p4 back from the end,
        /// as the wavtool places them (WavtoolMixer::layOut)
        static QList<double> anchorTimes(const kit::Envelope &envelope, double length);

        /// Whether every envelope anchor lies in the fragment and follows the preceding anchor.
        /// Invalid envelopes can be loaded from UST files written by third-party plugins.
        static bool isValidEnvelope(const kit::Envelope &envelope, double length);

        /// Where the parameter area draws the volume at milliseconds into the fragment of note
        /// index
        QPointF envelopePointOf(int index, double milliseconds, double volume);

        /// The vibrato of note index, as a gesture shows it if it does
        std::optional<kit::Vibrato> vibratoOf(int index) const;

        /// Where the handles of a vibrato are drawn: its trapezoid, from the start of the
        /// vibrato up to the end of its fade-in, along its top to the start of its fade-out, and
        /// down to the end of the note; and the box of one period from its phase on
        struct VibratoShape {
            QPointF start;
            QPointF fadeIn;
            QPointF fadeOut;
            QPointF end;
            QRectF period;
        };

        std::optional<VibratoShape> vibratoShapeOf(int index) const;

        /// Reports why an edit made in the roll was refused, if it was
        void report(const kit::DiagnosticList &diagnostics);

        /// An edit of the points of a note ends them at the pitch of the note, as UTAU draws
        /// them (see step 2 in docs/Tuning.md).
        static void endAtPitch(QList<kit::PortamentoPoint> &points);

        /// Writes the points of several notes, by index, in one step, each ending at the pitch
        /// of its note
        bool writePoints(const QString &message,
                         const QHash<int, QList<kit::PortamentoPoint>> &points,
                         kit::DiagnosticList &diagnostics);

        /// The selected points, by note index and the indices of the points in the note
        QHash<int, QSet<int>> selectedPointIndices() const;

        /// Removes the points of notes, by note index; each note keeps its first and last point
        /// where fewer than two would remain.
        bool removePoints(const QHash<int, QSet<int>> &removed, kit::DiagnosticList &diagnostics);

        /// The note whose portamento lies within curveGrip of position, and the tick there from
        /// the start of the note. The note is the one at that time, or the next one from its first
        /// point on.
        std::optional<std::pair<int, double>> portamentoNear(QPointF position) const;

        /// Inserts a point where position lies on the portamento of a note, and selects it.
        bool insertPointAt(QPointF position);

        void selectOnly(int index);

        void selectRange(int first, int last);

        bool snaps(Qt::KeyboardModifiers modifiers) const;

        /// The nearest grid line to tick, or tick itself without snapping
        qint64 snapped(double tick, Qt::KeyboardModifiers modifiers) const;

        /// The last grid line at or before tick
        qint64 snappedDown(double tick, Qt::KeyboardModifiers modifiers) const;

        QRectF rectOf(qint64 start, int length, int key) const;

        /// The notes in the order of indices, each where the one before ends, with lengths and
        /// keys changed as given
        QList<Placement> layOut(const QList<int> &order, const QHash<int, int> &lengths = {},
                                const QSet<int> &transposed = {}, int semitones = 0) const;

        QList<int> identityOrder() const;

        void clearPreview();

        /// Updates what depends on the notes as a whole, once control returns to the event
        /// loop, so that a transaction of many changes updates it once.
        void scheduleRefresh();

        void refresh();

        /// The render states on the ruler, under the time of their notes, neighbors of one
        /// state in one span
        void updateRenderSpans();

        QColor renderColor(PianoRoll::RenderState state) const;

        QColor &renderColorOf(PianoRoll::RenderState state);

        /// Scrolls so that note index is in view, if it is not.
        void ensureVisible(int index);

        void startEditing(int index);

        /// Ends the editing of a lyric, writing it if commit is true and it changed.
        void finishEditing(bool commit);

        void editNext(bool forward);
    };

}

#endif // HELLOUTAU_EDITOR_PIANOROLLSTATE_P_H
