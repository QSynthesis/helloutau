#include "PianoRoll.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <numeric>

#include <QtCore/QHash>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMimeData>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include <stdcorelib/pimpl.h>
#include <stdutau/utaconst.h>

#include <hellokit/Document/DocumentConstants.h>
#include <hellokit/Edit/ProjectEdits.h>
#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Edit/ProjectSession.h>
#include <hellokit/Edit/TrackTimeline.h>
#include <hellokit/Synth/PitchCurve.h>
#include <hellokit/Synth/SampleTiming.h>
#include <hellokit/VoiceBank/VoiceBank.h>

#include <helloutau/Widgets/PianoKeyboard.h>
#include <helloutau/Widgets/SceneView.h>
#include <helloutau/Widgets/TimelineRuler.h>

#include "VibratoDialog.h"

namespace hello::daw {

    namespace {

        // UST has no time signature, and UTAU shows 4/4.
        constexpr int BeatsPerBar = 4;
        constexpr int BarTicks = kit::ticksPerQuarter * BeatsPerBar;

        // The scene extends this many bars past the last note, and has at least this many.
        constexpr int TrailingBars = 8;
        constexpr int MinimumBars = 32;

        // Beat lines are drawn only this far apart at least, in pixels.
        constexpr double MinimumBeatSpacing = 8;

        constexpr int LyricPadding = 3;
        constexpr double NoteRadius = 2;

        // The right edge of a note is dragged to change its length within this many pixels, on a
        // note at least this wide, so that a narrow note can still be moved.
        constexpr double EndGrip = 4;
        constexpr double MinimumGripWidth = 12;

        // The lyric editor is at least this wide, in pixels.
        constexpr int MinimumEditorWidth = 80;

        constexpr int DefaultQuantization = kit::ticksPerQuarter / 4;

        // The type of the notes that PianoRoll::copySelected() puts on the clipboard
        constexpr char NotesMimeType[] = "application/x-helloutau-notes+json";

        // The part of the view to the left of the playhead after the view follows it
        constexpr double FollowMargin = 0.1;

        // The pitch curves are sampled this many pixels apart, and at least a tick apart.
        constexpr double CurveStep = 2;

        // A Mode2 point is drawn with this radius, and by default hit within this distance, in
        // pixels; by default a double click inserts one within this distance of the portamento.
        // See PianoRoll::pointGrip() and PianoRoll::curveGrip().
        constexpr double PointRadius = 3.5;
        constexpr double DefaultPointGrip = 6;
        constexpr double DefaultCurveGrip = 5;

        // The points of the notes whose portamento is not under the pointer have this radius.
        constexpr double FaintPointRadius = 3;

        // Ctrl snaps the height of a point to this many cents.
        constexpr double PitchSnap = 50;

        // A note without points receives these, at this many milliseconds on either side of
        // its start, when a point is inserted.
        constexpr double DefaultPortamento = 15;

        // The Mode1 values lie this many ticks apart (kit::PitchBend).
        constexpr double BendInterval = 5;

        // The parameter area below the roll is this high, spans the volumes of an envelope from 0
        // to this many percent, and leaves this many pixels above and below them.
        constexpr int ParameterHeight = 120;
        constexpr double EnvelopeRange = 200;
        constexpr double ParameterMargin = 6;

        // The values that the parameter area offers, as QSynthesis does: intensity from 0,
        // modulation from its negative, and velocity from VelocityMinimum, whose half below
        // the default is drawn at half the scale of the half above it
        constexpr double IntensityRange = 200;
        constexpr double ModulationRange = 200;
        constexpr double VelocityRange = 200;
        constexpr double VelocityMinimum = -100;

        // The envelope of a note that gives none, as UTAU applies it: 0 5 35 0 100 100 0
        kit::Envelope defaultEnvelope() {
            kit::Envelope envelope;
            envelope.anchors[0] = {0, 0};
            envelope.anchors[1] = {5, 100};
            envelope.anchors[3] = {35, 100};
            envelope.anchors[4] = {0, 0};
            return envelope;
        }

        // The trapezoid of a vibrato stands on a line this many rows below the pitch of its note,
        // this many cents to a row high, and its period box hangs this many rows below that line
        // (the form of OpenUtau).
        constexpr double VibratoBaseline = 3;
        constexpr double VibratoCentsPerRow = 50;
        constexpr double VibratoBoxHeight = 0.5;

        QString tempoText(double tempo) {
            return QString::number(tempo, 'g', 6);
        }

        // One note where a gesture shows it. index is the index in the timeline, or -1 for a note
        // being drawn.
        struct Placement {
            int index = -1;
            qint64 start = 0;
            int length = 0;
            int key = 0;
        };

        // Calls a function with each event of an object
        class EventWatcher : public QObject {
        public:
            EventWatcher(QObject *watched, std::function<void(QEvent *)> seen)
                : QObject(watched), m_seen(std::move(seen)) {
                watched->installEventFilter(this);
            }

        protected:
            bool eventFilter(QObject *watched, QEvent *event) override {
                m_seen(event);
                return QObject::eventFilter(watched, event);
            }

        private:
            std::function<void(QEvent *)> m_seen;
        };

        // Reports where the pointer is over a widget, and none once it leaves
        class PointerTracker : public QObject {
        public:
            PointerTracker(QWidget *widget, std::function<void(std::optional<QPointF>)> moved)
                : QObject(widget), m_moved(std::move(moved)) {
                widget->installEventFilter(this);
            }

        protected:
            bool eventFilter(QObject *watched, QEvent *event) override {
                if (event->type() == QEvent::MouseMove) {
                    m_moved(static_cast<QMouseEvent *>(event)->position());
                } else if (event->type() == QEvent::Leave) {
                    m_moved(std::nullopt);
                }
                return QObject::eventFilter(watched, event);
            }

        private:
            std::function<void(std::optional<QPointF>)> m_moved;
        };

        // The editor of a lyric, which reports the keys that end the editing.
        class LyricEditor : public QLineEdit {
        public:
            using QLineEdit::QLineEdit;

            std::function<void()> committed;
            std::function<void()> cancelled;
            std::function<void(bool forward)> tabbed;

        protected:
            bool event(QEvent *event) override {
                // Tab is taken here, before focus navigation consumes it.
                if (event->type() == QEvent::KeyPress) {
                    const auto key = static_cast<QKeyEvent *>(event);
                    if (key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab) {
                        tabbed(key->key() == Qt::Key_Tab &&
                               !(key->modifiers() & Qt::ShiftModifier));
                        return true;
                    }
                }
                return QLineEdit::event(event);
            }

            void keyPressEvent(QKeyEvent *event) override {
                switch (event->key()) {
                    case Qt::Key_Return:
                    case Qt::Key_Enter:
                        committed();
                        return;
                    case Qt::Key_Escape:
                        cancelled();
                        return;
                    default:
                        QLineEdit::keyPressEvent(event);
                }
            }

            void focusOutEvent(QFocusEvent *event) override {
                QLineEdit::focusOutEvent(event);
                // The context menu of the editor takes the focus without ending the editing.
                if (event->reason() != Qt::PopupFocusReason) {
                    committed();
                }
            }
        };
    }

    class PianoRoll::Impl {
    public:
        using Decl = PianoRoll;

        class GridLayer;
        class NoteLayer;
        class PitchLayer;
        class OverlayLayer;
        class EnvelopeLayer;
        class EnvelopeGesture;
        class ValueLayer;
        class ValueGesture;
        class MoveGesture;
        class LengthGesture;
        class BandGesture;
        class DrawGesture;
        class PointGesture;
        class VibratoGesture;
        class BendGesture;

        explicit Impl(Decl *decl) : _decl(decl) {
        }

        Decl *_decl;
        kit::ProjectSession *session = nullptr;
        kit::TrackTimeline *timeline = nullptr;
        SceneView *view = nullptr;
        TimelineRuler *ruler = nullptr;
        PianoKeyboard *keyboard = nullptr;
        QComboBox *quantizer = nullptr;
        LyricEditor *editor = nullptr;
        bool refreshPending = false;
        std::shared_ptr<const kit::VoiceBank> voiceBank;
        Tool tool = SelectTool;
        int quantization = DefaultQuantization;

        QSet<kit::edit::NodeId> selection;
        // The selected Mode2 points, by the identifiers of their nodes
        QSet<kit::edit::NodeId> selectedPoints;
        // What a gesture shows instead of the points of some notes, by note index
        QHash<int, QList<kit::PortamentoPoint>> pointPreview;
        // What a gesture shows instead of the vibrato of a note, by note index
        QHash<int, kit::Vibrato> vibratoPreview;
        // What a gesture shows instead of the envelope of a note, by note index
        QHash<int, kit::Envelope> envelopePreview;
        // What a gesture shows instead of the Mode1 values of a note, by note index
        QHash<int, kit::PitchBend> bendPreview;

        // The parameter area, what it shows, and the buttons that choose it
        SceneView *parameters = nullptr;
        Lane lane = EnvelopeLane;
        QButtonGroup *laneButtons = nullptr;
        QWidget *laneBar = nullptr;
        QColor envelopeColor;
        QColor parameterColor;
        // What a gesture shows instead of the value of some notes, by note index
        QHash<int, double> valuePreview;

        // The timing of the sample of every note, computed again after a change
        QList<kit::SampleTiming> timings;
        bool timingsStale = true;
        // The note from which Shift extends the selection
        kit::edit::NodeId anchor = 0;

        // What a gesture shows instead of the timeline: every note if placements is not empty,
        // a note being drawn, and a selection rectangle, in view coordinates.
        QList<Placement> placements;
        std::optional<Placement> drawn;
        std::optional<QRectF> band;

        // The note whose lyric is edited, or 0
        kit::edit::NodeId editing = 0;

        bool pitchVisible = true;
        double pointGrip = DefaultPointGrip;
        double curveGrip = DefaultCurveGrip;
        QColor pitchColor;
        QColor vibratoColor;
        QColor faintPointColor;

        std::optional<double> playhead;
        QColor playheadColor;

        QColor noteColor;
        QColor restColor;
        QColor lyricColor;
        QColor unsampledColor;
        QColor unsampledLyricColor;
        QColor selectionColor;
        QColor whiteRowColor;
        QColor blackRowColor;
        QColor lineColor;
        QColor barLineColor;

        kit::NoteListRef notes() const {
            return kit::ProjectRef(session).tracks().at(0).notes();
        }

        // Whether the project turns Mode2 off, so that the pitch is that of the Mode1 values
        bool mode1() const {
            return !kit::ProjectRef(session).settings().mode2();
        }

        // Whether the Mode2 points and the vibratos are drawn and edited
        bool pointsShown() const {
            return pitchVisible && !mode1();
        }

        // Whether the Mode1 pitch is drawn and edited
        bool bendShown() const {
            return pitchVisible && mode1();
        }

        // Whether a press with button draws the Mode1 pitch: the left button with the pitch
        // tool, the right one with any tool
        bool drawsBend(Qt::MouseButton button) const {
            return bendShown() &&
                   ((button == Qt::LeftButton && tool == PitchTool) || button == Qt::RightButton);
        }

        // A stroke of the Mode1 pitch from position, see BendGesture
        std::unique_ptr<SceneGesture> bendGesture(QPointF position, Qt::MouseButton button);

        int indexOf(kit::edit::NodeId id) const {
            for (int i = 0; i < timeline->noteCount(); ++i) {
                if (timeline->note(i).id == id) {
                    return i;
                }
            }
            return -1;
        }

        bool isSelected(int index) const {
            return selection.contains(timeline->note(index).id);
        }

        // Selects the notes ids; selecting a note clears the selected points.
        void setSelection(const QSet<kit::edit::NodeId> &ids) {
            stdc_decl_t;
            const bool clearsPoints = !ids.isEmpty() && !selectedPoints.isEmpty();
            if (ids == selection && !clearsPoints) {
                return;
            }
            selection = ids;
            if (clearsPoints) {
                selectedPoints.clear();
            }
            view->viewport()->update();
            Q_EMIT decl.selectionChanged();
        }

        // The note whose points are drawn plainly, the others' faintly: the one whose
        // portamento or point is under the pointer, or -1
        int hovered = -1;

        // Follows the pointer: the note of the point under it, or else the note whose
        // portamento it is on. A gesture keeps the note it began on, so that no other note
        // stands out while it lasts.
        void hover(std::optional<QPointF> position) {
            if (view->hasGesture()) {
                return;
            }
            int note = -1;
            if (position && pointsShown()) {
                if (const auto hit = view->hitAt(*position); hit && hit->part == PitchPoint) {
                    note = indexOf(hit->node);
                } else if (const auto near = portamentoNear(*position)) {
                    note = near->first;
                }
            }
            if (note != hovered) {
                hovered = note;
                view->viewport()->update();
            }
        }

        // Selects the points ids; selecting a point clears the selected notes.
        void selectPoints(const QSet<kit::edit::NodeId> &ids) {
            stdc_decl_t;
            const bool clearsNotes = !ids.isEmpty() && !selection.isEmpty();
            if (ids == selectedPoints && !clearsNotes) {
                return;
            }
            selectedPoints = ids;
            if (clearsNotes) {
                selection.clear();
            }
            view->viewport()->update();
            Q_EMIT decl.selectionChanged();
        }

        double ticksOf(double milliseconds, int index) const {
            return milliseconds * timeline->tempoMap().tempo(index) * kit::ticksPerQuarter / 60000;
        }

        double millisecondsOf(double ticks, int index) const {
            return ticks * 60000 / (timeline->tempoMap().tempo(index) * kit::ticksPerQuarter);
        }

        // Whether the first point of note index starts at the pitch of the previous note, as
        // the resampler curve has it (kit::PitchCurve)
        bool startsAtPrevious(int index) const {
            return index > 0 && !timeline->note(index - 1).rest;
        }

        // Whether the height of point j of the count points of note index stays as it is: the
        // first where it starts at the previous note, and the last
        bool heightFixed(int index, int j, int count) const {
            return (j == 0 && startsAtPrevious(index)) || (count >= 2 && j == count - 1);
        }

        // The points of note index, as a gesture shows them if it does
        QList<kit::PortamentoPoint> pointsOf(int index) const {
            if (const auto it = pointPreview.find(index); it != pointPreview.end()) {
                return *it;
            }
            const auto list = notes().at(index).portamento();
            QList<kit::PortamentoPoint> points;
            for (int j = 0; j < list.size(); ++j) {
                const auto ref = list.at(j);
                kit::PortamentoPoint point;
                point.x = ref.x();
                point.y = ref.y();
                point.type = ref.type();
                points.push_back(point);
            }
            return points;
        }

        // Where point j of note index is drawn: at the pitch of the previous note if it starts
        // there
        QPointF positionOf(int index, int j, const kit::PortamentoPoint &point) const {
            const auto &note = timeline->note(index);
            const double cents = j == 0 && startsAtPrevious(index)
                                     ? (timeline->note(index - 1).key - note.key) * 100.0
                                     : point.y;
            return {view->timeAxis().toX(double(note.start) + ticksOf(point.x, index)),
                    view->keyAxis().toY(note.key + 0.5 + cents / 100)};
        }

        // Shows every value of the lane in the height of the parameter area
        void fitParameters() {
            const auto range = rangeOf(lane);
            auto axis = parameters->keyAxis();
            const double height = parameters->viewport()->height();
            axis.pixelsPerKey =
                std::max(0.01, (height - 2 * ParameterMargin) / (range.maximum - range.minimum));
            axis.top = range.maximum + ParameterMargin / axis.pixelsPerKey;
            parameters->setKeyAxis(axis);
        }

        // The keys that a lane spans, and the value it draws a line at: that of UTAU where a
        // note gives none
        struct LaneRange {
            double minimum;
            double maximum;
            double fallback;
        };

        // Where a value of the lane is drawn, in the keys of the parameter area, which span
        // the minimum and maximum of rangeOf(). Velocity from VelocityMinimum to the default
        // occupies the keys from 0 to the default, and the values above it their own keys
        // (QSynthesis).
        static double keyOf(Lane lane, double value) {
            if (lane != VelocityLane || value >= utau::DEFAULT_VALUE_VELOCITY) {
                return value;
            }
            return (value - VelocityMinimum) * utau::DEFAULT_VALUE_VELOCITY /
                   (utau::DEFAULT_VALUE_VELOCITY - VelocityMinimum);
        }

        // The value of the lane drawn at key, the inverse of keyOf()
        static double valueAt(Lane lane, double key) {
            if (lane != VelocityLane || key >= utau::DEFAULT_VALUE_VELOCITY) {
                return key;
            }
            return VelocityMinimum + key * (utau::DEFAULT_VALUE_VELOCITY - VelocityMinimum) /
                                         utau::DEFAULT_VALUE_VELOCITY;
        }

