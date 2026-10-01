#include "PianoRollState_p.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include <QtCore/QTimer>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QMenu>

#include <stdutau/utaconst.h>

#include <hellokit/Synth/PitchCurve.h>

#include <helloutau/Widgets/TimelineRuler.h>

#include "PianoRollGestures_p.h"

namespace hello::daw {

    kit::Envelope PianoRollState::defaultEnvelope() {
        kit::Envelope envelope;
        envelope.anchors[0] = {0, 0};
        envelope.anchors[1] = {5, 100};
        envelope.anchors[3] = {35, 100};
        envelope.anchors[4] = {0, 0};
        return envelope;
    }

    QString PianoRollState::tempoText(double tempo) {
        return QString::number(tempo, 'g', 6);
    }

    bool PianoRollState::LyricEditor::event(QEvent *event) {
        // Tab is taken here, before focus navigation consumes it.
        if (event->type() == QEvent::KeyPress) {
            const auto key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab) {
                tabbed(key->key() == Qt::Key_Tab && !(key->modifiers() & Qt::ShiftModifier));
                return true;
            }
        }
        return QLineEdit::event(event);
    }

    void PianoRollState::LyricEditor::keyPressEvent(QKeyEvent *event) {
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

    void PianoRollState::LyricEditor::focusOutEvent(QFocusEvent *event) {
        QLineEdit::focusOutEvent(event);
        // The context menu of the editor takes the focus without ending the editing.
        if (event->reason() != Qt::PopupFocusReason) {
            committed();
        }
    }

    void PianoRollState::showRulerMenu(double tick, const QPoint &globalPosition) {
        auto &decl = *widget;
        const int count = timeline->noteCount();
        const int index = count == 0 ? -1 : std::clamp(timeline->noteAt(tick), 0, count - 1);
        QMenu menu(&decl);
        const auto set = menu.addAction(PianoRoll::tr("Set Tempo &Here..."));
        set->setEnabled(index >= 0);
        QObject::connect(set, &QAction::triggered, &decl,
                         [this, index] { Q_EMIT widget->tempoRequested(index); });
        const auto remove = menu.addAction(PianoRoll::tr("&Remove Tempo Mark"));
        remove->setEnabled(index > 0 && notes().at(index).tempo().has_value());
        QObject::connect(remove, &QAction::triggered, &decl, [this, index] {
            kit::NotePropertyChanges changes;
            changes.tempo = std::optional<double>();
            kit::DiagnosticList diagnostics;
            kit::ProjectEdits::setNoteProperties({notes().at(index)}, changes, diagnostics);
            report(diagnostics);
        });

        menu.addSeparator();
        const auto label = menu.addAction(PianoRoll::tr("Set &Label Here..."));
        label->setEnabled(index >= 0);
        QObject::connect(label, &QAction::triggered, &decl,
                         [this, index] { Q_EMIT widget->labelRequested(index); });
        // The selected notes if unbroken, otherwise the note at the position
        const auto range = decl.selectedRange().value_or(std::pair{index, index});
        const auto region = menu.addAction(PianoRoll::tr("&Name Region..."));
        region->setEnabled(index >= 0);
        QObject::connect(region, &QAction::triggered, &decl, [this, range] {
            Q_EMIT widget->regionRequested(range.first, range.second);
        });
        const auto load = menu.addMenu(PianoRoll::tr("L&oad Region"));
        decl.fillRegionMenu(load);
        menu.exec(globalPosition);
    }

    kit::NoteListRef PianoRollState::notes() const {
        return kit::ProjectRef(session).tracks().at(0).notes();
    }

    bool PianoRollState::mode1() const {
        return !kit::ProjectRef(session).settings().mode2();
    }

    bool PianoRollState::pointsShown() const {
        return pitchVisible && !mode1();
    }

    bool PianoRollState::bendShown() const {
        return pitchVisible && mode1();
    }

    bool PianoRollState::drawsBend(Qt::MouseButton button) const {
        return bendShown() && tool == PianoRoll::PitchTool &&
               (button == Qt::LeftButton || button == Qt::RightButton);
    }

    int PianoRollState::indexOf(kit::edit::NodeId id) const {
        for (int i = 0; i < timeline->noteCount(); ++i) {
            if (timeline->note(i).id == id) {
                return i;
            }
        }
        return -1;
    }

    bool PianoRollState::isSelected(int index) const {
        return selection.contains(timeline->note(index).id);
    }

    void PianoRollState::setSelection(const QSet<kit::edit::NodeId> &ids) {
        auto &decl = *widget;
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

    void PianoRollState::hover(std::optional<QPointF> position) {
        if (view->hasGesture()) {
            return;
        }
        int note = -1;
        if (position && pointsShown()) {
            if (const auto hit = view->hitAt(*position);
                hit && hit->part == PianoRoll::PitchPoint) {
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

    void PianoRollState::selectPoints(const QSet<kit::edit::NodeId> &ids) {
        auto &decl = *widget;
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

    double PianoRollState::ticksOf(double milliseconds, int index) const {
        return milliseconds * timeline->tempoMap().tempo(index) * kit::ticksPerQuarter / 60000;
    }

    double PianoRollState::millisecondsOf(double ticks, int index) const {
        return ticks * 60000 / (timeline->tempoMap().tempo(index) * kit::ticksPerQuarter);
    }

    bool PianoRollState::startsAtPrevious(int index) const {
        return index > 0 && !timeline->note(index - 1).rest;
    }

    bool PianoRollState::heightFixed(int index, int j, int count) const {
        return (j == 0 && startsAtPrevious(index)) || (count >= 2 && j == count - 1);
    }

    QList<kit::PortamentoPoint> PianoRollState::pointsOf(int index) const {
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

    QPointF PianoRollState::positionOf(int index, int j, const kit::PortamentoPoint &point) const {
        const auto &note = timeline->note(index);
        const double cents = j == 0 && startsAtPrevious(index)
                                 ? (timeline->note(index - 1).key - note.key) * 100.0
                                 : point.y;
        return {view->timeAxis().toX(double(note.start) + ticksOf(point.x, index)),
                view->keyAxis().toY(note.key + 0.5 + cents / 100)};
    }

    void PianoRollState::fitParameters() {
        const auto range = rangeOf(lane);
        auto axis = parameters->keyAxis();
        const double height = parameters->viewport()->height();
        axis.pixelsPerKey =
            std::max(0.01, (height - 2 * ParameterMargin) / (range.maximum - range.minimum));
        axis.top = range.maximum + ParameterMargin / axis.pixelsPerKey;
        parameters->setKeyAxis(axis);
    }

    double PianoRollState::keyOf(PianoRoll::Lane lane, double value) {
        if (lane != PianoRoll::VelocityLane || value >= utau::DEFAULT_VALUE_VELOCITY) {
            return value;
        }
        return (value - VelocityMinimum) * utau::DEFAULT_VALUE_VELOCITY /
               (utau::DEFAULT_VALUE_VELOCITY - VelocityMinimum);
    }

    double PianoRollState::valueAt(PianoRoll::Lane lane, double key) {
        if (lane != PianoRoll::VelocityLane || key >= utau::DEFAULT_VALUE_VELOCITY) {
            return key;
        }
        return VelocityMinimum + key * (utau::DEFAULT_VALUE_VELOCITY - VelocityMinimum) /
                                     utau::DEFAULT_VALUE_VELOCITY;
    }

    PianoRollState::LaneRange PianoRollState::rangeOf(PianoRoll::Lane lane) {
        switch (lane) {
            case PianoRoll::IntensityLane:
                return {0, IntensityRange, utau::DEFAULT_VALUE_INTENSITY};
            case PianoRoll::ModulationLane:
                return {-ModulationRange, ModulationRange, utau::DEFAULT_VALUE_MODULATION};
            case PianoRoll::VelocityLane:
                return {0, VelocityRange, utau::DEFAULT_VALUE_VELOCITY};
            default:
                return {0, EnvelopeRange, 100};
        }
    }

    double PianoRollState::quarterNearest(PianoRoll::Lane lane, double key) {
        const auto range = rangeOf(lane);
        const double quarter = (range.maximum - range.minimum) / 4;
        return range.minimum + std::round((key - range.minimum) / quarter) * quarter;
    }

    kit::ProjectEdits::NoteParameter PianoRollState::parameterOf(PianoRoll::Lane lane) {
        switch (lane) {
            case PianoRoll::ModulationLane:
                return kit::ProjectEdits::Modulation;
            case PianoRoll::VelocityLane:
                return kit::ProjectEdits::Velocity;
            default:
                return kit::ProjectEdits::Intensity;
        }
    }

    std::optional<double> PianoRollState::storedValueOf(int index) const {
        const auto note = notes().at(index);
        switch (lane) {
            case PianoRoll::ModulationLane:
                return note.modulation();
            case PianoRoll::VelocityLane:
                return note.velocity();
            default:
                return note.intensity();
        }
    }

    double PianoRollState::valueOf(int index) const {
        if (const auto it = valuePreview.find(index); it != valuePreview.end()) {
            return *it;
        }
        return storedValueOf(index).value_or(rangeOf(lane).fallback);
    }

    QList<int> PianoRollState::valueTargets(int index) const {
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

    void PianoRollState::writeValue(const QList<int> &indices, std::optional<double> value) {
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

    const QList<kit::SampleTiming> &PianoRollState::sampleTimings() {
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

    QList<kit::Note> PianoRollState::previewedNotes(int first, int last) const {
        const auto refs = notes();
        QList<kit::Note> result;
        for (int i = first; i < last; ++i) {
            result.push_back(refs.at(i).toNote());
            if (const auto it = pointPreview.find(i); it != pointPreview.end()) {
                result.last().portamento = *it;
            }
            if (const auto it = vibratoPreview.find(i); it != vibratoPreview.end()) {
                result.last().vibrato = *it;
            }
            if (const auto it = bendPreview.find(i); it != bendPreview.end()) {
                result.last().pitchBend = *it;
            }
        }
        return result;
    }

    kit::Envelope PianoRollState::envelopeOf(int index) const {
        if (const auto it = envelopePreview.find(index); it != envelopePreview.end()) {
            return *it;
        }
        return notes().at(index).envelope().value_or(defaultEnvelope());
    }

    std::pair<double, double> PianoRollState::fragmentOf(int index) {
        const auto &all = sampleTimings();
        const auto &map = timeline->tempoMap();
        const double duration = timeline->note(index).length * 125.0 / map.tempo(index);
        double length = duration + all[index].preUtterance;
        if (index + 1 < all.size()) {
            length += all[index + 1].voiceOverlap - all[index + 1].preUtterance;
        }
        return {map.startTime(index) - all[index].preUtterance, length};
    }

    QList<double> PianoRollState::anchorTimes(const kit::Envelope &envelope, double length) {
        const auto &a = envelope.anchors;
        QList<double> times{a[0].x, a[0].x + a[1].x};
        if (envelope.hasMiddle) {
            times.push_back(times.last() + a[2].x);
        }
        times.push_back(length - a[4].x - a[3].x);
        times.push_back(length - a[4].x);
        return times;
    }

    QPointF PianoRollState::envelopePointOf(int index, double milliseconds, double volume) {
        const double start = fragmentOf(index).first;
        return {parameters->timeAxis().toX(timeline->tempoMap().tickOf(start + milliseconds)),
                parameters->keyAxis().toY(volume)};
    }

    std::optional<kit::Vibrato> PianoRollState::vibratoOf(int index) const {
        if (const auto it = vibratoPreview.find(index); it != vibratoPreview.end()) {
            return *it;
        }
        return notes().at(index).vibrato();
    }

    std::optional<PianoRollState::VibratoShape> PianoRollState::vibratoShapeOf(int index) const {
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
        const double top =
            keys.toY(note.key + 0.5 - VibratoBaseline + vibrato->amplitude / VibratoCentsPerRow);
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

    void PianoRollState::report(const kit::DiagnosticList &diagnostics) {
        auto &decl = *widget;
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

    void PianoRollState::endAtPitch(QList<kit::PortamentoPoint> &points) {
        if (!points.isEmpty()) {
            points.last().y = 0;
        }
    }

    bool PianoRollState::writePoints(const QString &message,
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

    QHash<int, QSet<int>> PianoRollState::selectedPointIndices() const {
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

    bool PianoRollState::removePoints(const QHash<int, QSet<int>> &removed,
                                      kit::DiagnosticList &diagnostics) {
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

    void PianoRollState::selectOnly(int index) {
        anchor = timeline->note(index).id;
        setSelection({anchor});
    }

    void PianoRollState::selectRange(int first, int last) {
        QSet<kit::edit::NodeId> ids;
        for (int i = std::min(first, last); i <= std::max(first, last); ++i) {
            ids.insert(timeline->note(i).id);
        }
        setSelection(ids);
    }

    bool PianoRollState::snaps(Qt::KeyboardModifiers modifiers) const {
        return quantization > 0 && !(modifiers & Qt::AltModifier);
    }

    qint64 PianoRollState::snapped(double tick, Qt::KeyboardModifiers modifiers) const {
        if (!snaps(modifiers)) {
            return qint64(std::llround(tick));
        }
        return qint64(std::llround(tick / quantization)) * quantization;
    }

    qint64 PianoRollState::snappedDown(double tick, Qt::KeyboardModifiers modifiers) const {
        if (!snaps(modifiers)) {
            return qint64(std::floor(tick));
        }
        return qint64(std::floor(tick / quantization)) * quantization;
    }

    QRectF PianoRollState::rectOf(qint64 start, int length, int key) const {
        const auto &time = view->timeAxis();
        const auto &keys = view->keyAxis();
        return {time.toX(double(start)), keys.toY(key + 1), length * time.pixelsPerTick,
                keys.pixelsPerKey};
    }

    QList<PianoRollState::Placement> PianoRollState::layOut(const QList<int> &order,
                                                            const QHash<int, int> &lengths,
                                                            const QSet<int> &transposed,
                                                            int semitones) const {
        QList<Placement> result;
        result.reserve(order.size());
        qint64 start = 0;
        for (const int index : order) {
            const auto &note = timeline->note(index);
            const int length = lengths.value(index, note.length);
            result.push_back(
                {index, start, length, note.key + (transposed.contains(index) ? semitones : 0)});
            start += length;
        }
        return result;
    }

    QList<int> PianoRollState::identityOrder() const {
        QList<int> order(timeline->noteCount());
        std::iota(order.begin(), order.end(), 0);
        return order;
    }

    void PianoRollState::clearPreview() {
        placements.clear();
        drawn.reset();
        band.reset();
        view->viewport()->update();
    }

    void PianoRollState::scheduleRefresh() {
        auto &decl = *widget;
        if (refreshPending) {
            return;
        }
        refreshPending = true;
        QTimer::singleShot(0, &decl, [this] { refresh(); });
    }

    void PianoRollState::refresh() {
        refreshPending = false;
        const auto bars =
            std::max<qint64>(MinimumBars, timeline->length() / BarTicks + 1 + TrailingBars);
        view->setTickRange(0, double(bars * BarTicks));
        parameters->setTickRange(0, double(bars * BarTicks));

        // The tempo at the start, and wherever a note sets one
        const auto &map = timeline->tempoMap();
        QList<TimelineRuler::Mark> marks;
        markNotes.clear();
        for (int i = 0; i < timeline->noteCount(); ++i) {
            const auto &note = timeline->note(i);
            if (i == 0 || note.tempo) {
                marks.push_back({double(note.start), tempoText(map.tempo(i))});
                markNotes.push_back(i);
            }
        }
        ruler->setMarks(marks);

        // The regions, then the label of each note that has one
        QList<TimelineRuler::Section> sections;
        sectionNotes.clear();
        for (const auto &region : widget->regions()) {
            sections.push_back(
                {double(timeline->note(region.first).start),
                 double(timeline->note(region.last).start + timeline->note(region.last).length),
                 region.name, false});
            sectionNotes.push_back({region.first, region.last, false});
        }
        const auto refs = notes();
        for (int i = 0; i < timeline->noteCount(); ++i) {
            const auto label = refs.at(i).label();
            if (!label.isEmpty()) {
                const auto &note = timeline->note(i);
                sections.push_back(
                    {double(note.start), double(note.start + note.length), label, true});
                sectionNotes.push_back({i, i, true});
            }
        }
        ruler->setSections(sections);
        updateRenderSpans();
        view->viewport()->update();
    }

    void PianoRollState::updateRenderSpans() {
        auto &decl = *widget;
        QList<TimelineRuler::Span> spans;
        PianoRoll::RenderState last = PianoRoll::RenderSilent;
        const int count = std::min<int>(timeline->noteCount(), int(renderStates.size()));
        for (int i = 0; i < count; ++i) {
            const auto state = renderStates[i];
            if (state == PianoRoll::RenderSilent) {
                last = state;
                continue;
            }
            const auto &note = timeline->note(i);
            const double end = double(note.start + note.length);
            if (state == last && !spans.isEmpty()) {
                spans.last().last = end;
            } else {
                spans.push_back({double(note.start), end, renderColor(state)});
            }
            last = state;
        }
        ruler->setSpans(spans);
    }

    QColor PianoRollState::renderColor(PianoRoll::RenderState state) const {
        auto &decl = *widget;
        switch (state) {
            case PianoRoll::RenderWaiting:
                return decl.renderWaitingColor();
            case PianoRoll::RenderRunning:
                return decl.renderRunningColor();
            case PianoRoll::RenderReady:
                return decl.renderReadyColor();
            case PianoRoll::RenderFailed:
                return decl.renderFailedColor();
            default:
                return {};
        }
    }

    QColor &PianoRollState::renderColorOf(PianoRoll::RenderState state) {
        return renderColors[int(state) - int(PianoRoll::RenderWaiting)];
    }

    void PianoRollState::ensureVisible(int index) {
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

    void PianoRollState::startEditing(int index) {
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

    void PianoRollState::finishEditing(bool commit) {
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

    void PianoRollState::editNext(bool forward) {
        const int index = indexOf(editing);
        finishEditing(true);
        if (index >= 0) {
            startEditing(forward ? index + 1 : index - 1);
        }
    }

    std::unique_ptr<SceneGesture> PianoRollState::bendGesture(QPointF position,
                                                              Qt::MouseButton button) {
        finishEditing(true);
        return std::make_unique<BendGesture>(this, position, button == Qt::RightButton);
    }

    std::optional<std::pair<int, double>> PianoRollState::portamentoNear(QPointF position) const {
        const int count = timeline->noteCount();
        if (!pointsShown() || count == 0) {
            return std::nullopt;
        }
        const auto &time = view->timeAxis();
        const auto &keys = view->keyAxis();
        const double tick = time.toTick(position.x());
        const int at = timeline->noteAt(tick);
        if (at < 0 || at >= count) {
            return std::nullopt;
        }

        // The own portamento of the note at the position, of the note before it, whose last
        // point may lie past its end, and of the note after it, whose points may lie before
        // its start. The flat portamento of a note without points is hit, although it is not
        // drawn. The nearest curve is taken, the later note if two curves are equally near.
        const int first = std::max(0, at - 2);
        const int last = std::min(count, at + 2);
        const auto refs = notes();
        QList<kit::Note> around;
        for (int i = first; i < last; ++i) {
            around.push_back(refs.at(i).toNote());
        }
        std::optional<std::pair<int, double>> nearest;
        double distance = curveGrip;
        for (int index = std::max(0, at - 1); index < last; ++index) {
            const auto &note = timeline->note(index);
            if (note.rest) {
                continue;
            }
            const kit::PitchCurve curve(around, index - first, timeline->tempoMap().tempo(index));
            const double local = tick - double(note.start);
            const auto [start, stop] = curve.ownSpan();
            if (local < start || local > stop) {
                continue;
            }
            const double y = keys.toY(note.key + 0.5 + curve.ownPortamentoAt(local) / 100);
            if (const double d = std::abs(y - position.y()); d <= distance) {
                distance = d;
                nearest = std::pair{index, local};
            }
        }
        return nearest;
    }

    bool PianoRollState::insertPointAt(QPointF position) {
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

    bool PianoRollState::insert(const QString &lyric, kit::DiagnosticList &diagnostics) {
        auto &decl = *widget;
        const auto indices = decl.selectedIndices();
        const int count = timeline->noteCount();
        const int index = indices.isEmpty() ? count : indices.first();

        kit::Note note;
        note.lyric = lyric;
        note.length = decl.quantizedLength();
        note.noteNum = index < count ? timeline->note(index).key
                       : count > 0   ? timeline->note(count - 1).key
                                     : 60;
        const auto refs = notes();
        if (!kit::ProjectEdits::insertNotes(refs, index, {note}, diagnostics)) {
            return false;
        }
        anchor = refs.at(index).id();
        setSelection({anchor});
        return true;
    }

}
