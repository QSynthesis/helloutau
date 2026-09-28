#include "PianoRoll.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>

#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>

#include <hellokit/Document/DocumentConstants.h>
#include <hellokit/Edit/ProjectEdits.h>
#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Edit/ProjectSession.h>
#include <hellokit/Edit/TrackTimeline.h>
#include <hellokit/Synth/PitchCurve.h>
#include <hellokit/VoiceBank/VoiceBank.h>

#include <helloutau/Widgets/PianoKeyboard.h>
#include <helloutau/Widgets/SceneView.h>
#include <helloutau/Widgets/TimelineRuler.h>

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

        // Ctrl snaps the height of a point to this many cents.
        constexpr double PitchSnap = 50;

        // A note without points receives these, at this many milliseconds on either side of
        // its start, when a point is inserted.
        constexpr double DefaultPortamento = 15;

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
        class GridLayer;
        class NoteLayer;
        class PitchLayer;
        class OverlayLayer;
        class MoveGesture;
        class LengthGesture;
        class BandGesture;
        class DrawGesture;
        class PointGesture;

        explicit Impl(PianoRoll *decl) : _decl(decl) {
        }

        PianoRoll *_decl;
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
            const bool clearsPoints = !ids.isEmpty() && !selectedPoints.isEmpty();
            if (ids == selection && !clearsPoints) {
                return;
            }
            selection = ids;
            if (clearsPoints) {
                selectedPoints.clear();
            }
            view->viewport()->update();
            Q_EMIT _decl->selectionChanged();
        }

        // Selects the points ids; selecting a point clears the selected notes.
        void selectPoints(const QSet<kit::edit::NodeId> &ids) {
            const bool clearsNotes = !ids.isEmpty() && !selection.isEmpty();
            if (ids == selectedPoints && !clearsNotes) {
                return;
            }
            selectedPoints = ids;
            if (clearsNotes) {
                selection.clear();
            }
            view->viewport()->update();
            Q_EMIT _decl->selectionChanged();
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

        // Writes the points of several notes, by index, in one step
        bool writePoints(const QString &message,
                         const QHash<int, QList<kit::PortamentoPoint>> &points,
                         kit::DiagnosticList &diagnostics) {
            auto transaction = session->transaction(message);
            const auto refs = notes();
            for (auto it = points.begin(); it != points.end(); ++it) {
                kit::ProjectEdits::setPortamento(refs.at(it.key()), it.value(), diagnostics);
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

        // Inserts a point where position lies on the portamento of a note, and selects it.
        // The note is the one at that time, or the next one from its first point on.
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
            if (refreshPending) {
                return;
            }
            refreshPending = true;
            QTimer::singleShot(0, _decl, [this] { refresh(); });
        }

        void refresh() {
            refreshPending = false;
            const auto bars =
                std::max<qint64>(MinimumBars, timeline->length() / BarTicks + 1 + TrailingBars);
            view->setTickRange(0, double(bars * BarTicks));

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
            transaction.commit();
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
            if (m_roll->tool == PenTool &&
                view()->timeAxis().toTick(position.x()) >= double(m_roll->timeline->length())) {
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
            if (rect.width() >= MinimumGripWidth && position.x() >= rect.right() - EndGrip) {
                hit.part = NoteEnd;
                hit.cursor = Qt::SizeHorCursor;
            }
            return hit;
        }

        std::unique_ptr<SceneGesture> press(const SceneHit &hit, QPointF position,
                                            Qt::MouseButton button,
                                            Qt::KeyboardModifiers modifiers) override;

        bool doubleClick(const SceneHit &hit, QPointF position) override {
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

            // The points, also those of the next note, which may lie before it: filled where
            // they only move in time, and in the selection color where selected
            for (int i = begin; i < last; ++i) {
                if (timeline->note(i).rest) {
                    continue;
                }
                const auto &points = notes.at(i - first).portamento;
                const auto list = refs.at(i).portamento();
                for (int j = 0; j < points.size(); ++j) {
                    const bool selected =
                        j < list.size() && m_roll->selectedPoints.contains(list.at(j).id());
                    const bool fixed = m_roll->heightFixed(i, j, int(points.size()));
                    painter.setPen(
                        QPen(selected ? decl->selectionColor() : decl->pitchColor(), 1.5));
                    painter.setBrush(selected
                                         ? decl->selectionColor()
                                         : (fixed ? decl->pitchColor() : decl->whiteRowColor()));
                    painter.drawEllipse(m_roll->positionOf(i, j, points[j]), PointRadius,
                                        PointRadius);
                }
            }
        }

        std::optional<SceneHit> hitTest(QPointF position) const override {
            if (!m_roll->pitchVisible || !m_roll->placements.isEmpty()) {
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
            return nearest;
        }

        std::unique_ptr<SceneGesture> press(const SceneHit &hit, QPointF position,
                                            Qt::MouseButton button,
                                            Qt::KeyboardModifiers modifiers) override;

    private:
        PianoRoll::Impl *m_roll;

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
            : m_roll(roll), m_origin(position), m_previous(roll->selection) {
            if (modifiers & Qt::ControlModifier) {
                m_base = roll->selection;
            }
            roll->setSelection(m_base);
        }

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override {
            Q_UNUSED(modifiers);
            const auto rect = QRectF(m_origin, position).normalized();
            const auto timeline = m_roll->timeline;
            const auto &time = m_roll->view->timeAxis();
            auto ids = m_base;
            const auto [begin, end] =
                timeline->notesBetween(time.toTick(rect.left()), time.toTick(rect.right()));
            for (int i = begin; i < end; ++i) {
                const auto &note = timeline->note(i);
                if (m_roll->rectOf(note.start, note.length, note.key).intersects(rect)) {
                    ids.insert(note.id);
                }
            }
            m_roll->band = rect;
            m_roll->setSelection(ids);
            m_roll->view->viewport()->update();
        }

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override {
            move(position, modifiers);
            m_roll->clearPreview();
        }

        void cancel() override {
            m_roll->clearPreview();
            m_roll->setSelection(m_previous);
        }

    private:
        PianoRoll::Impl *m_roll;
        QPointF m_origin;
        QSet<kit::edit::NodeId> m_base;
        QSet<kit::edit::NodeId> m_previous;
    };

    // A drag of the pen after the last note, which draws a note there. A gap before it is filled
    // with a rest.
    class PianoRoll::Impl::DrawGesture : public SceneGesture {
    public:
        DrawGesture(PianoRoll::Impl *roll, QPointF position, Qt::KeyboardModifiers modifiers)
            : m_roll(roll), m_end(roll->timeline->length()) {
            const auto &keys = roll->view->keyAxis();
            m_key = std::clamp(keys.keyAt(position.y()), kit::lowestNoteNum, kit::highestNoteNum);
            const double tick = roll->view->timeAxis().toTick(position.x());
            m_start = std::max(m_end, roll->snappedDown(tick, modifiers));
            update(roll->_decl->quantizedLength());
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
            int index = notes.size();
            auto transaction = m_roll->session->transaction(PianoRoll::tr("Insert Note"));
            kit::DiagnosticList diagnostics;
            if (m_start > m_end) {
                kit::Note rest;
                rest.lyric = QString::fromLatin1(kit::restLyric);
                rest.length = int(m_start - m_end);
                rest.noteNum = m_key;
                kit::ProjectEdits::insertNote(notes, index++, rest, diagnostics);
            }
            kit::Note note;
            note.lyric = QString::fromLatin1(kit::defaultLyric);
            note.length = m_length;
            note.noteNum = m_key;
            kit::ProjectEdits::insertNote(notes, index, note, diagnostics);
            const auto id = notes.at(index).id();
            if (transaction.commit(diagnostics)) {
                m_roll->anchor = id;
                m_roll->setSelection({id});
            }
        }

        void cancel() override {
            m_roll->clearPreview();
        }

    private:
        PianoRoll::Impl *m_roll;
        qint64 m_end;
        qint64 m_start = 0;
        int m_key = 0;
        int m_length = 0;

        void update(int length) {
            m_length = length;
            m_roll->drawn = Placement{-1, m_start, m_length, m_key};
            m_roll->view->viewport()->update();
        }
    };

    // A drag of the selected points, all by the same time and height. No point passes a
    // neighbour that stays, and the heights that are fixed stay (heightFixed()). Shift snaps the
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
            for (auto it = m_moving.begin(); it != m_moving.end(); ++it) {
                const int index = it.key();
                const auto &points = m_original[index];
                for (const int j : it.value()) {
                    if (j > 0 && !it.value().contains(j - 1)) {
                        ticks =
                            std::max(ticks, m_roll->ticksOf(points[j - 1].x - points[j].x, index));
                    }
                    if (j + 1 < points.size() && !it.value().contains(j + 1)) {
                        ticks =
                            std::min(ticks, m_roll->ticksOf(points[j + 1].x - points[j].x, index));
                    }
                }
            }

            m_roll->pointPreview.clear();
            for (auto it = m_moving.begin(); it != m_moving.end(); ++it) {
                const int index = it.key();
                auto points = m_original[index];
                const int count = int(points.size());
                for (const int j : it.value()) {
                    // To a tenth of a millisecond and a cent, within the neighbours that stay
                    auto x =
                        std::round((points[j].x + m_roll->millisecondsOf(ticks, index)) * 10) / 10;
                    if (j > 0 && !it.value().contains(j - 1)) {
                        x = std::max(x, points[j - 1].x);
                    }
                    if (j + 1 < count && !it.value().contains(j + 1)) {
                        x = std::min(x, points[j + 1].x);
                    }
                    points[j].x = x;
                    if (!m_roll->heightFixed(index, j, count)) {
                        points[j].y = std::round(points[j].y + cents);
                    }
                }
                m_roll->pointPreview.insert(index, points);
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
            if (!changed.isEmpty()) {
                kit::DiagnosticList diagnostics;
                m_roll->writePoints(PianoRoll::tr("Move Pitch Points"), changed, diagnostics);
            }
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

        void start() {
            m_dragging = true;
            m_moving = m_roll->selectedPointIndices();
            m_moving[m_index].insert(m_point);
            for (auto it = m_moving.begin(); it != m_moving.end(); ++it) {
                m_original.insert(it.key(), m_roll->pointsOf(it.key()));
            }
        }
    };

    std::unique_ptr<SceneGesture>
        PianoRoll::Impl::PitchLayer::press(const SceneHit &hit, QPointF position,
                                           Qt::MouseButton button,
                                           Qt::KeyboardModifiers modifiers) {
        m_roll->finishEditing(true);
        const int index = m_roll->indexOf(hit.node);
        if (index < 0) {
            return nullptr;
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

    bool PianoRoll::Impl::insertPointAt(QPointF position) {
        const int count = timeline->noteCount();
        if (!pitchVisible || count == 0) {
            return false;
        }
        const auto &time = view->timeAxis();
        const auto &keys = view->keyAxis();
        const double tick = time.toTick(position.x());
        int index = timeline->noteAt(tick);
        if (index < 0 || index >= count) {
            return false;
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
            return false;
        }

        // Only on the portamento as it is drawn
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
            return false;
        }

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
        return true;
    }

    std::unique_ptr<SceneGesture>
        PianoRoll::Impl::GridLayer::press(const SceneHit &hit, QPointF position,
                                          Qt::MouseButton button, Qt::KeyboardModifiers modifiers) {
        Q_UNUSED(hit);
        if (button != Qt::LeftButton) {
            return nullptr;
        }
        m_roll->finishEditing(true);
        m_roll->selectPoints({});
        if (m_roll->tool == PenTool &&
            view()->timeAxis().toTick(position.x()) >= double(m_roll->timeline->length())) {
            return std::make_unique<DrawGesture>(m_roll, position, modifiers);
        }
        return std::make_unique<BandGesture>(m_roll, position, modifiers);
    }

    std::unique_ptr<SceneGesture>
        PianoRoll::Impl::NoteLayer::press(const SceneHit &hit, QPointF position,
                                          Qt::MouseButton button, Qt::KeyboardModifiers modifiers) {
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
        _impl->session = session;
        _impl->timeline = new kit::TrackTimeline(session, 0, this);
        _impl->view = new SceneView();
        _impl->ruler = new TimelineRuler(_impl->view);
        _impl->keyboard = new PianoKeyboard(_impl->view);
        _impl->ruler->setTicksPerBeat(kit::ticksPerQuarter);
        _impl->ruler->setBeatsPerBar(BeatsPerBar);

        _impl->view->addLayer(std::make_unique<Impl::GridLayer>(_impl.get()));
        _impl->view->addLayer(std::make_unique<Impl::NoteLayer>(_impl.get()));
        _impl->view->addLayer(std::make_unique<Impl::PitchLayer>(_impl.get()));
        _impl->view->addLayer(std::make_unique<Impl::OverlayLayer>(_impl.get()));

        _impl->quantizer = new QComboBox();
        _impl->quantizer->setToolTip(tr("Quantization"));
        _impl->quantizer->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        for (const int ticks : quantizations()) {
            _impl->quantizer->addItem(
                ticks > 0 ? QStringLiteral("1/%1").arg(BarTicks / ticks) : tr("Off"), ticks);
        }
        connect(_impl->quantizer, &QComboBox::currentIndexChanged, this, [this](int index) {
            _impl->quantization = _impl->quantizer->itemData(index).toInt();
        });
        setQuantization(DefaultQuantization);

        _impl->editor = new LyricEditor(_impl->view->viewport());
        _impl->editor->hide();
        _impl->editor->committed = [this] { _impl->finishEditing(true); };
        _impl->editor->cancelled = [this] { _impl->finishEditing(false); };
        _impl->editor->tabbed = [this](bool forward) { _impl->editNext(forward); };

        auto layout = new QGridLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(_impl->quantizer, 0, 0);
        layout->addWidget(_impl->ruler, 0, 1);
        layout->addWidget(_impl->keyboard, 1, 0);
        layout->addWidget(_impl->view, 1, 1);
        layout->setColumnStretch(1, 1);
        layout->setRowStretch(1, 1);

        connect(_impl->timeline, &kit::TrackTimeline::invalidated, this, [this] {
            _impl->view->viewport()->update();
            _impl->scheduleRefresh();
            // The note being edited may be gone, as may selected ones.
            if (_impl->editing && _impl->indexOf(_impl->editing) < 0) {
                _impl->finishEditing(false);
            }
            Q_EMIT selectionChanged();
        });
        // Scrolling and zooming move the note away from the editor.
        connect(_impl->view, &SceneView::timeAxisChanged, this,
                [this] { _impl->finishEditing(true); });
        connect(_impl->view, &SceneView::keyAxisChanged, this,
                [this] { _impl->finishEditing(true); });
        _impl->refresh();
        scrollToNotes();
    }

    PianoRoll::~PianoRoll() = default;

    SceneView *PianoRoll::view() const {
        return _impl->view;
    }

    TimelineRuler *PianoRoll::ruler() const {
        return _impl->ruler;
    }

    PianoKeyboard *PianoRoll::keyboard() const {
        return _impl->keyboard;
    }

    kit::TrackTimeline *PianoRoll::timeline() const {
        return _impl->timeline;
    }

    void PianoRoll::scrollToNotes() {
        const auto timeline = _impl->timeline;
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

        auto keys = _impl->view->keyAxis();
        keys.top = middle + _impl->view->viewport()->height() / keys.pixelsPerKey / 2;
        _impl->view->setKeyAxis(keys);
        auto time = _impl->view->timeAxis();
        time.left = 0;
        _impl->view->setTimeAxis(time);
    }

    std::shared_ptr<const kit::VoiceBank> PianoRoll::voiceBank() const {
        return _impl->voiceBank;
    }

    void PianoRoll::setVoiceBank(std::shared_ptr<const kit::VoiceBank> bank) {
        _impl->voiceBank = std::move(bank);
        _impl->view->viewport()->update();
    }

    bool PianoRoll::lacksSample(int index) const {
        const auto &bank = _impl->voiceBank;
        if (!bank) {
            return false;
        }
        const auto &note = _impl->timeline->note(index);
        return !note.rest && !bank->find(note.key, note.lyric);
    }

    PianoRoll::Tool PianoRoll::tool() const {
        return _impl->tool;
    }

    void PianoRoll::setTool(Tool tool) {
        _impl->tool = tool;
    }

    int PianoRoll::quantization() const {
        return _impl->quantization;
    }

    void PianoRoll::setQuantization(int ticks) {
        const int index = _impl->quantizer->findData(ticks);
        if (index >= 0) {
            _impl->quantizer->setCurrentIndex(index);
        }
        _impl->quantization = ticks;
    }

    QList<int> PianoRoll::quantizations() {
        return {BarTicks / 4, BarTicks / 8, BarTicks / 16, BarTicks / 32, BarTicks / 64, 0};
    }

    int PianoRoll::quantizedLength() const {
        return _impl->quantization > 0 ? _impl->quantization : kit::ticksPerQuarter;
    }

    QComboBox *PianoRoll::quantizationBox() const {
        return _impl->quantizer;
    }

    QList<int> PianoRoll::selectedIndices() const {
        QList<int> indices;
        if (_impl->selection.isEmpty()) {
            return indices;
        }
        for (int i = 0; i < _impl->timeline->noteCount(); ++i) {
            if (_impl->isSelected(i)) {
                indices.push_back(i);
            }
        }
        return indices;
    }

    void PianoRoll::setSelectedIndices(const QList<int> &indices) {
        QSet<kit::edit::NodeId> ids;
        for (const int index : indices) {
            ids.insert(_impl->timeline->note(index).id);
        }
        if (!indices.isEmpty()) {
            _impl->anchor = _impl->timeline->note(indices.first()).id;
        }
        _impl->setSelection(ids);
    }

    void PianoRoll::selectAll() {
        if (_impl->timeline->noteCount() > 0) {
            _impl->selectRange(0, _impl->timeline->noteCount() - 1);
        }
    }

    QList<std::pair<int, int>> PianoRoll::selectedPoints() const {
        QList<std::pair<int, int>> result;
        const auto selected = _impl->selectedPointIndices();
        for (auto it = selected.begin(); it != selected.end(); ++it) {
            for (const int j : it.value()) {
                result.push_back({it.key(), j});
            }
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    void PianoRoll::setSelectedPoints(const QList<std::pair<int, int>> &points) {
        QSet<kit::edit::NodeId> ids;
        const auto refs = _impl->notes();
        for (const auto &[index, j] : points) {
            if (index < 0 || index >= _impl->timeline->noteCount()) {
                continue;
            }
            const auto list = refs.at(index).portamento();
            if (j >= 0 && j < list.size()) {
                ids.insert(list.at(j).id());
            }
        }
        _impl->selectPoints(ids);
    }

    bool PianoRoll::removeSelected(kit::DiagnosticList &diagnostics) {
        if (!_impl->selectedPoints.isEmpty()) {
            return _impl->removePoints(_impl->selectedPointIndices(), diagnostics);
        }
        const auto indices = selectedIndices();
        if (indices.isEmpty()) {
            return true;
        }
        return kit::ProjectEdits::removeNotes(_impl->notes(), indices, diagnostics);
    }

    bool PianoRoll::transposeSelected(int semitones, kit::DiagnosticList &diagnostics) {
        const auto notes = _impl->notes();
        QList<kit::NoteRef> refs;
        for (const int index : selectedIndices()) {
            refs.push_back(notes.at(index));
        }
        return kit::ProjectEdits::transpose(refs, semitones, diagnostics);
    }

    bool PianoRoll::insertNote(kit::DiagnosticList &diagnostics) {
        const auto indices = selectedIndices();
        const auto timeline = _impl->timeline;
        const int count = timeline->noteCount();
        const int index = indices.isEmpty() ? count : indices.first();

        kit::Note note;
        note.lyric = QString::fromLatin1(kit::defaultLyric);
        note.length = quantizedLength();
        note.noteNum = index < count ? timeline->note(index).key
                       : count > 0   ? timeline->note(count - 1).key
                                     : 60;
        const auto notes = _impl->notes();
        if (!kit::ProjectEdits::insertNote(notes, index, note, diagnostics)) {
            return false;
        }
        _impl->anchor = notes.at(index).id();
        _impl->setSelection({_impl->anchor});
        return true;
    }

    void PianoRoll::editLyric(int index) {
        _impl->startEditing(index);
    }

    QLineEdit *PianoRoll::lyricEditor() const {
        return _impl->editor;
    }

    std::optional<double> PianoRoll::playheadPosition() const {
        return _impl->playhead;
    }

    void PianoRoll::setPlayheadPosition(std::optional<double> tick) {
        if (tick == _impl->playhead) {
            return;
        }
        _impl->playhead = tick;
        if (tick) {
            // A page at a time, so that the view does not move under the pointer continuously
            auto time = _impl->view->timeAxis();
            const double width = _impl->view->viewport()->width();
            const double x = time.toX(*tick);
            if (x < 0 || x > width) {
                time.left = *tick - width / time.pixelsPerTick * FollowMargin;
                _impl->view->setTimeAxis(time);
            }
        }
        _impl->view->viewport()->update();
    }

    bool PianoRoll::isPitchVisible() const {
        return _impl->pitchVisible;
    }

    void PianoRoll::setPitchVisible(bool visible) {
        _impl->pitchVisible = visible;
        _impl->view->viewport()->update();
    }

    double PianoRoll::pointGrip() const {
        return _impl->pointGrip;
    }

    void PianoRoll::setPointGrip(double pixels) {
        _impl->pointGrip = std::max(0.0, pixels);
    }

    double PianoRoll::curveGrip() const {
        return _impl->curveGrip;
    }

    void PianoRoll::setCurveGrip(double pixels) {
        _impl->curveGrip = std::max(0.0, pixels);
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
        return _impl->noteColor.isValid() ? _impl->noteColor : palette().color(QPalette::Highlight);
    }

    void PianoRoll::setNoteColor(const QColor &color) {
        _impl->noteColor = color;
        _impl->view->viewport()->update();
    }

    QColor PianoRoll::restColor() const {
        if (_impl->restColor.isValid()) {
            return _impl->restColor;
        }
        auto color = palette().color(QPalette::Mid);
        color.setAlphaF(0.4f);
        return color;
    }

    void PianoRoll::setRestColor(const QColor &color) {
        _impl->restColor = color;
        _impl->view->viewport()->update();
    }

    QColor PianoRoll::lyricColor() const {
        return _impl->lyricColor.isValid() ? _impl->lyricColor
                                           : palette().color(QPalette::HighlightedText);
    }

    void PianoRoll::setLyricColor(const QColor &color) {
        _impl->lyricColor = color;
        _impl->view->viewport()->update();
    }

    QColor PianoRoll::unsampledColor() const {
        return _impl->unsampledColor.isValid() ? _impl->unsampledColor
                                               : palette().color(QPalette::Highlight);
    }

    void PianoRoll::setUnsampledColor(const QColor &color) {
        _impl->unsampledColor = color;
        _impl->view->viewport()->update();
    }

    QColor PianoRoll::unsampledLyricColor() const {
        return _impl->unsampledLyricColor.isValid() ? _impl->unsampledLyricColor
                                                    : palette().color(QPalette::Text);
    }

    void PianoRoll::setUnsampledLyricColor(const QColor &color) {
        _impl->unsampledLyricColor = color;
        _impl->view->viewport()->update();
    }

    QColor PianoRoll::selectionColor() const {
        return _impl->selectionColor.isValid() ? _impl->selectionColor
                                               : palette().color(QPalette::WindowText);
    }

    void PianoRoll::setSelectionColor(const QColor &color) {
        _impl->selectionColor = color;
        _impl->view->viewport()->update();
    }

    QColor PianoRoll::pitchColor() const {
        return _impl->pitchColor.isValid() ? _impl->pitchColor : palette().color(QPalette::Text);
    }

    void PianoRoll::setPitchColor(const QColor &color) {
        _impl->pitchColor = color;
        _impl->view->viewport()->update();
    }

    QColor PianoRoll::vibratoColor() const {
        if (_impl->vibratoColor.isValid()) {
            return _impl->vibratoColor;
        }
        auto color = palette().color(QPalette::Text);
        color.setAlphaF(0.5f);
        return color;
    }

    void PianoRoll::setVibratoColor(const QColor &color) {
        _impl->vibratoColor = color;
        _impl->view->viewport()->update();
    }

    QColor PianoRoll::playheadColor() const {
        return _impl->playheadColor.isValid() ? _impl->playheadColor
                                              : palette().color(QPalette::Link);
    }

    void PianoRoll::setPlayheadColor(const QColor &color) {
        _impl->playheadColor = color;
        _impl->view->viewport()->update();
    }

    QColor PianoRoll::whiteRowColor() const {
        return _impl->whiteRowColor.isValid() ? _impl->whiteRowColor
                                              : palette().color(QPalette::Base);
    }

    void PianoRoll::setWhiteRowColor(const QColor &color) {
        _impl->whiteRowColor = color;
        _impl->view->viewport()->update();
    }

    QColor PianoRoll::blackRowColor() const {
        return _impl->blackRowColor.isValid() ? _impl->blackRowColor
                                              : palette().color(QPalette::AlternateBase);
    }

    void PianoRoll::setBlackRowColor(const QColor &color) {
        _impl->blackRowColor = color;
        _impl->view->viewport()->update();
    }

    QColor PianoRoll::lineColor() const {
        if (_impl->lineColor.isValid()) {
            return _impl->lineColor;
        }
        auto color = palette().color(QPalette::Mid);
        color.setAlphaF(0.35f);
        return color;
    }

    void PianoRoll::setLineColor(const QColor &color) {
        _impl->lineColor = color;
        _impl->view->viewport()->update();
    }

    QColor PianoRoll::barLineColor() const {
        return _impl->barLineColor.isValid() ? _impl->barLineColor : palette().color(QPalette::Mid);
    }

    void PianoRoll::setBarLineColor(const QColor &color) {
        _impl->barLineColor = color;
        _impl->view->viewport()->update();
    }

}