        // The keys that the lane spans, see keyOf()
        static LaneRange rangeOf(Lane lane) {
            switch (lane) {
                case IntensityLane:
                    return {0, IntensityRange, utau::DEFAULT_VALUE_INTENSITY};
                case ModulationLane:
                    return {-ModulationRange, ModulationRange, utau::DEFAULT_VALUE_MODULATION};
                case VelocityLane:
                    return {0, VelocityRange, utau::DEFAULT_VALUE_VELOCITY};
                default:
                    return {0, EnvelopeRange, 100};
            }
        }

        static kit::ProjectEdits::NoteParameter parameterOf(Lane lane) {
            switch (lane) {
                case ModulationLane:
                    return kit::ProjectEdits::Modulation;
                case VelocityLane:
                    return kit::ProjectEdits::Velocity;
                default:
                    return kit::ProjectEdits::Intensity;
            }
        }

        // The value of the lane that note index gives, or none
        std::optional<double> storedValueOf(int index) const {
            const auto note = notes().at(index);
            switch (lane) {
                case ModulationLane:
                    return note.modulation();
                case VelocityLane:
                    return note.velocity();
                default:
                    return note.intensity();
            }
        }

        // The value of the lane for note index, as a gesture shows it if it does, or that of
        // UTAU
        double valueOf(int index) const {
            if (const auto it = valuePreview.find(index); it != valuePreview.end()) {
                return *it;
            }
            return storedValueOf(index).value_or(rangeOf(lane).fallback);
        }

        // The sung notes that an edit of the value of note index changes: the selected ones if
        // it is selected, or else itself
        QList<int> valueTargets(int index) const {
            if (!isSelected(index)) {
                return {index};
            }
            QList<int> targets;
            for (int i = 0; i < timeline->noteCount(); ++i) {
                if (isSelected(i) && !timeline->note(i).rest) {
                    targets.push_back(i);
                }
            }
            return targets;
        }

        // Sets the value of the lane of the notes indices, or removes it
        void writeValue(const QList<int> &indices, std::optional<double> value) {
            const auto refs = notes();
            QList<kit::NoteRef> changed;
            for (const int i : indices) {
                if (storedValueOf(i) != value) {
                    changed.push_back(refs.at(i));
                }
            }
            if (changed.isEmpty()) {
                return;
            }
            kit::DiagnosticList diagnostics;
            kit::ProjectEdits::setParameter(changed, parameterOf(lane), value, diagnostics);
            report(diagnostics);
        }

        // The timing of the sample of every note, with the voice bank if there is one
        const QList<kit::SampleTiming> &sampleTimings() {
            if (timingsStale) {
                const auto refs = notes();
                QList<kit::Note> all;
                for (int i = 0; i < timeline->noteCount(); ++i) {
                    all.push_back(refs.at(i).toNote());
                }
                timings = kit::SampleTiming::of(all, timeline->tempoMap(), voiceBank.get());
                timingsStale = false;
            }
            return timings;
        }

        // The envelope of note index, as a gesture shows it if it does, or that of UTAU
        kit::Envelope envelopeOf(int index) const {
            if (const auto it = envelopePreview.find(index); it != envelopePreview.end()) {
                return *it;
            }
            return notes().at(index).envelope().value_or(defaultEnvelope());
        }

        // Where the fragment of note index lies in the track: its start in milliseconds, and
        // its length as the wavtool appends it
        std::pair<double, double> fragmentOf(int index) {
            const auto &all = sampleTimings();
            const auto &map = timeline->tempoMap();
            const double duration = timeline->note(index).length * 125.0 / map.tempo(index);
            double length = duration + all[index].preUtterance;
            if (index + 1 < all.size()) {
                length += all[index + 1].voiceOverlap - all[index + 1].preUtterance;
            }
            return {map.startTime(index) - all[index].preUtterance, length};
        }

        // The anchors of envelope in time order, in milliseconds from the start of a fragment
        // of length: p1, p2 and p5 count forward from the start, p3 and p4 back from the end,
        // as the wavtool places them (WavtoolMixer::layOut)
        static QList<double> anchorTimes(const kit::Envelope &envelope, double length) {
            const auto &a = envelope.anchors;
            QList<double> times{a[0].x, a[0].x + a[1].x};
            if (envelope.hasMiddle) {
                times.push_back(times.last() + a[2].x);
            }
            times.push_back(length - a[4].x - a[3].x);
            times.push_back(length - a[4].x);
            return times;
        }

        // Where the parameter area draws the volume at milliseconds into the fragment of note
        // index
        QPointF envelopePointOf(int index, double milliseconds, double volume) {
            const double start = fragmentOf(index).first;
            return {parameters->timeAxis().toX(timeline->tempoMap().tickOf(start + milliseconds)),
                    parameters->keyAxis().toY(volume)};
        }

        // The vibrato of note index, as a gesture shows it if it does
        std::optional<kit::Vibrato> vibratoOf(int index) const {
            if (const auto it = vibratoPreview.find(index); it != vibratoPreview.end()) {
                return *it;
            }
            return notes().at(index).vibrato();
        }

        // Where the handles of a vibrato are drawn: its trapezoid, from the start of the
        // vibrato up to the end of its fade-in, along its top to the start of its fade-out, and
        // down to the end of the note; and the box of one period from its phase on
        struct VibratoShape {
            QPointF start;
            QPointF fadeIn;
            QPointF fadeOut;
            QPointF end;
            QRectF period;
        };

        std::optional<VibratoShape> vibratoShapeOf(int index) const {
            const auto &note = timeline->note(index);
            const auto vibrato = vibratoOf(index);
            if (note.rest || !vibrato || vibrato->length <= 0) {
                return std::nullopt;
            }
            const auto &time = view->timeAxis();
            const auto &keys = view->keyAxis();
            const double span = vibrato->length / 100 * note.length;
            const double end = double(note.start + note.length);
            const double start = end - span;
            const double base = keys.toY(note.key + 0.5 - VibratoBaseline);
            const double top = keys.toY(note.key + 0.5 - VibratoBaseline +
                                        vibrato->amplitude / VibratoCentsPerRow);
            const double period = ticksOf(vibrato->period, index);
            const double from = start + vibrato->phase / 100 * period;

            VibratoShape shape;
            shape.start = {time.toX(start), base};
            shape.fadeIn = {time.toX(start + vibrato->attack / 100 * span), top};
            shape.fadeOut = {time.toX(end - vibrato->release / 100 * span), top};
            shape.end = {time.toX(end), base};
            shape.period =
                QRectF(QPointF(time.toX(from), base),
                       QPointF(time.toX(from + period),
                               keys.toY(note.key + 0.5 - VibratoBaseline - VibratoBoxHeight)));
            return shape;
        }

        // Reports why an edit made in the roll was refused, if it was
        void report(const kit::DiagnosticList &diagnostics) {
            stdc_decl_t;
            QStringList messages;
            for (const auto &diagnostic : diagnostics) {
                if (diagnostic.severity == kit::DiagnosticSeverity::Error) {
                    messages.push_back(diagnostic.message);
                }
            }
            if (!messages.isEmpty()) {
                Q_EMIT decl.editRefused(messages.join(u' '));
            }
        }

        // An edit of the points of a note ends them at the pitch of the note, as UTAU draws
        // them (see step 2 in docs/Tuning.md).
        static void endAtPitch(QList<kit::PortamentoPoint> &points) {
            if (!points.isEmpty()) {
                points.last().y = 0;
            }
        }

        // Writes the points of several notes, by index, in one step, each ending at the pitch
        // of its note
        bool writePoints(const QString &message,
                         const QHash<int, QList<kit::PortamentoPoint>> &points,
                         kit::DiagnosticList &diagnostics) {
            auto transaction = session->transaction(message);
            const auto refs = notes();
            for (auto it = points.begin(); it != points.end(); ++it) {
                auto written = it.value();
                endAtPitch(written);
                kit::ProjectEdits::setPortamento(refs.at(it.key()), written, diagnostics);
            }
            return transaction.commit(diagnostics);
        }

        // The selected points, by note index and the indices of the points in the note
        QHash<int, QSet<int>> selectedPointIndices() const {
            QHash<int, QSet<int>> result;
            if (selectedPoints.isEmpty()) {
                return result;
            }
            const auto refs = notes();
            for (int i = 0; i < timeline->noteCount(); ++i) {
                const auto list = refs.at(i).portamento();
                for (int j = 0; j < list.size(); ++j) {
                    if (selectedPoints.contains(list.at(j).id())) {
                        result[i].insert(j);
                    }
                }
            }
            return result;
        }

        // Removes the points of notes, by note index; each note keeps its first and last point
        // where fewer than two would remain.
        bool removePoints(const QHash<int, QSet<int>> &removed, kit::DiagnosticList &diagnostics) {
            QHash<int, QList<kit::PortamentoPoint>> result;
            for (auto it = removed.begin(); it != removed.end(); ++it) {
                const auto points = pointsOf(it.key());
                auto gone = it.value();
                if (points.size() - gone.size() < 2) {
                    gone.remove(0);
                    gone.remove(int(points.size()) - 1);
                }
                QList<kit::PortamentoPoint> kept;
                for (int j = 0; j < points.size(); ++j) {
                    if (!gone.contains(j)) {
                        kept.push_back(points[j]);
                    }
                }
                if (kept.size() != points.size()) {
                    result.insert(it.key(), kept);
                }
            }
            if (result.isEmpty()) {
                return true;
            }
            selectPoints({});
            return writePoints(PianoRoll::tr("Delete Pitch Points"), result, diagnostics);
        }

        // The note whose portamento lies within curveGrip of position, and the tick there from
        // the start of the note. The note is the one at that time, or the next one from its first
        // point on.
        std::optional<std::pair<int, double>> portamentoNear(QPointF position) const;

        // Inserts a point where position lies on the portamento of a note, and selects it.
        bool insertPointAt(QPointF position);

        void selectOnly(int index) {
            anchor = timeline->note(index).id;
            setSelection({anchor});
        }

        void selectRange(int first, int last) {
            QSet<kit::edit::NodeId> ids;
            for (int i = std::min(first, last); i <= std::max(first, last); ++i) {
                ids.insert(timeline->note(i).id);
            }
            setSelection(ids);
        }

        bool snaps(Qt::KeyboardModifiers modifiers) const {
            return quantization > 0 && !(modifiers & Qt::AltModifier);
        }

        // The nearest grid line to tick, or tick itself without snapping
        qint64 snapped(double tick, Qt::KeyboardModifiers modifiers) const {
            if (!snaps(modifiers)) {
                return qint64(std::llround(tick));
            }
            return qint64(std::llround(tick / quantization)) * quantization;
        }

        // The last grid line at or before tick
        qint64 snappedDown(double tick, Qt::KeyboardModifiers modifiers) const {
            if (!snaps(modifiers)) {
                return qint64(std::floor(tick));
            }
            return qint64(std::floor(tick / quantization)) * quantization;
        }

        QRectF rectOf(qint64 start, int length, int key) const {
            const auto &time = view->timeAxis();
            const auto &keys = view->keyAxis();
            return {time.toX(double(start)), keys.toY(key + 1), length * time.pixelsPerTick,
                    keys.pixelsPerKey};
        }

        // The notes in the order of indices, each where the one before ends, with lengths and
        // keys changed as given
        QList<Placement> layOut(const QList<int> &order, const QHash<int, int> &lengths = {},
                                const QSet<int> &transposed = {}, int semitones = 0) const {
            QList<Placement> result;
            result.reserve(order.size());
            qint64 start = 0;
            for (const int index : order) {
                const auto &note = timeline->note(index);
                const int length = lengths.value(index, note.length);
                result.push_back({index, start, length,
                                  note.key + (transposed.contains(index) ? semitones : 0)});
                start += length;
            }
            return result;
        }

        QList<int> identityOrder() const {
            QList<int> order(timeline->noteCount());
            std::iota(order.begin(), order.end(), 0);
            return order;
        }

        void clearPreview() {
            placements.clear();
            drawn.reset();
            band.reset();
            view->viewport()->update();
        }

        // Updates what depends on the notes as a whole, once control returns to the event
        // loop, so that a transaction of many changes updates it once.
        void scheduleRefresh() {
            stdc_decl_t;
            if (refreshPending) {
                return;
            }
            refreshPending = true;
            QTimer::singleShot(0, &decl, [this] { refresh(); });
        }

        void refresh() {
            refreshPending = false;
            const auto bars =
                std::max<qint64>(MinimumBars, timeline->length() / BarTicks + 1 + TrailingBars);
            view->setTickRange(0, double(bars * BarTicks));
            parameters->setTickRange(0, double(bars * BarTicks));

            // The tempo at the start, and wherever a note sets one
            const auto &map = timeline->tempoMap();
            QList<TimelineRuler::Mark> marks;
            for (int i = 0; i < timeline->noteCount(); ++i) {
                const auto &note = timeline->note(i);
                if (i == 0 || note.tempo) {
                    marks.push_back({double(note.start), tempoText(map.tempo(i))});
                }
            }
            ruler->setMarks(marks);
            view->viewport()->update();
        }

        // Scrolls so that note index is in view, if it is not.
        void ensureVisible(int index) {
            const auto &note = timeline->note(index);
            const auto rect = rectOf(note.start, note.length, note.key);
            const auto area = QRectF(view->viewport()->rect());
            if (rect.left() < 0 || rect.left() >= area.right()) {
                auto time = view->timeAxis();
                time.left = double(note.start) - area.width() / time.pixelsPerTick / 4;
                view->setTimeAxis(time);
            }
            if (rect.top() < 0 || rect.bottom() > area.bottom()) {
                auto keys = view->keyAxis();
                keys.top = note.key + 0.5 + area.height() / keys.pixelsPerKey / 2;
                view->setKeyAxis(keys);
            }
        }

        void startEditing(int index) {
            finishEditing(true);
            if (index < 0 || index >= timeline->noteCount()) {
                return;
            }
            selectOnly(index);
            ensureVisible(index);

            const auto &note = timeline->note(index);
            editing = note.id;
            const auto rect = rectOf(note.start, note.length, note.key);
            const int height = std::max(editor->sizeHint().height(), int(rect.height()));
            editor->setGeometry(int(rect.left()), int(rect.center().y() - height / 2.0),
                                std::max(MinimumEditorWidth, int(rect.width())), height);
            editor->setText(note.lyric);
            editor->selectAll();
            editor->show();
            editor->setFocus(Qt::OtherFocusReason);
        }

        // Ends the editing of a lyric, writing it if commit is true and it changed.
        void finishEditing(bool commit) {
            if (!editing) {
                return;
            }
            const auto id = editing;
            editing = 0;
            const auto text = editor->text();
            editor->hide();
            view->setFocus(Qt::OtherFocusReason);

            const int index = indexOf(id);
            if (!commit || index < 0 || timeline->note(index).lyric == text) {
                return;
            }
            auto transaction = session->transaction(PianoRoll::tr("Change Lyric"));
            notes().at(index).setLyric(text);
            kit::DiagnosticList diagnostics;
            transaction.commit(diagnostics);
            report(diagnostics);
        }

        void editNext(bool forward) {
            const int index = indexOf(editing);
            finishEditing(true);
            if (index >= 0) {
                startEditing(forward ? index + 1 : index - 1);
            }
        }
    };

    // The rows of the keys and the lines of the bars and beats, and the background that a press
    // selects in or draws on
    class PianoRoll::Impl::GridLayer : public SceneLayer {
    public:
        explicit GridLayer(PianoRoll::Impl *roll) : m_roll(roll) {
        }

        void paint(QPainter &painter, const QRect &exposed) override {
            const auto &keys = view()->keyAxis();
            const auto &time = view()->timeAxis();
            const auto decl = m_roll->_decl;

            painter.fillRect(exposed, decl->whiteRowColor());
            const int highest = keys.keyAt(exposed.top());
            const int lowest = keys.keyAt(exposed.bottom());
            painter.setPen(decl->lineColor());
            for (int key = lowest; key <= highest; ++key) {
                const QRectF row(exposed.left(), keys.toY(key + 1), exposed.width(),
                                 keys.pixelsPerKey);
                if (PianoKeyboard::isBlackKey(key)) {
                    painter.fillRect(row, decl->blackRowColor());
                }
                painter.drawLine(QPointF(exposed.left(), row.bottom()),
                                 QPointF(exposed.right(), row.bottom()));
            }

            const double firstTick = std::max(0.0, time.toTick(exposed.left()));
            const double lastTick = time.toTick(exposed.right() + 1);
            const double beatWidth = kit::ticksPerQuarter * time.pixelsPerTick;
            const auto firstBeat = qint64(std::floor(firstTick / kit::ticksPerQuarter));
            const auto lastBeat = qint64(std::ceil(lastTick / kit::ticksPerQuarter));
            for (auto beat = firstBeat; beat <= lastBeat; ++beat) {
                const bool bar = beat % BeatsPerBar == 0;
                if (!bar && beatWidth < MinimumBeatSpacing) {
                    continue;
                }
                painter.setPen(bar ? decl->barLineColor() : decl->lineColor());
                const double x = time.toX(double(beat) * kit::ticksPerQuarter);
                painter.drawLine(QPointF(x, exposed.top()), QPointF(x, exposed.bottom() + 1));
            }
        }

        std::optional<SceneHit> hitTest(QPointF position) const override {
            SceneHit hit;
            hit.part = Background;
            Q_UNUSED(position);
            if (m_roll->tool == PenTool || m_roll->drawsBend(Qt::LeftButton)) {
                hit.cursor = Qt::CrossCursor;
            }
            return hit;
        }

        std::unique_ptr<SceneGesture> press(const SceneHit &hit, QPointF position,
                                            Qt::MouseButton button,
                                            Qt::KeyboardModifiers modifiers) override;

        // On the portamento a double click inserts a point.
        bool doubleClick(const SceneHit &hit, QPointF position) override {
            Q_UNUSED(hit);
            return m_roll->insertPointAt(position);
        }

    private:
        PianoRoll::Impl *m_roll;
    };

    // The notes, each a bar in the row of its key with its lyric
    class PianoRoll::Impl::NoteLayer : public SceneLayer {
    public:
        explicit NoteLayer(PianoRoll::Impl *roll) : m_roll(roll) {
        }

        void paint(QPainter &painter, const QRect &exposed) override {
            const auto timeline = m_roll->timeline;
            painter.setRenderHint(QPainter::Antialiasing);
            if (m_roll->placements.isEmpty()) {
                const auto &time = view()->timeAxis();
                const auto [begin, end] = timeline->notesBetween(time.toTick(exposed.left()),
                                                                 time.toTick(exposed.right() + 1));
                for (int i = begin; i < end; ++i) {
                    const auto &note = timeline->note(i);
                    paintNote(painter, exposed, {i, note.start, note.length, note.key});
                }
            } else {
                for (const auto &placement : std::as_const(m_roll->placements)) {
                    paintNote(painter, exposed, placement);
                }
            }
            if (m_roll->drawn) {
                paintNote(painter, exposed, *m_roll->drawn);
            }
        }

        std::optional<SceneHit> hitTest(QPointF position) const override {
            const auto timeline = m_roll->timeline;
            const int index = timeline->noteAt(view()->timeAxis().toTick(position.x()));
            if (index < 0 || index >= timeline->noteCount()) {
                return std::nullopt;
            }
            const auto &note = timeline->note(index);
            const auto rect = m_roll->rectOf(note.start, note.length, note.key);
            if (!rect.contains(position)) {
                return std::nullopt;
            }
            SceneHit hit;
            hit.node = note.id;
            hit.part = NoteBody;
            if (m_roll->drawsBend(Qt::LeftButton)) {
                hit.cursor = Qt::CrossCursor;
            } else if (rect.width() >= MinimumGripWidth && position.x() >= rect.right() - EndGrip) {
                hit.part = NoteEnd;
                hit.cursor = Qt::SizeHorCursor;
            }
            return hit;
        }

        std::unique_ptr<SceneGesture> press(const SceneHit &hit, QPointF position,
                                            Qt::MouseButton button,
                                            Qt::KeyboardModifiers modifiers) override;

        bool doubleClick(const SceneHit &hit, QPointF position) override {
            // The pitch tool only draws.
            if (m_roll->drawsBend(Qt::LeftButton)) {
                return true;
            }
            // On the portamento a double click inserts a point.
            if (m_roll->insertPointAt(position)) {
                return true;
            }
            const int index = m_roll->indexOf(hit.node);
            if (index < 0) {
                return false;
            }
            m_roll->startEditing(index);
            return true;
        }

    private:
        PianoRoll::Impl *m_roll;

        void paintNote(QPainter &painter, const QRect &exposed, const Placement &placement) {
            const auto decl = m_roll->_decl;
            const auto rect = m_roll->rectOf(placement.start, placement.length, placement.key);
            if (!rect.intersects(exposed)) {
                return;
            }
            // A note being drawn has no index, and is a sung note with the default lyric.
            const bool drawn = placement.index < 0;
            const auto *note = drawn ? nullptr : &m_roll->timeline->note(placement.index);
            const bool rest = note && note->rest;
            const bool unsampled = note && decl->lacksSample(placement.index);
            const bool selected = drawn || m_roll->selection.contains(note->id);

            if (unsampled) {
                painter.setPen(QPen(decl->unsampledColor(), 1));
                painter.setBrush(Qt::NoBrush);
            } else {
                painter.setPen(Qt::NoPen);
                painter.setBrush(rest ? decl->restColor() : decl->noteColor());
            }
            const auto body = rect.adjusted(0.5, 0.5, -0.5, -0.5);
            painter.drawRoundedRect(body, NoteRadius, NoteRadius);
            if (selected) {
                painter.setPen(QPen(decl->selectionColor(), 2));
                painter.setBrush(Qt::NoBrush);
                painter.drawRoundedRect(body.adjusted(0.5, 0.5, -0.5, -0.5), NoteRadius,
                                        NoteRadius);
            }

            // The lyric under the editor is not drawn.
            if (note && note->id == m_roll->editing) {
                return;
            }
            painter.setPen(unsampled ? decl->unsampledLyricColor() : decl->lyricColor());
            painter.drawText(rect.adjusted(LyricPadding, 0, -LyricPadding, 0),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             drawn ? QString::fromLatin1(kit::defaultLyric) : note->lyric);
        }
    };

    // The pitch of each sung note, as the resampler receives it: the portamento as a line through
    // the rows, and apart from it the vibrato around the middle of the row of the note
    class PianoRoll::Impl::PitchLayer : public SceneLayer {
    public:
        explicit PitchLayer(PianoRoll::Impl *roll) : m_roll(roll) {
        }

        void paint(QPainter &painter, const QRect &exposed) override {
            // While notes are dragged, their curves are not yet known.
            if (!m_roll->pitchVisible || !m_roll->placements.isEmpty()) {
                return;
            }
            const auto timeline = m_roll->timeline;
            const auto &time = view()->timeAxis();
            const auto &keys = view()->keyAxis();
            const double left = time.toTick(exposed.left());
            const double right = time.toTick(exposed.right() + 1);
            const auto [begin, end] = timeline->notesBetween(left, right);
            if (begin >= end) {
                return;
            }

            // The curve of a note reads the two notes before it and the one after.
            const int first = std::max(0, begin - 2);
            const int last = std::min(timeline->noteCount(), end + 1);
            const auto refs = m_roll->notes();
            QList<kit::Note> notes;
            for (int i = first; i < last; ++i) {
                notes.push_back(refs.at(i).toNote());
                if (const auto it = m_roll->pointPreview.find(i);
                    it != m_roll->pointPreview.end()) {
                    notes.last().portamento = *it;
                }
                if (const auto it = m_roll->vibratoPreview.find(i);
                    it != m_roll->vibratoPreview.end()) {
                    notes.last().vibrato = *it;
                }
                if (const auto it = m_roll->bendPreview.find(i); it != m_roll->bendPreview.end()) {
                    notes.last().pitchBend = *it;
                }
            }
            if (m_roll->mode1()) {
                paintBends(painter, exposed, notes, first, begin, end);
                return;
            }

            const auto decl = m_roll->_decl;
            const QPen portamentoPen(decl->pitchColor(), 1.5);
            const QPen vibratoPen(decl->vibratoColor(), 1);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setBrush(Qt::NoBrush);
            const double step = std::max(1.0, CurveStep / time.pixelsPerTick);
            for (int i = begin; i < end; ++i) {
                const auto &entry = timeline->note(i);
                if (entry.rest) {
                    continue;
                }
                const auto &note = notes.at(i - first);
                const double tempo = timeline->tempoMap().tempo(i);
                const kit::PitchCurve curve(notes, i - first, tempo);

                // Each note draws its own span, which its neighbours also bend. The span before
                // it belongs to the previous note, unless that is a rest.
                double from = 0;
                if ((i == 0 || timeline->note(i - 1).rest) && !note.portamento.isEmpty()) {
                    from = std::min(0.0, note.portamento.first().x * tempo * kit::ticksPerQuarter /
                                             60000);
                }
                from = std::max(from, left - double(entry.start) - step);
                const double to =
                    std::min(double(entry.length), right - double(entry.start) + step);
                if (from >= to) {
                    continue;
                }

                const auto pointAt = [&](double tick, double cents) {
                    return QPointF(time.toX(double(entry.start) + tick),
                                   keys.toY(entry.key + 0.5 + cents / 100));
                };
                QPolygonF portamento;
                QList<QPolygonF> vibrato;
                bool vibrating = false;
                for (double tick = from;; tick = std::min(tick + step, to)) {
                    portamento.push_back(pointAt(tick, curve.portamentoAt(tick)));
                    const double v = curve.vibratoAt(tick);
                    if (v != 0) {
                        if (!vibrating) {
                            vibrato.push_back({});
                        }
                        vibrato.last().push_back(pointAt(tick, v));
                    }
                    vibrating = v != 0;
                    if (tick >= to) {
                        break;
                    }
                }
                painter.setPen(vibratoPen);
                for (const auto &run : std::as_const(vibrato)) {
                    painter.drawPolyline(run);
                }
                painter.setPen(portamentoPen);
                painter.drawPolyline(portamento);
            }

            // The points, also those of the next note, which may lie before it. Those of the note
            // whose portamento is under the pointer are plain: filled where they only move in
            // time, and in the selection color where selected. The others are small rings in
            // faintPointColor, selected ones in the selection color, through which the
            // portamento shows.
            for (int i = begin; i < last; ++i) {
                if (timeline->note(i).rest) {
                    continue;
                }
                const bool faint = i != m_roll->hovered;
                const auto &points = notes.at(i - first).portamento;
                const auto list = refs.at(i).portamento();
                for (int j = 0; j < points.size(); ++j) {
                    const auto center = m_roll->positionOf(i, j, points[j]);
                    const bool selected =
                        j < list.size() && m_roll->selectedPoints.contains(list.at(j).id());
                    if (faint) {
                        painter.setPen(
                            QPen(selected ? decl->selectionColor() : decl->faintPointColor(), 1));
                        painter.setBrush(Qt::NoBrush);
                        painter.drawEllipse(center, FaintPointRadius, FaintPointRadius);
                        continue;
                    }
                    const bool fixed = m_roll->heightFixed(i, j, int(points.size()));
                    painter.setPen(
                        QPen(selected ? decl->selectionColor() : decl->pitchColor(), 1.5));
                    painter.setBrush(selected
                                         ? decl->selectionColor()
                                         : (fixed ? decl->pitchColor() : decl->whiteRowColor()));
                    painter.drawEllipse(center, PointRadius, PointRadius);
                }
            }

            // The handles of the vibratos: the trapezoid with its top edge stressed, and the
            // box of one period
            for (int i = begin; i < end; ++i) {
                const auto shape = m_roll->vibratoShapeOf(i);
                if (!shape) {
                    continue;
                }
                auto boxColor = decl->vibratoColor();
                painter.setPen(QPen(boxColor, 1, Qt::DashLine));
                boxColor.setAlphaF(boxColor.alphaF() * 0.2f);
                painter.setBrush(boxColor);
                painter.drawRect(shape->period);
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(decl->vibratoColor(), 1));
                painter.drawPolyline(
                    QPolygonF{shape->start, shape->fadeIn, shape->fadeOut, shape->end});
                painter.setPen(QPen(decl->vibratoColor(), 2.5));
                painter.drawLine(shape->fadeIn, shape->fadeOut);
                painter.setPen(QPen(decl->vibratoColor(), 1.5));
                painter.setBrush(decl->whiteRowColor());
                for (const auto &handle : {shape->start, shape->fadeIn, shape->fadeOut}) {
                    painter.drawEllipse(handle, PointRadius, PointRadius);
                }
                painter.setBrush(Qt::NoBrush);
            }
        }

        // The handle of a vibrato at position, if any: its points first, then its top edge,
        // the right edge of its period box and the inside of the box
        std::optional<SceneHit> vibratoHitAt(QPointF position, int begin, int end) const {
            const auto grip = m_roll->pointGrip;
            const auto near = [grip, position](QPointF handle) {
                return std::hypot(handle.x() - position.x(), handle.y() - position.y()) <= grip;
            };
            for (int i = begin; i < end; ++i) {
                const auto shape = m_roll->vibratoShapeOf(i);
                if (!shape) {
                    continue;
                }
                SceneHit hit;
                hit.node = m_roll->timeline->note(i).id;
                hit.cursor = Qt::SizeHorCursor;
                const auto period = shape->period;
                const double left = std::min(shape->fadeIn.x(), shape->fadeOut.x());
                const double right = std::max(shape->fadeIn.x(), shape->fadeOut.x());
                if (near(shape->start)) {
                    hit.part = VibratoStart;
                } else if (near(shape->fadeIn)) {
                    hit.part = VibratoFadeIn;
                } else if (near(shape->fadeOut)) {
                    hit.part = VibratoFadeOut;
                } else if (position.x() >= left && position.x() <= right &&
                           std::abs(position.y() - shape->fadeIn.y()) <= grip) {
                    hit.part = VibratoDepth;
                    hit.cursor = Qt::SizeVerCursor;
                } else if (std::abs(position.x() - period.right()) <= grip &&
                           position.y() >= period.top() - grip &&
                           position.y() <= period.bottom() + grip) {
                    hit.part = VibratoPeriod;
                } else if (period.contains(position)) {
                    hit.part = VibratoPhase;
                } else {
                    continue;
                }
                return hit;
            }
            return std::nullopt;
        }

        std::optional<SceneHit> hitTest(QPointF position) const override {
            if (!m_roll->pointsShown() || !m_roll->placements.isEmpty()) {
                return std::nullopt;
            }
            // The notes around the position, and the next one, whose points may lie before it
            const auto timeline = m_roll->timeline;
            const auto &time = view()->timeAxis();
            const double grip = m_roll->pointGrip / time.pixelsPerTick;
            auto [begin, end] = timeline->notesBetween(time.toTick(position.x()) - grip,
                                                       time.toTick(position.x()) + grip);
            begin = std::max(0, begin - 1);
            end = std::min(timeline->noteCount(), end + 1);

            std::optional<SceneHit> nearest;
            double distance = m_roll->pointGrip;
            for (int i = begin; i < end; ++i) {
                if (timeline->note(i).rest) {
                    continue;
                }
                const auto points = m_roll->pointsOf(i);
                for (int j = 0; j < points.size(); ++j) {
                    const auto offset = m_roll->positionOf(i, j, points[j]) - position;
                    const double d = std::hypot(offset.x(), offset.y());
                    if (d <= distance) {
                        distance = d;
                        SceneHit hit;
                        hit.node = timeline->note(i).id;
                        hit.part = PitchPoint;
                        hit.index = j;
                        hit.cursor = m_roll->heightFixed(i, j, int(points.size()))
                                         ? Qt::SizeHorCursor
                                         : Qt::SizeAllCursor;
                        nearest = hit;
                    }
                }
            }
            if (nearest) {
                return nearest;
            }
            // A period box may reach past the end of its note.
            return vibratoHitAt(position, std::max(0, begin - 1), end);
        }

        std::unique_ptr<SceneGesture> press(const SceneHit &hit, QPointF position,
                                            Qt::MouseButton button,
                                            Qt::KeyboardModifiers modifiers) override;

    private:
        PianoRoll::Impl *m_roll;

        // The Mode1 pitch of the sung notes from begin to end, of which notes holds those from
        // first on, drawn as the portamento is: each note over its own span, and a note after
        // a rest or none from its first value on
        void paintBends(QPainter &painter, const QRect &exposed, const QList<kit::Note> &notes,
                        int first, int begin, int end) {
            const auto timeline = m_roll->timeline;
            const auto &time = view()->timeAxis();
            const auto &keys = view()->keyAxis();
            const double left = time.toTick(exposed.left());
            const double right = time.toTick(exposed.right() + 1);
            const double step = std::max(1.0, CurveStep / time.pixelsPerTick);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(m_roll->_decl->pitchColor(), 1.5));
            for (int i = begin; i < end; ++i) {
                const auto &entry = timeline->note(i);
                if (entry.rest) {
                    continue;
                }
                const auto &bend = notes.at(i - first).pitchBend;
                kit::PreviousBend previous;
                if (i > 0) {
                    previous = kit::PreviousBend::of(notes.at(i - 1 - first), notes.at(i - first));
                }
                const double tempo = timeline->tempoMap().tempo(i);

                double from = 0;
                if ((i == 0 || timeline->note(i - 1).rest) && bend && !bend->values.isEmpty()) {
                    from = std::min(0.0, m_roll->ticksOf(bend->start.value_or(0), i));
                }
                from = std::max(from, left - double(entry.start) - step);
                const double to =
                    std::min(double(entry.length), right - double(entry.start) + step);
                if (from >= to) {
                    continue;
                }
                QPolygonF line;
                for (double tick = from;; tick = std::min(tick + step, to)) {
                    const double cents = kit::PitchBend::curveAt(bend, previous, tick, tempo);
                    line.push_back(QPointF(time.toX(double(entry.start) + tick),
                                           keys.toY(entry.key + 0.5 + cents / 100)));
                    if (tick >= to) {
                        break;
                    }
                }
                painter.drawPolyline(line);
            }
        }

        // The context menu of point j of note index: its shape, and its removal
        void showMenu(int index, int j, QPointF position) {
            const auto points = m_roll->pointsOf(index);
            QMenu menu(view());
            const std::pair<kit::PortamentoPoint::Type, const char *> shapes[] = {
                {kit::PortamentoPoint::S,      QT_TRANSLATE_NOOP("hello::daw::PianoRoll", "S-Curve")},
                {kit::PortamentoPoint::Linear,
                 QT_TRANSLATE_NOOP("hello::daw::PianoRoll",                               "Linear") },
                {kit::PortamentoPoint::R,      QT_TRANSLATE_NOOP("hello::daw::PianoRoll", "R-Curve")},
                {kit::PortamentoPoint::J,      QT_TRANSLATE_NOOP("hello::daw::PianoRoll", "J-Curve")},
            };
            for (const auto &[type, name] : shapes) {
                const auto action = menu.addAction(PianoRoll::tr(name));
                action->setCheckable(true);
                action->setChecked(points[j].type == type);
                // The shape is that of the segment that ends at a point, which the first lacks.
                action->setEnabled(j > 0);
                QObject::connect(action, &QAction::triggered, view(),
                                 [this, index, j, type = type] {
                                     auto changed = m_roll->pointsOf(index);
                                     if (j >= changed.size() || changed[j].type == type) {
                                         return;
                                     }
                                     changed[j].type = type;
                                     kit::DiagnosticList diagnostics;
                                     m_roll->writePoints(PianoRoll::tr("Change Pitch Point"),
                                                         {
                                                             {index, changed}
                                     },
                                                         diagnostics);
                                     m_roll->report(diagnostics);
                                 });
            }
            menu.addSeparator();
            const auto remove = menu.addAction(PianoRoll::tr("Delete Point"));
            remove->setEnabled(points.size() > 2);
            QObject::connect(remove, &QAction::triggered, view(), [this, index, j] {
                kit::DiagnosticList diagnostics;
                m_roll->removePoints(
                    {
                        {index, {j}}
                },
                    diagnostics);
                m_roll->report(diagnostics);
            });
            menu.exec(view()->viewport()->mapToGlobal(position.toPoint()));
        }
    };

    // What is drawn over everything: the selection rectangle and the playhead
    class PianoRoll::Impl::OverlayLayer : public SceneLayer {
    public:
        explicit OverlayLayer(PianoRoll::Impl *roll) : m_roll(roll) {
        }

        void paint(QPainter &painter, const QRect &exposed) override {
            if (m_roll->band) {
                auto color = m_roll->_decl->selectionColor();
                painter.setPen(QPen(color, 1));
                color.setAlphaF(0.15f);
                painter.setBrush(color);
                painter.drawRect(*m_roll->band);
            }
            if (m_roll->playhead) {
                const double x = view()->timeAxis().toX(*m_roll->playhead);
                painter.setPen(QPen(m_roll->_decl->playheadColor(), 1));
                painter.drawLine(QPointF(x, exposed.top()), QPointF(x, exposed.bottom() + 1));
            }
        }

    private:
        PianoRoll::Impl *m_roll;
    };

    // The background of the parameter area with a line at the value of UTAU, and on the lane of
    // the envelopes the envelope of each sung note, over the fragment of its sample: a filled
    // outline from the start of the fragment through its anchors to its end. On that lane it
    // answers every position, so that a double click reaches it anywhere.
    class PianoRoll::Impl::EnvelopeLayer : public SceneLayer {
    public:
        explicit EnvelopeLayer(PianoRoll::Impl *roll) : m_roll(roll) {
        }

        void paint(QPainter &painter, const QRect &exposed) override {
            const auto decl = m_roll->_decl;
            const auto &keys = view()->keyAxis();
            const auto lane = m_roll->lane;
            const double fallback = keys.toY(keyOf(lane, rangeOf(lane).fallback));
            painter.fillRect(exposed, decl->whiteRowColor());
            painter.setPen(QPen(decl->lineColor(), 1, Qt::DashLine));
            painter.drawLine(QPointF(exposed.left(), fallback),
                             QPointF(exposed.right() + 1, fallback));
            if (m_roll->lane != EnvelopeLane) {
                return;
            }

            painter.setRenderHint(QPainter::Antialiasing);
            auto fill = decl->envelopeColor();
            fill.setAlphaF(fill.alphaF() * 0.25f);
            const auto [begin, end] = visibleNotes(exposed);
            for (int i = begin; i < end; ++i) {
                if (m_roll->timeline->note(i).rest) {
                    continue;
                }
                const auto outline = outlineOf(i);
                painter.setPen(QPen(decl->envelopeColor(), 1.5));
                painter.setBrush(fill);
                painter.drawPolygon(outline);
                painter.setBrush(decl->whiteRowColor());
                // The anchors, without the ends of the fragment
                for (qsizetype k = 1; k + 1 < outline.size(); ++k) {
                    painter.drawEllipse(outline[k], PointRadius, PointRadius);
                }
            }
        }

        std::optional<SceneHit> hitTest(QPointF position) const override {
            if (m_roll->lane != EnvelopeLane) {
                return std::nullopt;
            }
            SceneHit hit;
            hit.part = Background;
            double distance = m_roll->pointGrip;
            const auto [begin, end] = visibleNotes(QRect(position.toPoint(), QSize(1, 1)));
            for (int i = begin; i < end; ++i) {
                if (m_roll->timeline->note(i).rest) {
                    continue;
                }
                const auto outline = outlineOf(i);
                for (qsizetype k = 1; k + 1 < outline.size(); ++k) {
                    const auto offset = outline[k] - position;
                    const double d = std::hypot(offset.x(), offset.y());
                    if (d <= distance) {
                        distance = d;
                        hit.node = m_roll->timeline->note(i).id;
                        hit.part = EnvelopePoint;
                        hit.index = int(k - 1);
                        hit.cursor = Qt::SizeAllCursor;
                    }
                }
            }
            return hit;
        }

        std::unique_ptr<SceneGesture> press(const SceneHit &hit, QPointF position,
                                            Qt::MouseButton button,
                                            Qt::KeyboardModifiers modifiers) override;

        // On the middle anchor a double click removes it; on the envelope between the end of
        // the attack and the start of the release it inserts one there.
        bool doubleClick(const SceneHit &hit, QPointF position) override {
            if (hit.part == EnvelopePoint) {
                const int index = m_roll->indexOf(hit.node);
                auto envelope = m_roll->envelopeOf(index);
                if (!envelope.hasMiddle || hit.index != 2) {
                    return false;
                }
                envelope.hasMiddle = false;
                write(index, envelope);
                return true;
            }
            const auto [begin, end] = visibleNotes(QRect(position.toPoint(), QSize(1, 1)));
            for (int i = begin; i < end; ++i) {
                if (m_roll->timeline->note(i).rest) {
                    continue;
                }
                auto envelope = m_roll->envelopeOf(i);
                if (envelope.hasMiddle) {
                    continue;
                }
                const auto outline = outlineOf(i);
                // Between the end of the attack and the start of the release
                const QLineF line(outline[2], outline[3]);
                if (position.x() <= line.x1() || position.x() >= line.x2()) {
                    continue;
                }
                const double share = (position.x() - line.x1()) / (line.x2() - line.x1());
                const auto on = line.pointAt(share);
                if (std::abs(on.y() - position.y()) > m_roll->curveGrip) {
                    continue;
                }
                const auto [start, length] = m_roll->fragmentOf(i);
                const auto times = anchorTimes(envelope, length);
                const double at =
                    m_roll->timeline->tempoMap().timeOf(view()->timeAxis().toTick(position.x())) -
                    start;
                envelope.hasMiddle = true;
                envelope.anchors[2].x = std::round((at - times[1]) * 10) / 10;
                envelope.anchors[2].y = std::round(view()->keyAxis().toKey(on.y()));
                write(i, envelope);
                return true;
            }
            return false;
        }

        void write(int index, const kit::Envelope &envelope) {
            kit::DiagnosticList diagnostics;
            kit::ProjectEdits::setEnvelope({m_roll->notes().at(index)}, envelope, diagnostics);
            m_roll->report(diagnostics);
        }

    private:
        PianoRoll::Impl *m_roll;

        // The notes whose fragments may reach into rect: those at its time and one on either
        // side, since a fragment starts before its note and ends after it
        std::pair<int, int> visibleNotes(const QRect &rect) const {
            const auto &time = view()->timeAxis();
            const auto [begin, end] = m_roll->timeline->notesBetween(time.toTick(rect.left()),
                                                                     time.toTick(rect.right() + 1));
            return {std::max(0, begin - 1), std::min(m_roll->timeline->noteCount(), end + 1)};
        }

        // The outline of the envelope of note index: the start of its fragment, its anchors in
        // time order, and the end of its fragment
        QPolygonF outlineOf(int index) const {
            const auto envelope = m_roll->envelopeOf(index);
            const auto length = m_roll->fragmentOf(index).second;
            const auto times = anchorTimes(envelope, length);
            const auto anchors = envelope.anchorsInTimeOrder();
            QPolygonF outline{m_roll->envelopePointOf(index, 0, 0)};
            for (qsizetype k = 0; k < times.size(); ++k) {
                outline.push_back(m_roll->envelopePointOf(index, times[k], anchors[k].y));
            }
            outline.push_back(m_roll->envelopePointOf(index, length, 0));
            return outline;
        }
    };

    // A drag of an anchor of an envelope: in time between its neighbours, the others staying
    // where they are, and in volume from 0 to EnvelopeRange. Times are kept to a tenth of a
    // millisecond, volumes to a percent.
    class PianoRoll::Impl::EnvelopeGesture : public SceneGesture {
    public:
        EnvelopeGesture(PianoRoll::Impl *roll, int index, int anchor, QPointF position)
            : m_roll(roll), m_index(index), m_anchor(anchor), m_origin(position),
              m_original(roll->envelopeOf(index)) {
            m_length = roll->fragmentOf(index).second;
        }

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override {
            Q_UNUSED(modifiers);
            const auto view = m_roll->parameters;
            const auto &map = m_roll->timeline->tempoMap();
            const auto timeAt = [&](QPointF point) {
                return map.timeOf(view->timeAxis().toTick(point.x()));
            };
            const auto times = anchorTimes(m_original, m_length);
            const int last = int(times.size()) - 1;
            const int k = m_anchor;
            double delta = std::round((timeAt(position) - timeAt(m_origin)) * 10) / 10;
            delta = std::clamp(delta, (k > 0 ? times[k - 1] : 0) - times[k],
                               (k < last ? times[k + 1] : m_length) - times[k]);

            // Each p counts from a neighbour, so a move changes two of them at most; the others
            // keep their values exactly.
            auto anchors = m_original.anchorsInTimeOrder();
            if (k < last - 1) {
                // p1, p2 and p5 count forward
                anchors[k].x += delta;
                if (k + 1 < last - 1) {
                    anchors[k + 1].x -= delta;
                }
            } else if (k == last - 1) {
                // p3 counts back from the end
                anchors[k].x -= delta;
            } else {
                // p4 counts back from the end of the fragment, p3 back from it
                anchors[k].x -= delta;
                anchors[k - 1].x += delta;
            }
            const double volume = m_original.anchorsInTimeOrder()[k].y +
                                  view->keyAxis().toKey(position.y()) -
                                  view->keyAxis().toKey(m_origin.y());
            anchors[k].y = std::clamp(std::round(volume), 0.0, EnvelopeRange);

            m_roll->envelopePreview.insert(m_index, *kit::Envelope::fromTimeOrder(anchors));
            view->viewport()->update();
        }

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override {
            move(position, modifiers);
            const auto envelope = m_roll->envelopePreview.take(m_index);
            m_roll->parameters->viewport()->update();
            if (envelope != m_original) {
                kit::DiagnosticList diagnostics;
                kit::ProjectEdits::setEnvelope({m_roll->notes().at(m_index)}, envelope,
                                               diagnostics);
                m_roll->report(diagnostics);
            }
        }

        void cancel() override {
            m_roll->envelopePreview.remove(m_index);
            m_roll->parameters->viewport()->update();
        }

    private:
        PianoRoll::Impl *m_roll;
        int m_index;
        int m_anchor;
        QPointF m_origin;
        kit::Envelope m_original;
        double m_length = 0;
    };

    // A press on an anchor drags it; the right button sets its volume to 100%.
    std::unique_ptr<SceneGesture>
        PianoRoll::Impl::EnvelopeLayer::press(const SceneHit &hit, QPointF position,
                                              Qt::MouseButton button,
                                              Qt::KeyboardModifiers modifiers) {
        Q_UNUSED(modifiers);
        if (hit.part != EnvelopePoint) {
            return nullptr;
        }
        m_roll->finishEditing(true);
        const int index = m_roll->indexOf(hit.node);
        if (index < 0) {
            return nullptr;
        }
        if (button == Qt::RightButton) {
            auto anchors = m_roll->envelopeOf(index).anchorsInTimeOrder();
            if (anchors[hit.index].y != 100) {
                anchors[hit.index].y = 100;
                write(index, *kit::Envelope::fromTimeOrder(anchors));
            }
            return nullptr;
        }
        if (button != Qt::LeftButton) {
            return nullptr;
        }
        return std::make_unique<EnvelopeGesture>(m_roll, index, hit.index, position);
    }

    // The value of the lane of each sung note in the parameter area, as QSynthesis draws it: a
    // point at the start of the note, a line across the note at the value, and a stem down to
    // the bottom of the area. A note that leaves the value to the default of UTAU is drawn in
    // faintPointColor, a selected one filled.
    class PianoRoll::Impl::ValueLayer : public SceneLayer {
    public:
        explicit ValueLayer(PianoRoll::Impl *roll) : m_roll(roll) {
        }

        void paint(QPainter &painter, const QRect &exposed) override {
            if (m_roll->lane == EnvelopeLane) {
                return;
            }
            const auto decl = m_roll->_decl;
            const double bottom = view()->keyAxis().toY(rangeOf(m_roll->lane).minimum);
            painter.setRenderHint(QPainter::Antialiasing);
            const auto [begin, end] = visibleNotes(exposed);
            for (int i = begin; i < end; ++i) {
                if (m_roll->timeline->note(i).rest) {
                    continue;
                }
                const auto [point, right] = handleOf(i);
                const bool stored =
                    m_roll->valuePreview.contains(i) || m_roll->storedValueOf(i).has_value();
                const auto color = stored ? decl->parameterColor() : decl->faintPointColor();
                painter.setPen(QPen(color, 1.5));
                painter.drawLine(point, QPointF(point.x(), bottom));
                painter.drawLine(point, QPointF(right, point.y()));
                painter.setBrush(m_roll->isSelected(i) ? color : decl->whiteRowColor());
                painter.drawEllipse(point, PointRadius, PointRadius);
            }
        }

        // The point of a handle, or anywhere on the line across its note
        std::optional<SceneHit> hitTest(QPointF position) const override {
            if (m_roll->lane == EnvelopeLane) {
                return std::nullopt;
            }
            std::optional<SceneHit> hit;
            double distance = std::numeric_limits<double>::infinity();
            const auto [begin, end] = visibleNotes(QRect(position.toPoint(), QSize(1, 1)));
            for (int i = begin; i < end; ++i) {
                if (m_roll->timeline->note(i).rest) {
                    continue;
                }
                const auto [point, right] = handleOf(i);
                const auto offset = point - position;
                double d = std::hypot(offset.x(), offset.y());
                if (d > m_roll->pointGrip) {
                    d = position.x() >= point.x() && position.x() <= right
                            ? std::abs(position.y() - point.y())
                            : std::numeric_limits<double>::infinity();
                    if (d > m_roll->curveGrip) {
                        continue;
                    }
                }
                if (d < distance) {
                    distance = d;
                    hit = SceneHit();
                    hit->node = m_roll->timeline->note(i).id;
                    hit->part = ParameterHandle;
                    hit->cursor = Qt::SizeVerCursor;
                }
            }
            return hit;
        }

        std::unique_ptr<SceneGesture> press(const SceneHit &hit, QPointF position,
                                            Qt::MouseButton button,
                                            Qt::KeyboardModifiers modifiers) override;

    private:
        PianoRoll::Impl *m_roll;

        std::pair<int, int> visibleNotes(const QRect &rect) const {
            const auto &time = view()->timeAxis();
            return m_roll->timeline->notesBetween(time.toTick(rect.left()),
                                                  time.toTick(rect.right() + 1));
        }

        // The point of the handle of note index, and the right end of its line. A value beyond
        // the lane is drawn at its edge.
        std::pair<QPointF, double> handleOf(int index) const {
            const auto &note = m_roll->timeline->note(index);
            const auto &time = view()->timeAxis();
            const auto lane = m_roll->lane;
            const auto range = rangeOf(lane);
            const double key =
                std::clamp(keyOf(lane, m_roll->valueOf(index)), range.minimum, range.maximum);
            return {QPointF(time.toX(double(note.start)), view()->keyAxis().toY(key)),
                    time.toX(double(note.start + note.length))};
        }
    };

    // A drag of the handle of a value: every note it changes takes the value it is dragged to,
    // a whole number within the lane.
    class PianoRoll::Impl::ValueGesture : public SceneGesture {
    public:
        ValueGesture(PianoRoll::Impl *roll, int index, QPointF position)
            : m_roll(roll), m_targets(roll->valueTargets(index)), m_origin(position),
              m_start(roll->valueOf(index)) {
        }

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override {
            Q_UNUSED(modifiers);
            const auto &keys = m_roll->parameters->keyAxis();
            const auto lane = m_roll->lane;
            const auto range = rangeOf(lane);
            const double key = std::clamp(keyOf(lane, m_start) + keys.toKey(position.y()) -
                                              keys.toKey(m_origin.y()),
                                          range.minimum, range.maximum);
            const double value = std::round(valueAt(lane, key));
            for (const int i : std::as_const(m_targets)) {
                m_roll->valuePreview.insert(i, value);
            }
            m_roll->parameters->viewport()->update();
        }

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override {
            move(position, modifiers);
            const double value = m_roll->valuePreview.value(m_targets.first());
            m_roll->valuePreview.clear();
            m_roll->parameters->viewport()->update();
            m_roll->writeValue(m_targets, value);
        }

        void cancel() override {
            m_roll->valuePreview.clear();
            m_roll->parameters->viewport()->update();
        }

    private:
        PianoRoll::Impl *m_roll;
        QList<int> m_targets;
        QPointF m_origin;
        double m_start;
    };

    // A press on a handle drags it; the right button removes the value, which leaves the default
    // of UTAU. Either applies to the selected notes if the note is selected.
    std::unique_ptr<SceneGesture>
        PianoRoll::Impl::ValueLayer::press(const SceneHit &hit, QPointF position,
                                           Qt::MouseButton button,
                                           Qt::KeyboardModifiers modifiers) {
        Q_UNUSED(modifiers);
        m_roll->finishEditing(true);
        const int index = m_roll->indexOf(hit.node);
        if (hit.part != ParameterHandle || index < 0) {
            return nullptr;
        }
        if (button == Qt::RightButton) {
            m_roll->writeValue(m_roll->valueTargets(index), std::nullopt);
            return nullptr;
        }
        if (button != Qt::LeftButton) {
            return nullptr;
        }
        return std::make_unique<ValueGesture>(m_roll, index, position);
    }

    // A drag of the selected notes: vertically transposes them, horizontally moves them in the
    // sequence to the boundary between notes nearest to where they are dragged. A selection with
    // gaps is first extended to the run of notes it spans, see step 4 in docs/Widgets.md.
    class PianoRoll::Impl::MoveGesture : public SceneGesture {
    public:
        MoveGesture(PianoRoll::Impl *roll, int pressed, QPointF position, bool wasSelected)
            : m_roll(roll), m_pressed(pressed), m_origin(position), m_wasSelected(wasSelected),
              m_previous(roll->selection) {
        }

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override {
            Q_UNUSED(modifiers);
            if (!m_dragging) {
                if ((position - m_origin).manhattanLength() < QApplication::startDragDistance()) {
                    return;
                }
                start();
            }

            const auto timeline = m_roll->timeline;
            const auto &time = m_roll->view->timeAxis();
            const auto &keys = m_roll->view->keyAxis();

            // The keys, kept within the range of note numbers
            int lowest = kit::highestNoteNum;
            int highest = kit::lowestNoteNum;
            for (int i = m_first; i <= m_last; ++i) {
                lowest = std::min(lowest, timeline->note(i).key);
                highest = std::max(highest, timeline->note(i).key);
            }
            m_semitones = std::clamp(keys.keyAt(position.y()) - keys.keyAt(m_origin.y()),
                                     kit::lowestNoteNum - lowest, kit::highestNoteNum - highest);

            // The boundary between the other notes nearest to where the run starts now
            QList<int> others;
            for (int i = 0; i < timeline->noteCount(); ++i) {
                if (i < m_first || i > m_last) {
                    others.push_back(i);
                }
            }
            const double wanted = double(timeline->note(m_first).start) +
                                  time.toTick(position.x()) - time.toTick(m_origin.x());
            qint64 boundary = 0;
            double nearest = std::abs(wanted);
            m_destination = 0;
            for (int i = 0; i < others.size(); ++i) {
                boundary += timeline->note(others[i]).length;
                if (std::abs(wanted - double(boundary)) < nearest) {
                    nearest = std::abs(wanted - double(boundary));
                    m_destination = i + 1;
                }
            }

            QList<int> order = others.mid(0, m_destination);
            QSet<int> run;
            for (int i = m_first; i <= m_last; ++i) {
                order.push_back(i);
                run.insert(i);
            }
            order += others.mid(m_destination);
            m_roll->placements = m_roll->layOut(order, {}, run, m_semitones);
            m_roll->view->viewport()->update();
        }

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override {
            move(position, modifiers);
            if (!m_dragging) {
                // A click on one of several selected notes selects it alone.
                if (m_wasSelected && m_roll->selection.size() > 1) {
                    m_roll->selectOnly(m_pressed);
                }
                return;
            }

            const bool moved = m_destination != m_first;
            m_roll->clearPreview();
            if (!moved && m_semitones == 0) {
                return;
            }
            const auto notes = m_roll->notes();
            QList<kit::NoteRef> run;
            for (int i = m_first; i <= m_last; ++i) {
                run.push_back(notes.at(i));
            }
            auto transaction = m_roll->session->transaction(moved ? PianoRoll::tr("Move Notes")
                                                                  : PianoRoll::tr("Transpose"));
            kit::DiagnosticList diagnostics;
            if (moved) {
                kit::ProjectEdits::moveNotes(notes, m_first, int(run.size()), m_destination,
                                             diagnostics);
            }
            kit::ProjectEdits::transpose(run, m_semitones, diagnostics);
            transaction.commit(diagnostics);
            m_roll->report(diagnostics);
        }

        void cancel() override {
            m_roll->clearPreview();
            m_roll->setSelection(m_previous);
        }

    private:
        PianoRoll::Impl *m_roll;
        int m_pressed;
        QPointF m_origin;
        bool m_wasSelected;
        QSet<kit::edit::NodeId> m_previous;
        bool m_dragging = false;
        int m_first = 0;
        int m_last = 0;
        int m_semitones = 0;
        int m_destination = 0;

        void start() {
            m_dragging = true;
            const auto selected = m_roll->_decl->selectedIndices();
            m_first = selected.isEmpty() ? m_pressed : selected.first();
            m_last = selected.isEmpty() ? m_pressed : selected.last();
            m_roll->selectRange(m_first, m_last);
            m_destination = m_first;
        }
    };

    // A drag of the right edge of a note, which changes its length
    class PianoRoll::Impl::LengthGesture : public SceneGesture {
    public:
        LengthGesture(PianoRoll::Impl *roll, int index)
            : m_roll(roll), m_index(index), m_start(roll->timeline->note(index).start),
              m_original(roll->timeline->note(index).length), m_length(m_original) {
        }

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override {
            const double tick = m_roll->view->timeAxis().toTick(position.x());
            qint64 length = m_roll->snapped(tick, modifiers) - m_start;
            if (length <= 0) {
                // The first grid line after the start, or one tick
                length = m_roll->snaps(modifiers)
                             ? m_roll->snappedDown(double(m_start), modifiers) +
                                   m_roll->quantization - m_start
                             : 1;
            }
            m_length = int(length);
            m_roll->placements = m_roll->layOut(m_roll->identityOrder(), {
                                                                             {m_index, m_length}
            });
            m_roll->view->viewport()->update();
        }

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override {
            move(position, modifiers);
            m_roll->clearPreview();
            if (m_length == m_original) {
                return;
            }
            kit::DiagnosticList diagnostics;
            kit::ProjectEdits::setLength(m_roll->notes().at(m_index), m_length, diagnostics);
            m_roll->report(diagnostics);
        }

        void cancel() override {
            m_roll->clearPreview();
        }

    private:
        PianoRoll::Impl *m_roll;
        int m_index;
        qint64 m_start;
        int m_original;
        int m_length;
    };

    // A drag on the background that selects the notes in a rectangle, added to the selection
    // with Ctrl
    class PianoRoll::Impl::BandGesture : public SceneGesture {
    public:
        BandGesture(PianoRoll::Impl *roll, QPointF position, Qt::KeyboardModifiers modifiers)
            : m_roll(roll), m_origin(position), m_previous(roll->selection),
              m_previousPoints(roll->selectedPoints) {
            if (modifiers & Qt::ControlModifier) {
                m_base = roll->selection;
                m_basePoints = roll->selectedPoints;
            }
            roll->setSelection(m_base);
            roll->selectPoints(m_basePoints);
        }

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override {
            Q_UNUSED(modifiers);
            const auto rect = QRectF(m_origin, position).normalized();
            const auto timeline = m_roll->timeline;
            const auto &time = m_roll->view->timeAxis();
            const auto [begin, end] =
                timeline->notesBetween(time.toTick(rect.left()), time.toTick(rect.right()));
            m_roll->band = rect;
            m_roll->view->viewport()->update();

            // With the pitch shown, the points in the rectangle if there are any, among them
            // those of the notes beside it, which may lie beyond their notes
            if (m_roll->pointsShown()) {
                auto points = m_basePoints;
                bool found = false;
                const auto refs = m_roll->notes();
                for (int i = std::max(0, begin - 1); i < std::min(timeline->noteCount(), end + 1);
                     ++i) {
                    if (timeline->note(i).rest) {
                        continue;
                    }
                    const auto values = m_roll->pointsOf(i);
                    const auto list = refs.at(i).portamento();
                    for (int j = 0; j < values.size() && j < list.size(); ++j) {
                        if (rect.contains(m_roll->positionOf(i, j, values[j]))) {
                            points.insert(list.at(j).id());
                            found = true;
                        }
                    }
                }
                if (found || !m_basePoints.isEmpty()) {
                    m_roll->selectPoints(points);
                    return;
                }
            }

            auto ids = m_base;
            for (int i = begin; i < end; ++i) {
                const auto &note = timeline->note(i);
                if (m_roll->rectOf(note.start, note.length, note.key).intersects(rect)) {
                    ids.insert(note.id);
                }
            }
            m_roll->selectPoints({});
            m_roll->setSelection(ids);
        }

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override {
            move(position, modifiers);
            m_roll->clearPreview();
        }

        void cancel() override {
            m_roll->clearPreview();
            m_roll->setSelection(m_previous);
            m_roll->selectPoints(m_previousPoints);
        }

    private:
        PianoRoll::Impl *m_roll;
        QPointF m_origin;
        QSet<kit::edit::NodeId> m_base;
        QSet<kit::edit::NodeId> m_previous;
        QSet<kit::edit::NodeId> m_basePoints;
        QSet<kit::edit::NodeId> m_previousPoints;
    };

    // A drag of the pen on the background, which draws a note (step 4 in docs/Widgets.md). The
    // note goes before the note at the pointer, or after the last note, and starts where the note
    // before it ends; the notes after it start later by its length. With Shift held on the press,
    // a rest fills the gap from there to the pointer and the note starts at the pointer; within
    // a rest, the two take its place, and the notes after it start later only as far as the note
    // passes its end. The drag sets the length, snapped to the quantization. The first note
    // inserted where a note that sets a tempo started takes that tempo, so that the tempo there
    // stays.
    class PianoRoll::Impl::DrawGesture : public SceneGesture {
    public:
        DrawGesture(PianoRoll::Impl *roll, QPointF position, Qt::KeyboardModifiers modifiers)
            : m_roll(roll) {
            const auto timeline = roll->timeline;
            const auto &keys = roll->view->keyAxis();
            m_key = std::clamp(keys.keyAt(position.y()), kit::lowestNoteNum, kit::highestNoteNum);
            const double tick = roll->view->timeAxis().toTick(position.x());
            const int count = timeline->noteCount();
            m_index = std::clamp(timeline->noteAt(tick), 0, count);
            m_from = m_index < count ? timeline->note(m_index).start : timeline->length();
            m_fills = modifiers & Qt::ShiftModifier;
            m_splits = m_fills && m_index < count && timeline->note(m_index).rest;
            m_start = m_fills ? std::max(m_from, roll->snappedDown(tick, modifiers)) : m_from;
            const qint64 reach = roll->snapped(tick, modifiers) - m_start;
            update(!m_fills && reach > 0 ? int(reach) : roll->_decl->quantizedLength());
        }

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override {
            const double tick = m_roll->view->timeAxis().toTick(position.x());
            const qint64 length = m_roll->snapped(tick, modifiers) - m_start;
            update(length > 0 ? int(length)
                              : (m_roll->snaps(modifiers) ? m_roll->quantization : 1));
        }

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override {
            move(position, modifiers);
            m_roll->clearPreview();

            const auto notes = m_roll->notes();
            const auto rest = [this](int length) {
                kit::Note note;
                note.lyric = QString::fromLatin1(kit::restLyric);
                note.length = length;
                note.noteNum = m_key;
                return note;
            };
            kit::Note note;
            note.lyric = QString::fromLatin1(kit::defaultLyric);
            note.length = m_length;
            note.noteNum = m_key;
            const auto tempo = m_index < notes.size() ? notes.at(m_index).tempo() : std::nullopt;

            // One note drawn, with the rests around it
            auto transaction = m_roll->session->transaction(PianoRoll::tr("Insert Note"));
            kit::DiagnosticList diagnostics;
            int drawn = m_index;
            if (m_splits) {
                const auto split = notes.at(m_index);
                const int offset = int(m_start - m_from);
                const int remainder = split.length() - offset - m_length;
                if (offset > 0) {
                    kit::ProjectEdits::setLength(split, offset, diagnostics);
                    QList<kit::Note> inserted{note};
                    if (remainder > 0) {
                        inserted.push_back(rest(remainder));
                    }
                    drawn = m_index + 1;
                    kit::ProjectEdits::insertNotes(notes, drawn, inserted, diagnostics);
                } else {
                    note.tempo = tempo;
                    if (remainder > 0) {
                        kit::ProjectEdits::setLength(split, remainder, diagnostics);
                    } else {
                        kit::ProjectEdits::removeNotes(notes, {m_index}, diagnostics);
                    }
                    kit::ProjectEdits::insertNotes(notes, m_index, {note}, diagnostics);
                }
            } else {
                QList<kit::Note> inserted;
                if (m_start > m_from) {
                    inserted.push_back(rest(int(m_start - m_from)));
                }
                inserted.push_back(note);
                inserted.first().tempo = tempo;
                drawn = m_index + int(inserted.size()) - 1;
                kit::ProjectEdits::insertNotes(notes, m_index, inserted, diagnostics);
            }
            if (transaction.commit(diagnostics)) {
                const auto id = notes.at(drawn).id();
                m_roll->anchor = id;
                m_roll->setSelection({id});
            }
            m_roll->report(diagnostics);
        }

        void cancel() override {
            m_roll->clearPreview();
        }

    private:
        PianoRoll::Impl *m_roll;
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
        void update(int length) {
            m_length = length;
            m_roll->drawn = Placement{-1, m_start, m_length, m_key};
            const auto timeline = m_roll->timeline;
            m_roll->placements.clear();
            if (m_index < timeline->noteCount()) {
                const auto &at = timeline->note(m_index);
                const qint64 shift =
                    m_splits ? std::max<qint64>(0, m_start + m_length - at.start - at.length)
                             : m_start - m_from + m_length;
                for (int i = 0; i < timeline->noteCount(); ++i) {
                    const auto &note = timeline->note(i);
                    Placement placement{i, note.start, note.length, note.key};
                    if (i == m_index && m_splits) {
                        // What remains of the rest: before the note, or else after it
                        const qint64 before = m_start - m_from;
                        const qint64 after = at.length - before - m_length;
                        if (before > 0) {
                            placement.length = int(before);
                        } else if (after > 0) {
                            placement.start = m_start + m_length;
                            placement.length = int(after);
                        } else {
                            continue;
                        }
                    } else if (i >= m_index) {
                        placement.start += shift;
                    }
                    m_roll->placements.push_back(placement);
                }
            }
            m_roll->view->viewport()->update();
        }
    };

    // A drag of the selected points, all by the same time and height. The points of a note are
    // kept in time order, a moving point passing the others, and the heights that are fixed stay
    // (heightFixed(), by the place of a point before the drag). Shift snaps the
    // pressed point to the time of another point of its note, Ctrl its height to PitchSnap.
    class PianoRoll::Impl::PointGesture : public SceneGesture {
    public:
        PointGesture(PianoRoll::Impl *roll, int index, int point, QPointF position)
            : m_roll(roll), m_index(index), m_point(point), m_origin(position) {
        }

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override {
            if (!m_dragging) {
                if ((position - m_origin).manhattanLength() < QApplication::startDragDistance()) {
                    return;
                }
                start();
            }
            const auto &time = m_roll->view->timeAxis();
            const auto &keys = m_roll->view->keyAxis();
            double ticks = time.toTick(position.x()) - time.toTick(m_origin.x());
            double cents = (keys.toKey(position.y()) - keys.toKey(m_origin.y())) * 100;

            const auto &pressedNote = m_original[m_index];
            const auto &pressed = pressedNote[m_point];
            if (modifiers & Qt::ShiftModifier) {
                const double at = m_roll->ticksOf(pressed.x, m_index);
                std::optional<double> nearest;
                for (int j = 0; j < pressedNote.size(); ++j) {
                    const double other = m_roll->ticksOf(pressedNote[j].x, m_index);
                    if (!m_moving[m_index].contains(j) &&
                        (!nearest ||
                         std::abs(other - at - ticks) < std::abs(*nearest - at - ticks))) {
                        nearest = other;
                    }
                }
                if (nearest) {
                    ticks = *nearest - at;
                }
            }
            if (modifiers & Qt::ControlModifier) {
                cents = std::round((pressed.y + cents) / PitchSnap) * PitchSnap - pressed.y;
            }

            // The points of each note in time order again, the moving ones passing the others;
            // where a moving point ends up is kept for selecting it afterwards.
            m_roll->pointPreview.clear();
            m_moved.clear();
            for (auto it = m_moving.begin(); it != m_moving.end(); ++it) {
                const int index = it.key();
                const auto &original = m_original[index];
                const int count = int(original.size());
                QList<std::pair<kit::PortamentoPoint, bool>> points;
                for (int j = 0; j < count; ++j) {
                    auto point = original[j];
                    const bool moving = it.value().contains(j);
                    if (moving) {
                        // To a tenth of a millisecond and a cent
                        point.x =
                            std::round((point.x + m_roll->millisecondsOf(ticks, index)) * 10) / 10;
                        if (!m_roll->heightFixed(index, j, count)) {
                            point.y = std::round(point.y + cents);
                        }
                    }
                    points.push_back({point, moving});
                }
                std::stable_sort(points.begin(), points.end(), [](const auto &a, const auto &b) {
                    return a.first.x < b.first.x;
                });
                QList<kit::PortamentoPoint> sorted;
                for (int j = 0; j < count; ++j) {
                    sorted.push_back(points[j].first);
                    if (points[j].second) {
                        m_moved[index].insert(j);
                    }
                }
                // As it will be written
                endAtPitch(sorted);
                m_roll->pointPreview.insert(index, sorted);
            }
            m_roll->view->viewport()->update();
        }

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override {
            move(position, modifiers);
            if (!m_dragging) {
                return;
            }
            QHash<int, QList<kit::PortamentoPoint>> changed;
            for (auto it = m_roll->pointPreview.begin(); it != m_roll->pointPreview.end(); ++it) {
                if (it.value() != m_original[it.key()]) {
                    changed.insert(it.key(), it.value());
                }
            }
            m_roll->pointPreview.clear();
            m_roll->view->viewport()->update();
            if (changed.isEmpty()) {
                return;
            }
            kit::DiagnosticList diagnostics;
            if (!m_roll->writePoints(PianoRoll::tr("Move Pitch Points"), changed, diagnostics)) {
                m_roll->report(diagnostics);
                return;
            }
            // The same points stay selected where the order put them.
            QSet<kit::edit::NodeId> ids;
            const auto refs = m_roll->notes();
            for (auto it = m_moved.begin(); it != m_moved.end(); ++it) {
                const auto list = refs.at(it.key()).portamento();
                for (const int j : it.value()) {
                    ids.insert(list.at(j).id());
                }
            }
            m_roll->selectPoints(ids);
        }

        void cancel() override {
            m_roll->pointPreview.clear();
            m_roll->view->viewport()->update();
        }

    private:
        PianoRoll::Impl *m_roll;
        int m_index;
        int m_point;
        QPointF m_origin;
        bool m_dragging = false;
        // The points that move, by note index, and the points of those notes before the drag
        QHash<int, QSet<int>> m_moving;
        QHash<int, QList<kit::PortamentoPoint>> m_original;
        // Where the moving points are in the points shown, by note index
        QHash<int, QSet<int>> m_moved;

        void start() {
            m_dragging = true;
            m_moving = m_roll->selectedPointIndices();
            m_moving[m_index].insert(m_point);
            for (auto it = m_moving.begin(); it != m_moving.end(); ++it) {
                m_original.insert(it.key(), m_roll->pointsOf(it.key()));
            }
        }
    };

    // A drag of a handle of the vibrato of a note (see Part). The values are whole numbers, as
    // UTAU shows them, and the percentages lie between 0 and 100.
    class PianoRoll::Impl::VibratoGesture : public SceneGesture {
    public:
        VibratoGesture(PianoRoll::Impl *roll, int index, int part, QPointF position)
            : m_roll(roll), m_index(index), m_part(part), m_origin(position),
              m_original(roll->vibratoOf(index).value_or(kit::Vibrato())) {
        }

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override {
            Q_UNUSED(modifiers);
            const auto &time = m_roll->view->timeAxis();
            const auto &keys = m_roll->view->keyAxis();
            const double ticks = time.toTick(position.x()) - time.toTick(m_origin.x());
            const double length = m_roll->timeline->note(m_index).length;
            const double span = m_original.length / 100 * length;
            const double period = m_roll->ticksOf(m_original.period, m_index);
            const auto percent = [](double value) {
                return std::clamp(std::round(value), 0.0, 100.0);
            };

            auto vibrato = m_original;
            switch (m_part) {
                case VibratoStart:
                    vibrato.length = percent((span - ticks) / length * 100);
                    break;
                case VibratoFadeIn:
                    if (span > 0) {
                        vibrato.attack = percent(m_original.attack + ticks / span * 100);
                    }
                    break;
                case VibratoFadeOut:
                    if (span > 0) {
                        vibrato.release = percent(m_original.release - ticks / span * 100);
                    }
                    break;
                case VibratoDepth:
                    vibrato.amplitude =
                        std::max(0.0, std::round(m_original.amplitude + (keys.toKey(position.y()) -
                                                                         keys.toKey(m_origin.y())) *
                                                                            VibratoCentsPerRow));
                    break;
                case VibratoPeriod: {
                    // The right edge of the box follows the pointer; the box starts at the
                    // phase, a share of the period itself.
                    const double share = 1 + m_original.phase / 100;
                    vibrato.period = std::max(1.0, std::round(m_roll->millisecondsOf(
                                                       (share * period + ticks) / share, m_index)));
                    break;
                }
                case VibratoPhase:
                    if (period > 0) {
                        vibrato.phase = percent(m_original.phase + ticks / period * 100);
                    }
                    break;
                default:
                    break;
            }
            m_roll->vibratoPreview.insert(m_index, vibrato);
            m_roll->view->viewport()->update();
        }

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override {
            move(position, modifiers);
            const auto vibrato = m_roll->vibratoPreview.take(m_index);
            m_roll->view->viewport()->update();
            if (vibrato != m_original) {
                kit::DiagnosticList diagnostics;
                kit::ProjectEdits::setVibrato({m_roll->notes().at(m_index)}, vibrato, diagnostics);
                m_roll->report(diagnostics);
            }
        }

        void cancel() override {
            m_roll->vibratoPreview.remove(m_index);
            m_roll->view->viewport()->update();
        }

    private:
        PianoRoll::Impl *m_roll;
        int m_index;
        int m_part;
        QPointF m_origin;
        kit::Vibrato m_original;
    };

    // A stroke of the Mode1 pitch (step 5 in docs/Tuning.md). Each sung note along it takes, at
    // the places of its values from its first reading to its end, the pitch of the path of the
    // pointer there, a later part of the path over an earlier one, in whole cents; or 0 for a
    // stroke that erases, which changes only the values a note has. A note without values
    // starts them at its first reading, as UTAU does, those before the stroke taking the curve
    // as it was. Written when released, in one step.
    class PianoRoll::Impl::BendGesture : public SceneGesture {
    public:
        BendGesture(PianoRoll::Impl *roll, QPointF position, bool erases)
            : m_roll(roll), m_erases(erases) {
            add(position);
            update();
        }

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override {
            Q_UNUSED(modifiers);
            add(position);
            update();
        }

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override {
            move(position, modifiers);
            m_roll->bendPreview.clear();
            m_roll->view->viewport()->update();
            if (m_drawn.isEmpty()) {
                return;
            }
            kit::DiagnosticList diagnostics;
            auto transaction = m_roll->session->transaction(m_erases ? PianoRoll::tr("Reset Pitch")
                                                                     : PianoRoll::tr("Draw Pitch"));
            // From the last note back, so that each note fills its gaps from the curve of the
            // previous note as it was
            auto indices = m_drawn.keys();
            std::sort(indices.begin(), indices.end(), std::greater<>());
            const auto notes = m_roll->notes();
            for (const int index : std::as_const(indices)) {
                const auto &[tick, values] = m_drawn[index];
                kit::ProjectEdits::drawPitchBend(notes, index, tick, values, diagnostics);
            }
            transaction.commit(diagnostics);
            m_roll->report(diagnostics);
        }

        void cancel() override {
            m_roll->bendPreview.clear();
            m_roll->view->viewport()->update();
        }

    private:
        PianoRoll::Impl *m_roll;
        bool m_erases;
        // The path of the pointer, in ticks and keys
        QList<QPointF> m_path;
        // What is written, by note index: the tick of the first value, and the values
        QHash<int, std::pair<double, QList<double>>> m_drawn;

        void add(QPointF position) {
            const auto &time = m_roll->view->timeAxis();
            const auto &keys = m_roll->view->keyAxis();
            m_path.push_back({time.toTick(position.x()), keys.toKey(position.y())});
        }

        // The key of the path at tick, where the latest part of the path that spans it lies
        std::optional<double> keyAt(double tick) const {
            for (auto i = m_path.size() - 1; i >= 1; --i) {
                const auto a = m_path[i - 1];
                const auto b = m_path[i];
                if (tick < std::min(a.x(), b.x()) || tick > std::max(a.x(), b.x())) {
                    continue;
                }
                if (a.x() == b.x()) {
                    return b.y();
                }
                return a.y() + (b.y() - a.y()) * (tick - a.x()) / (b.x() - a.x());
            }
            if (m_path.size() == 1 && tick == m_path.first().x()) {
                return m_path.first().y();
            }
            return std::nullopt;
        }

        void update() {
            m_roll->bendPreview.clear();
            m_drawn.clear();
            double low = m_path.first().x();
            double high = low;
            for (const auto &point : std::as_const(m_path)) {
                low = std::min(low, point.x());
                high = std::max(high, point.x());
            }

            // The notes along the path, and the next one, which reads its curve before it
            const auto timeline = m_roll->timeline;
            const int count = timeline->noteCount();
            auto [begin, end] = timeline->notesBetween(low, high);
            end = std::min(count, end + 1);
            const auto &timings = m_roll->sampleTimings();
            const auto refs = m_roll->notes();
            for (int i = begin; i < end; ++i) {
                const auto &entry = timeline->note(i);
                if (entry.rest) {
                    continue;
                }
                const auto note = refs.at(i).toNote();
                const auto &bend = note.pitchBend;
                const bool hasValues = bend && !bend->values.isEmpty();
                if (m_erases && !hasValues) {
                    continue;
                }
                kit::PreviousBend previous;
                if (i > 0) {
                    previous = kit::PreviousBend::of(refs.at(i - 1).toNote(), note);
                }
                const double tempo = timeline->tempoMap().tempo(i);

                // The first reading of the resampler, as the synthesis places it
                kit::PitchCurve::Timing timing;
                timing.preUtterance = timings[i].preUtterance;
                timing.startPoint = timings[i].startPoint;
                if (i + 1 < count) {
                    timing.nextPreUtterance = timings[i + 1].preUtterance;
                    timing.nextOverlap = timings[i + 1].voiceOverlap;
                }
                const auto readings = kit::PitchCurve({note}, 0, tempo).readingTicks(timing);
                if (readings.isEmpty()) {
                    continue;
                }
                const double firstReading = readings.first();

                // The places of the values within the stroke, from the first reading to the
                // end of the note, or for an erasure within the values
                const double origin =
                    hasValues ? m_roll->ticksOf(bend->start.value_or(0), i) : firstReading;
                double from = std::max(low - double(entry.start), firstReading);
                double to = std::min(high - double(entry.start), double(entry.length));
                if (m_erases) {
                    from = std::max(from, origin);
                    to = std::min(to, origin + BendInterval * double(bend->values.size() - 1));
                }
                auto k = qsizetype(std::ceil((from - origin) / BendInterval - 1e-9));
                QList<double> values;
                for (; origin + BendInterval * double(k) <= to + 1e-9 &&
                       origin + BendInterval * double(k) < double(entry.length);
                     ++k) {
                    if (m_erases) {
                        values.push_back(0);
                        continue;
                    }
                    const double at = origin + BendInterval * double(k);
                    const auto key = keyAt(double(entry.start) + at);
                    values.push_back(
                        key ? std::round((*key - entry.key - 0.5) * 100)
                            : std::round(kit::PitchBend::curveAt(bend, previous, at, tempo)));
                }
                if (values.isEmpty()) {
                    continue;
                }
                const auto firstK = k - values.size();
                double tick = origin + BendInterval * double(firstK);
                if (!hasValues) {
                    // From the first reading, the curve as it was up to the stroke
                    QList<double> before;
                    for (qsizetype j = 0; j < firstK; ++j) {
                        before.push_back(std::round(kit::PitchBend::curveAt(
                            bend, previous, origin + BendInterval * double(j), tempo)));
                    }
                    values = before + values;
                    tick = origin;
                }
                m_drawn.insert(i, {tick, values});
                m_roll->bendPreview.insert(
                    i, kit::PitchBend::drawn(bend, previous, tempo, tick, values));
            }
            m_roll->view->viewport()->update();
        }
    };

    std::unique_ptr<SceneGesture> PianoRoll::Impl::bendGesture(QPointF position,
                                                               Qt::MouseButton button) {
        finishEditing(true);
        return std::make_unique<BendGesture>(this, position, button == Qt::RightButton);
    }

    std::unique_ptr<SceneGesture>
        PianoRoll::Impl::PitchLayer::press(const SceneHit &hit, QPointF position,
                                           Qt::MouseButton button,
                                           Qt::KeyboardModifiers modifiers) {
        m_roll->finishEditing(true);
        const int index = m_roll->indexOf(hit.node);
        if (index < 0) {
            return nullptr;
        }
        if (hit.part != PitchPoint) {
            if (button != Qt::LeftButton) {
                return nullptr;
            }
            return std::make_unique<VibratoGesture>(m_roll, index, hit.part, position);
        }
        const auto list = m_roll->notes().at(index).portamento();
        if (hit.index < 0 || hit.index >= list.size()) {
            return nullptr;
        }
        const auto id = list.at(hit.index).id();
        if (button == Qt::RightButton) {
            if (!m_roll->selectedPoints.contains(id)) {
                m_roll->selectPoints({id});
            }
            showMenu(index, hit.index, position);
            return nullptr;
        }
        if (button != Qt::LeftButton) {
            return nullptr;
        }
        if (modifiers & Qt::ControlModifier) {
            auto ids = m_roll->selectedPoints;
            if (!ids.remove(id)) {
                ids.insert(id);
            }
            m_roll->selectPoints(ids);
            return nullptr;
        }
        if (!m_roll->selectedPoints.contains(id)) {
            m_roll->selectPoints({id});
        }
        return std::make_unique<PointGesture>(m_roll, index, hit.index, position);
    }

    std::optional<std::pair<int, double>> PianoRoll::Impl::portamentoNear(QPointF position) const {
        const int count = timeline->noteCount();
        if (!pointsShown() || count == 0) {
            return std::nullopt;
        }
        const auto &time = view->timeAxis();
        const auto &keys = view->keyAxis();
        const double tick = time.toTick(position.x());
        int index = timeline->noteAt(tick);
        if (index < 0 || index >= count) {
            return std::nullopt;
        }
        if (index + 1 < count) {
            const auto next = pointsOf(index + 1);
            if (!next.isEmpty() && tick >= double(timeline->note(index + 1).start) +
                                               ticksOf(next.first().x, index + 1)) {
                ++index;
            }
        }
        const auto &note = timeline->note(index);
        if (note.rest) {
            return std::nullopt;
        }

        // The portamento as it is drawn
        const int first = std::max(0, index - 2);
        const int last = std::min(count, index + 2);
        const auto refs = notes();
        QList<kit::Note> around;
        for (int i = first; i < last; ++i) {
            around.push_back(refs.at(i).toNote());
        }
        const double local = tick - double(note.start);
        const kit::PitchCurve curve(around, index - first, timeline->tempoMap().tempo(index));
        const double y = keys.toY(note.key + 0.5 + curve.portamentoAt(local) / 100);
        if (std::abs(y - position.y()) > curveGrip) {
            return std::nullopt;
        }
        return std::pair{index, local};
    }

    bool PianoRoll::Impl::insertPointAt(QPointF position) {
        const auto near = portamentoNear(position);
        if (!near) {
            return false;
        }
        const auto [index, local] = *near;
        const auto &note = timeline->note(index);
        const auto &keys = view->keyAxis();
        const auto refs = notes();

        auto points = pointsOf(index);
        if (points.isEmpty()) {
            kit::PortamentoPoint before;
            before.x = -DefaultPortamento;
            kit::PortamentoPoint after;
            after.x = DefaultPortamento;
            points = {before, after};
        }
        kit::PortamentoPoint point;
        point.x = std::round(millisecondsOf(local, index) * 10) / 10;
        point.y = std::round((keys.toKey(position.y()) - note.key - 0.5) * 100);
        int at = 0;
        while (at < points.size() && points[at].x <= point.x) {
            ++at;
        }
        points.insert(at, point);
        kit::DiagnosticList diagnostics;
        if (writePoints(PianoRoll::tr("Insert Pitch Point"),
                        {
                            {index, points}
        },
                        diagnostics)) {
            selectPoints({refs.at(index).portamento().at(at).id()});
        }
        report(diagnostics);
        return true;
    }

    std::unique_ptr<SceneGesture>
        PianoRoll::Impl::GridLayer::press(const SceneHit &hit, QPointF position,
                                          Qt::MouseButton button, Qt::KeyboardModifiers modifiers) {
        Q_UNUSED(hit);
        if (m_roll->drawsBend(button)) {
            return m_roll->bendGesture(position, button);
        }
        if (button != Qt::LeftButton) {
            return nullptr;
        }
        m_roll->finishEditing(true);
        if (m_roll->tool == PenTool) {
            m_roll->selectPoints({});
            return std::make_unique<DrawGesture>(m_roll, position, modifiers);
        }
        return std::make_unique<BandGesture>(m_roll, position, modifiers);
    }

    std::unique_ptr<SceneGesture>
        PianoRoll::Impl::NoteLayer::press(const SceneHit &hit, QPointF position,
                                          Qt::MouseButton button, Qt::KeyboardModifiers modifiers) {
        if (m_roll->drawsBend(button)) {
            return m_roll->bendGesture(position, button);
        }
        if (button != Qt::LeftButton) {
            return nullptr;
        }
        m_roll->finishEditing(true);
        m_roll->selectPoints({});
        const int index = m_roll->indexOf(hit.node);
        if (index < 0) {
            return nullptr;
        }
        if (hit.part == NoteEnd) {
            m_roll->selectOnly(index);
            return std::make_unique<LengthGesture>(m_roll, index);
        }
        if (modifiers & Qt::ControlModifier) {
            auto ids = m_roll->selection;
            const auto id = m_roll->timeline->note(index).id;
            if (!ids.remove(id)) {
                ids.insert(id);
            }
            m_roll->anchor = id;
            m_roll->setSelection(ids);
            return nullptr;
        }
        if (modifiers & Qt::ShiftModifier) {
            const int anchor = m_roll->indexOf(m_roll->anchor);
            m_roll->selectRange(anchor < 0 ? index : anchor, index);
            return nullptr;
        }
        const bool wasSelected = m_roll->isSelected(index);
        if (!wasSelected) {
            m_roll->selectOnly(index);
        }
        return std::make_unique<MoveGesture>(m_roll, index, position, wasSelected);
    }

    PianoRoll::PianoRoll(kit::ProjectSession *session, QWidget *parent)
        : QWidget(parent), _impl(std::make_unique<Impl>(this)) {
        stdc_impl_t;
        impl.session = session;
        impl.timeline = new kit::TrackTimeline(session, 0, this);
        impl.view = new SceneView();
        impl.ruler = new TimelineRuler(impl.view);
        impl.keyboard = new PianoKeyboard(impl.view);
        impl.ruler->setTicksPerBeat(kit::ticksPerQuarter);
        impl.ruler->setBeatsPerBar(BeatsPerBar);

        impl.view->addLayer(std::make_unique<Impl::GridLayer>(&impl));
        impl.view->addLayer(std::make_unique<Impl::NoteLayer>(&impl));
        impl.view->addLayer(std::make_unique<Impl::PitchLayer>(&impl));
        impl.view->addLayer(std::make_unique<Impl::OverlayLayer>(&impl));
        new PointerTracker(impl.view->viewport(), [this](std::optional<QPointF> position) {
            stdc_impl_t;
            impl.hover(position);
        });

        // The parameter area: the time axis of the roll, and volumes in percent for keys, all
        // of them in view
        impl.parameters = new SceneView();
        impl.parameters->setFixedHeight(ParameterHeight);
        impl.parameters->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        impl.parameters->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        impl.parameters->setKeyScaleRange(0.01, 100);
        // Room for the margins beyond the volumes
        impl.parameters->setKeyRange(-int(EnvelopeRange / 10), int(EnvelopeRange * 1.1));
        impl.parameters->addLayer(std::make_unique<Impl::EnvelopeLayer>(&impl));
        impl.parameters->addLayer(std::make_unique<Impl::ValueLayer>(&impl));

        // The buttons that choose what the parameter area shows, one above the other
        // (QSynthesis)
        impl.laneBar = new QWidget();
        impl.laneButtons = new QButtonGroup(this);
        auto laneLayout = new QVBoxLayout(impl.laneBar);
        laneLayout->setContentsMargins(2, 2, 2, 2);
        laneLayout->setSpacing(1);
        const struct {
            Lane lane;
            const char *text;
            const char *toolTip;
        } lanes[] = {
            {EnvelopeLane,   QT_TR_NOOP("Env"), QT_TR_NOOP("Envelope")  },
            {IntensityLane,  QT_TR_NOOP("Int"), QT_TR_NOOP("Intensity") },
            {ModulationLane, QT_TR_NOOP("Mod"), QT_TR_NOOP("Modulation")},
            {VelocityLane,   QT_TR_NOOP("Vel"), QT_TR_NOOP("Velocity")  },
        };
        for (const auto &lane : lanes) {
            auto button = new QToolButton();
            button->setText(tr(lane.text));
            button->setToolTip(tr(lane.toolTip));
            button->setCheckable(true);
            button->setChecked(lane.lane == impl.lane);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            impl.laneButtons->addButton(button, lane.lane);
            laneLayout->addWidget(button);
        }
        connect(impl.laneButtons, &QButtonGroup::idClicked, this,
                [this](int id) { setLane(Lane(id)); });
        new EventWatcher(impl.parameters->viewport(), [this](QEvent *event) {
            stdc_impl_t;
            if (event->type() == QEvent::Resize) {
                impl.fitParameters();
            }
        });
        connect(impl.parameters, &SceneView::keyAxisChanged, this, [this] {
            stdc_impl_t;
            impl.fitParameters();
        });
        connect(impl.view, &SceneView::timeAxisChanged, this, [this] {
            stdc_impl_t;
            impl.parameters->setTimeAxis(impl.view->timeAxis());
        });
        connect(impl.parameters, &SceneView::timeAxisChanged, this, [this] {
            stdc_impl_t;
            impl.view->setTimeAxis(impl.parameters->timeAxis());
        });

        impl.quantizer = new QComboBox();
        impl.quantizer->setToolTip(tr("Quantization"));
        impl.quantizer->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        for (const int ticks : quantizations()) {
            impl.quantizer->addItem(
                ticks > 0 ? QStringLiteral("1/%1").arg(BarTicks / ticks) : tr("Off"), ticks);
        }
        connect(impl.quantizer, &QComboBox::currentIndexChanged, this, [this](int index) {
            stdc_impl_t;
            impl.quantization = impl.quantizer->itemData(index).toInt();
        });
        setQuantization(DefaultQuantization);

        impl.editor = new LyricEditor(impl.view->viewport());
        impl.editor->hide();
        impl.editor->committed = [this] {
            stdc_impl_t;
            impl.finishEditing(true);
        };
        impl.editor->cancelled = [this] {
            stdc_impl_t;
            impl.finishEditing(false);
        };
        impl.editor->tabbed = [this](bool forward) {
            stdc_impl_t;
            impl.editNext(forward);
        };

        auto layout = new QGridLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(impl.quantizer, 0, 0);
        layout->addWidget(impl.ruler, 0, 1);
        layout->addWidget(impl.keyboard, 1, 0);
        layout->addWidget(impl.view, 1, 1);
        layout->addWidget(impl.laneBar, 2, 0);
        layout->addWidget(impl.parameters, 2, 1);
        layout->setColumnStretch(1, 1);
        layout->setRowStretch(1, 1);

        connect(impl.timeline, &kit::TrackTimeline::invalidated, this, [this] {
            stdc_impl_t;
            impl.timingsStale = true;
            impl.view->viewport()->update();
            impl.parameters->viewport()->update();
            impl.scheduleRefresh();
            // The note being edited may be gone, as may selected ones.
            if (impl.editing && impl.indexOf(impl.editing) < 0) {
                impl.finishEditing(false);
            }
            // With Mode2 off, the points are hidden and none is selected.
            if (impl.mode1()) {
                impl.selectedPoints.clear();
                impl.hovered = -1;
            }
            Q_EMIT selectionChanged();
        });
        // Scrolling and zooming move the note away from the editor.
        connect(impl.view, &SceneView::timeAxisChanged, this, [this] {
            stdc_impl_t;
            impl.finishEditing(true);
        });
        connect(impl.view, &SceneView::keyAxisChanged, this, [this] {
            stdc_impl_t;
            impl.finishEditing(true);
        });
        impl.refresh();
        scrollToNotes();
    }

    PianoRoll::~PianoRoll() = default;

    SceneView *PianoRoll::view() const {
        stdc_impl_t;
        return impl.view;
    }

    SceneView *PianoRoll::parameterView() const {
        stdc_impl_t;
        return impl.parameters;
    }

    PianoRoll::Lane PianoRoll::lane() const {
        stdc_impl_t;
        return impl.lane;
    }

    void PianoRoll::setLane(Lane lane) {
        stdc_impl_t;
        impl.laneButtons->button(lane)->setChecked(true);
        if (lane == impl.lane) {
            return;
        }
        impl.lane = lane;
        // Room for the margins beyond the values
        const auto range = Impl::rangeOf(lane);
        const double span = range.maximum - range.minimum;
        impl.parameters->setKeyRange(int(std::floor(range.minimum - span / 10)),
                                     int(std::ceil(range.maximum + span / 10)));
        impl.fitParameters();
        impl.parameters->viewport()->update();
    }

    TimelineRuler *PianoRoll::ruler() const {
        stdc_impl_t;
        return impl.ruler;
    }

    PianoKeyboard *PianoRoll::keyboard() const {
        stdc_impl_t;
        return impl.keyboard;
    }

    kit::TrackTimeline *PianoRoll::timeline() const {
        stdc_impl_t;
        return impl.timeline;
    }

    void PianoRoll::scrollToNotes() {
        stdc_impl_t;
        const auto timeline = impl.timeline;
        int lowest = 127;
        int highest = 0;
        for (int i = 0; i < timeline->noteCount(); ++i) {
            const auto &note = timeline->note(i);
            if (!note.rest) {
                lowest = std::min(lowest, note.key);
                highest = std::max(highest, note.key);
            }
        }
        // C4 when the track has no sung note
        const double middle = lowest <= highest ? (lowest + highest + 1) / 2.0 : 60.5;

        auto keys = impl.view->keyAxis();
        keys.top = middle + impl.view->viewport()->height() / keys.pixelsPerKey / 2;
        impl.view->setKeyAxis(keys);
        auto time = impl.view->timeAxis();
        time.left = 0;
        impl.view->setTimeAxis(time);
    }

    std::shared_ptr<const kit::VoiceBank> PianoRoll::voiceBank() const {
        stdc_impl_t;
        return impl.voiceBank;
    }

    void PianoRoll::setVoiceBank(std::shared_ptr<const kit::VoiceBank> bank) {
        stdc_impl_t;
        impl.voiceBank = std::move(bank);
        impl.timingsStale = true;
        impl.view->viewport()->update();
        impl.parameters->viewport()->update();
    }

    bool PianoRoll::lacksSample(int index) const {
        stdc_impl_t;
        const auto &bank = impl.voiceBank;
        if (!bank) {
            return false;
        }
        const auto &note = impl.timeline->note(index);
        return !note.rest && !bank->find(note.key, note.lyric);
    }

    PianoRoll::Tool PianoRoll::tool() const {
        stdc_impl_t;
        return impl.tool;
    }

    void PianoRoll::setTool(Tool tool) {
        stdc_impl_t;
        impl.tool = tool;
    }

    int PianoRoll::quantization() const {
        stdc_impl_t;
        return impl.quantization;
    }

    void PianoRoll::setQuantization(int ticks) {
        stdc_impl_t;
        const int index = impl.quantizer->findData(ticks);
        if (index >= 0) {
            impl.quantizer->setCurrentIndex(index);
        }
        impl.quantization = ticks;
    }

    QList<int> PianoRoll::quantizations() {
        return {BarTicks / 4, BarTicks / 8, BarTicks / 16, BarTicks / 32, BarTicks / 64, 0};
    }

    int PianoRoll::quantizedLength() const {
        stdc_impl_t;
        return impl.quantization > 0 ? impl.quantization : kit::ticksPerQuarter;
    }

    QComboBox *PianoRoll::quantizationBox() const {
        stdc_impl_t;
        return impl.quantizer;
    }

    QList<int> PianoRoll::selectedIndices() const {
        stdc_impl_t;
        QList<int> indices;
        if (impl.selection.isEmpty()) {
            return indices;
        }
        for (int i = 0; i < impl.timeline->noteCount(); ++i) {
            if (impl.isSelected(i)) {
                indices.push_back(i);
            }
        }
        return indices;
    }

    void PianoRoll::setSelectedIndices(const QList<int> &indices) {
        stdc_impl_t;
        QSet<kit::edit::NodeId> ids;
        for (const int index : indices) {
            ids.insert(impl.timeline->note(index).id);
        }
        if (!indices.isEmpty()) {
            impl.anchor = impl.timeline->note(indices.first()).id;
        }
        impl.setSelection(ids);
    }

    void PianoRoll::selectAll() {
        stdc_impl_t;
        if (impl.timeline->noteCount() > 0) {
            impl.selectRange(0, impl.timeline->noteCount() - 1);
        }
    }

    QList<std::pair<int, int>> PianoRoll::selectedPoints() const {
        stdc_impl_t;
        QList<std::pair<int, int>> result;
        const auto selected = impl.selectedPointIndices();
        for (auto it = selected.begin(); it != selected.end(); ++it) {
            for (const int j : it.value()) {
                result.push_back({it.key(), j});
            }
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    void PianoRoll::setSelectedPoints(const QList<std::pair<int, int>> &points) {
        stdc_impl_t;
        QSet<kit::edit::NodeId> ids;
        const auto refs = impl.notes();
        for (const auto &[index, j] : points) {
            if (index < 0 || index >= impl.timeline->noteCount()) {
                continue;
            }
            const auto list = refs.at(index).portamento();
            if (j >= 0 && j < list.size()) {
                ids.insert(list.at(j).id());
            }
        }
        impl.selectPoints(ids);
    }

    bool PianoRoll::removeSelected(kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        if (!impl.selectedPoints.isEmpty()) {
            return impl.removePoints(impl.selectedPointIndices(), diagnostics);
        }
        const auto indices = selectedIndices();
        if (indices.isEmpty()) {
            return true;
        }
        return kit::ProjectEdits::removeNotes(impl.notes(), indices, diagnostics);
    }

    bool PianoRoll::togglePortamento(kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        QList<int> sung;
        bool lacking = false;
        for (const int index : selectedIndices()) {
            if (impl.timeline->note(index).rest) {
                continue;
            }
            sung.push_back(index);
            lacking = lacking || impl.pointsOf(index).isEmpty();
        }
        if (sung.isEmpty()) {
            return true;
        }
        QHash<int, QList<kit::PortamentoPoint>> points;
        for (const int index : std::as_const(sung)) {
            if (!lacking) {
                points.insert(index, {});
            } else if (impl.pointsOf(index).isEmpty()) {
                kit::PortamentoPoint before;
                before.x = -DefaultPortamento;
                kit::PortamentoPoint after;
                after.x = DefaultPortamento;
                points.insert(index, {before, after});
            }
        }
        return impl.writePoints(lacking ? tr("Add Portamento") : tr("Remove Portamento"), points,
                                diagnostics);
    }

    bool PianoRoll::toggleVibrato(kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        const auto refs = impl.notes();
        QList<kit::NoteRef> sung;
        QList<kit::NoteRef> lacking;
        for (const int index : selectedIndices()) {
            if (impl.timeline->note(index).rest) {
                continue;
            }
            sung.push_back(refs.at(index));
            if (!sung.last().vibrato()) {
                lacking.push_back(sung.last());
            }
        }
        if (sung.isEmpty()) {
            return true;
        }
        auto transaction =
            impl.session->transaction(lacking.isEmpty() ? tr("Remove Vibrato") : tr("Add Vibrato"));
        if (lacking.isEmpty()) {
            kit::ProjectEdits::setVibrato(sung, std::nullopt, diagnostics);
        } else {
            kit::ProjectEdits::setVibrato(lacking, VibratoDialog::defaultVibrato(), diagnostics);
        }
        return transaction.commit(diagnostics);
    }

    bool PianoRoll::crossfadeEnvelopes(Crossfade crossfade, kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        const auto timeline = impl.timeline;
        const auto &timings = impl.sampleTimings();
        const auto refs = impl.notes();
        const int count = timeline->noteCount();
        // To a tenth of a millisecond, as the anchors that a drag moves
        const auto rounded = [](double value) { return std::round(value * 10) / 10; };

        auto transaction = impl.session->transaction(tr("Crossfade Envelopes"));
        for (const int index : selectedIndices()) {
            if (timeline->note(index).rest) {
                continue;
            }
            auto anchors = impl.envelopeOf(index).anchorsInTimeOrder();
            const int last = int(anchors.size()) - 1;
            bool changed = false;
            if (index > 0 && !timeline->note(index - 1).rest && timings[index].voiceOverlap > 0) {
                const double overlap = rounded(timings[index].voiceOverlap);
                if (crossfade == CrossfadeP2P3) {
                    anchors[0].x = 0;
                    anchors[1].x = overlap;
                } else {
                    anchors[0].x = overlap;
                    anchors[0].y = anchors[1].y;
                    anchors[1].x = 5;
                }
                changed = true;
            }
            if (index + 1 < count && !timeline->note(index + 1).rest &&
                timings[index + 1].voiceOverlap > 0) {
                const double overlap = rounded(timings[index + 1].voiceOverlap);
                if (crossfade == CrossfadeP2P3) {
                    anchors[last].x = 0;
                    anchors[last - 1].x = overlap;
                } else {
                    anchors[last].x = overlap;
                    anchors[last].y = anchors[last - 1].y;
                    anchors[last - 1].x = 5;
                }
                changed = true;
            }
            if (!changed) {
                continue;
            }
            if (anchors.size() == 5) {
                anchors.removeAt(2);
            }
            kit::ProjectEdits::setEnvelope({refs.at(index)}, kit::Envelope::fromTimeOrder(anchors),
                                           diagnostics);
        }
        return transaction.commit(diagnostics);
    }

    bool PianoRoll::copySelected() {
        stdc_impl_t;
        const auto indices = selectedIndices();
        if (indices.isEmpty()) {
            return false;
        }
        const auto refs = impl.notes();
        QJsonArray notes;
        for (const int index : indices) {
            notes.append(refs.at(index).toNote().toJson());
        }
        const auto bytes = QJsonDocument(QJsonObject{
                                             {QLatin1String("notes"), notes}
        })
                               .toJson(QJsonDocument::Compact);
        auto data = new QMimeData();
        data->setData(QLatin1String(NotesMimeType), bytes);
        data->setText(QString::fromUtf8(bytes));
        QGuiApplication::clipboard()->setMimeData(data);
        return true;
    }

    QList<kit::Note> PianoRoll::copiedNotes() {
        const auto data = QGuiApplication::clipboard()->mimeData();
        if (!data || !data->hasFormat(QLatin1String(NotesMimeType))) {
            return {};
        }
        const auto document = QJsonDocument::fromJson(data->data(QLatin1String(NotesMimeType)));
        QList<kit::Note> notes;
        kit::DiagnosticList diagnostics;
        for (const auto &value : document.object().value(QLatin1String("notes")).toArray()) {
            if (const auto note = kit::Note::fromJson(value.toObject(), diagnostics)) {
                notes.push_back(*note);
            }
        }
        return notes;
    }

    bool PianoRoll::pasteParameters(Parameters parameters, kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        QList<kit::Note> sources;
        for (const auto &note : copiedNotes()) {
            if (!note.isRest()) {
                sources.push_back(note);
            }
        }
        QList<int> targets;
        for (const int index : selectedIndices()) {
            if (!impl.timeline->note(index).rest) {
                targets.push_back(index);
            }
        }
        if (sources.isEmpty() || targets.isEmpty() || !parameters) {
            return true;
        }

        const auto refs = impl.notes();
        const bool one = sources.size() == 1;
        const auto count = one ? targets.size() : std::min(sources.size(), targets.size());
        auto transaction = impl.session->transaction(tr("Paste Parameters"));
        for (qsizetype k = 0; k < count; ++k) {
            const auto &source = sources[one ? 0 : k];
            const auto target = refs.at(targets[k]);
            if (parameters & PortamentoParameter) {
                kit::ProjectEdits::setPortamento(target, source.portamento, diagnostics);
            }
            if (parameters & VibratoParameter) {
                kit::ProjectEdits::setVibrato({target}, source.vibrato, diagnostics);
            }
            if (parameters & EnvelopeParameter) {
                kit::ProjectEdits::setEnvelope({target}, source.envelope, diagnostics);
            }
        }
        return transaction.commit(diagnostics);
    }

    bool PianoRoll::resetParameters(Parameters parameters, kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        // The selected notes, those of the selected points, or all
        auto indices = selectedIndices();
        if (indices.isEmpty()) {
            indices = impl.selectedPointIndices().keys();
        }
        if (indices.isEmpty()) {
            indices = impl.identityOrder();
        }
        const auto refs = impl.notes();
        QList<kit::NoteRef> sung;
        for (const int index : std::as_const(indices)) {
            if (!impl.timeline->note(index).rest) {
                sung.push_back(refs.at(index));
            }
        }
        if (sung.isEmpty() || !parameters) {
            return true;
        }

        auto transaction = impl.session->transaction(tr("Reset Parameters"));
        if (parameters & PortamentoParameter) {
            for (const auto &note : std::as_const(sung)) {
                kit::ProjectEdits::setPortamento(note, {}, diagnostics);
            }
        }
        if (parameters & VibratoParameter) {
            kit::ProjectEdits::setVibrato(sung, std::nullopt, diagnostics);
        }
        if (parameters & EnvelopeParameter) {
            kit::ProjectEdits::setEnvelope(sung, std::nullopt, diagnostics);
        }
        return transaction.commit(diagnostics);
    }

    bool PianoRoll::scalePitch(double portamento, double vibrato,
                               kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        const auto refs = impl.notes();
        QList<kit::NoteRef> sung;
        for (const int index : selectedIndices()) {
            if (!impl.timeline->note(index).rest) {
                sung.push_back(refs.at(index));
            }
        }
        return kit::ProjectEdits::scalePitch(sung, portamento, vibrato, diagnostics);
    }

    bool PianoRoll::transposeSelected(int semitones, kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        const auto notes = impl.notes();
        QList<kit::NoteRef> refs;
        for (const int index : selectedIndices()) {
            refs.push_back(notes.at(index));
        }
        return kit::ProjectEdits::transpose(refs, semitones, diagnostics);
    }

    bool PianoRoll::insertNote(kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        const auto indices = selectedIndices();
        const auto timeline = impl.timeline;
        const int count = timeline->noteCount();
        const int index = indices.isEmpty() ? count : indices.first();

        kit::Note note;
        note.lyric = QString::fromLatin1(kit::defaultLyric);
        note.length = quantizedLength();
        note.noteNum = index < count ? timeline->note(index).key
                       : count > 0   ? timeline->note(count - 1).key
                                     : 60;
        const auto notes = impl.notes();
        if (!kit::ProjectEdits::insertNotes(notes, index, {note}, diagnostics)) {
            return false;
        }
        impl.anchor = notes.at(index).id();
        impl.setSelection({impl.anchor});
        return true;
    }

    bool PianoRoll::pasteNotes(kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        const auto copied = copiedNotes();
        if (copied.isEmpty()) {
            return true;
        }
        const auto indices = selectedIndices();
        const int index = indices.isEmpty() ? impl.timeline->noteCount() : indices.first();
        const auto notes = impl.notes();
        auto transaction = impl.session->transaction(tr("Paste"));
        kit::ProjectEdits::insertNotes(notes, index, copied, diagnostics);
        QSet<kit::edit::NodeId> pasted;
        for (int i = 0; i < copied.size(); ++i) {
            pasted.insert(notes.at(index + i).id());
        }
        const auto first = notes.at(index).id();
        if (!transaction.commit(diagnostics)) {
            return false;
        }
        impl.anchor = first;
        impl.setSelection(pasted);
        return true;
    }

    void PianoRoll::editLyric(int index) {
        stdc_impl_t;
        impl.startEditing(index);
    }

    QLineEdit *PianoRoll::lyricEditor() const {
        stdc_impl_t;
        return impl.editor;
    }

    std::optional<double> PianoRoll::playheadPosition() const {
        stdc_impl_t;
        return impl.playhead;
    }

    void PianoRoll::setPlayheadPosition(std::optional<double> tick) {
        stdc_impl_t;
        if (tick == impl.playhead) {
            return;
        }
        impl.playhead = tick;
        if (tick) {
            // A page at a time, so that the view does not move under the pointer continuously
            auto time = impl.view->timeAxis();
            const double width = impl.view->viewport()->width();
            const double x = time.toX(*tick);
            if (x < 0 || x > width) {
                time.left = *tick - width / time.pixelsPerTick * FollowMargin;
                impl.view->setTimeAxis(time);
            }
        }
        impl.view->viewport()->update();
    }

    bool PianoRoll::isPitchVisible() const {
        stdc_impl_t;
        return impl.pitchVisible;
    }

    void PianoRoll::setPitchVisible(bool visible) {
        stdc_impl_t;
        impl.pitchVisible = visible;
        impl.view->viewport()->update();
    }

    double PianoRoll::pointGrip() const {
        stdc_impl_t;
        return impl.pointGrip;
    }

    void PianoRoll::setPointGrip(double pixels) {
        stdc_impl_t;
        impl.pointGrip = std::max(0.0, pixels);
    }

    double PianoRoll::curveGrip() const {
        stdc_impl_t;
        return impl.curveGrip;
    }

    void PianoRoll::setCurveGrip(double pixels) {
        stdc_impl_t;
        impl.curveGrip = std::max(0.0, pixels);
    }

    void PianoRoll::keyPressEvent(QKeyEvent *event) {
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
            event->modifiers() == Qt::NoModifier) {
            const auto indices = selectedIndices();
            if (!indices.isEmpty()) {
                editLyric(indices.first());
                event->accept();
                return;
            }
        }
        QWidget::keyPressEvent(event);
    }

    QColor PianoRoll::noteColor() const {
        stdc_impl_t;
        return impl.noteColor.isValid() ? impl.noteColor : palette().color(QPalette::Highlight);
    }

    void PianoRoll::setNoteColor(const QColor &color) {
        stdc_impl_t;
        impl.noteColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::restColor() const {
        stdc_impl_t;
        if (impl.restColor.isValid()) {
            return impl.restColor;
        }
        auto color = palette().color(QPalette::Mid);
        color.setAlphaF(0.4f);
        return color;
    }

    void PianoRoll::setRestColor(const QColor &color) {
        stdc_impl_t;
        impl.restColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::lyricColor() const {
        stdc_impl_t;
        return impl.lyricColor.isValid() ? impl.lyricColor
                                         : palette().color(QPalette::HighlightedText);
    }

    void PianoRoll::setLyricColor(const QColor &color) {
        stdc_impl_t;
        impl.lyricColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::unsampledColor() const {
        stdc_impl_t;
        return impl.unsampledColor.isValid() ? impl.unsampledColor
                                             : palette().color(QPalette::Highlight);
    }

    void PianoRoll::setUnsampledColor(const QColor &color) {
        stdc_impl_t;
        impl.unsampledColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::unsampledLyricColor() const {
        stdc_impl_t;
        return impl.unsampledLyricColor.isValid() ? impl.unsampledLyricColor
                                                  : palette().color(QPalette::Text);
    }

    void PianoRoll::setUnsampledLyricColor(const QColor &color) {
        stdc_impl_t;
        impl.unsampledLyricColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::selectionColor() const {
        stdc_impl_t;
        return impl.selectionColor.isValid() ? impl.selectionColor
                                             : palette().color(QPalette::WindowText);
    }

    void PianoRoll::setSelectionColor(const QColor &color) {
        stdc_impl_t;
        impl.selectionColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::pitchColor() const {
        stdc_impl_t;
        return impl.pitchColor.isValid() ? impl.pitchColor : palette().color(QPalette::Text);
    }

    void PianoRoll::setPitchColor(const QColor &color) {
        stdc_impl_t;
        impl.pitchColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::vibratoColor() const {
        stdc_impl_t;
        if (impl.vibratoColor.isValid()) {
            return impl.vibratoColor;
        }
        auto color = palette().color(QPalette::Text);
        color.setAlphaF(0.5f);
        return color;
    }

    void PianoRoll::setVibratoColor(const QColor &color) {
        stdc_impl_t;
        impl.vibratoColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::envelopeColor() const {
        stdc_impl_t;
        return impl.envelopeColor.isValid() ? impl.envelopeColor
                                            : palette().color(QPalette::Highlight);
    }

    void PianoRoll::setEnvelopeColor(const QColor &color) {
        stdc_impl_t;
        impl.envelopeColor = color;
        impl.parameters->viewport()->update();
    }

    QColor PianoRoll::faintPointColor() const {
        stdc_impl_t;
        return impl.faintPointColor.isValid() ? impl.faintPointColor
                                              : palette().color(QPalette::Mid);
    }

    void PianoRoll::setFaintPointColor(const QColor &color) {
        stdc_impl_t;
        impl.faintPointColor = color;
        impl.view->viewport()->update();
        impl.parameters->viewport()->update();
    }

    QColor PianoRoll::parameterColor() const {
        stdc_impl_t;
        return impl.parameterColor.isValid() ? impl.parameterColor
                                             : palette().color(QPalette::Highlight);
    }

    void PianoRoll::setParameterColor(const QColor &color) {
        stdc_impl_t;
        impl.parameterColor = color;
        impl.parameters->viewport()->update();
    }

    QColor PianoRoll::playheadColor() const {
        stdc_impl_t;
        return impl.playheadColor.isValid() ? impl.playheadColor : palette().color(QPalette::Link);
    }

    void PianoRoll::setPlayheadColor(const QColor &color) {
        stdc_impl_t;
        impl.playheadColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::whiteRowColor() const {
        stdc_impl_t;
        return impl.whiteRowColor.isValid() ? impl.whiteRowColor : palette().color(QPalette::Base);
    }

    void PianoRoll::setWhiteRowColor(const QColor &color) {
        stdc_impl_t;
        impl.whiteRowColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::blackRowColor() const {
        stdc_impl_t;
        return impl.blackRowColor.isValid() ? impl.blackRowColor
                                            : palette().color(QPalette::AlternateBase);
    }

    void PianoRoll::setBlackRowColor(const QColor &color) {
        stdc_impl_t;
        impl.blackRowColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::lineColor() const {
        stdc_impl_t;
        if (impl.lineColor.isValid()) {
            return impl.lineColor;
        }
        auto color = palette().color(QPalette::Mid);
        color.setAlphaF(0.35f);
        return color;
    }

    void PianoRoll::setLineColor(const QColor &color) {
        stdc_impl_t;
        impl.lineColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::barLineColor() const {
        stdc_impl_t;
        return impl.barLineColor.isValid() ? impl.barLineColor : palette().color(QPalette::Mid);
    }

    void PianoRoll::setBarLineColor(const QColor &color) {
        stdc_impl_t;
        impl.barLineColor = color;
        impl.view->viewport()->update();
    }

}
