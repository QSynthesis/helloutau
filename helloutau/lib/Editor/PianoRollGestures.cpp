#include "PianoRollGestures_p.h"

#include <algorithm>
#include <cmath>

#include <QtWidgets/QApplication>

#include <hellokit/Edit/ProjectEdits.h>
#include <hellokit/Synth/PitchCurve.h>

namespace hello::daw {

    void PianoRollState::MoveGesture::move(QPointF position, Qt::KeyboardModifiers modifiers) {
        Q_UNUSED(modifiers);
        if (!m_dragging) {
            if ((position - m_origin).manhattanLength() < QApplication::startDragDistance()) {
                return;
            }
            start();
        }

        const auto timeline = m_state->timeline;
        const auto &time = m_state->view->timeAxis();
        const auto &keys = m_state->view->keyAxis();

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
        const double wanted = double(timeline->note(m_first).start) + time.toTick(position.x()) -
                              time.toTick(m_origin.x());
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
        m_state->placements = m_state->layOut(order, {}, run, m_semitones);
        m_state->view->viewport()->update();
    }

    void PianoRollState::MoveGesture::release(QPointF position, Qt::KeyboardModifiers modifiers) {
        move(position, modifiers);
        if (!m_dragging) {
            // A click on one of several selected notes selects it alone.
            if (m_wasSelected && m_state->selection.size() > 1) {
                m_state->selectOnly(m_pressed);
            }
            return;
        }

        const bool moved = m_destination != m_first;
        m_state->clearPreview();
        if (!moved && m_semitones == 0) {
            return;
        }
        const auto notes = m_state->notes();
        QList<kit::NoteRef> run;
        for (int i = m_first; i <= m_last; ++i) {
            run.push_back(notes.at(i));
        }
        auto transaction = m_state->session->transaction(moved ? PianoRoll::tr("Move Notes")
                                                               : PianoRoll::tr("Transpose"));
        kit::DiagnosticList diagnostics;
        if (moved) {
            kit::ProjectEdits::moveNotes(notes, m_first, int(run.size()), m_destination,
                                         diagnostics);
        }
        kit::ProjectEdits::transpose(run, m_semitones, diagnostics);
        transaction.commit(diagnostics);
        m_state->report(diagnostics);
    }

    void PianoRollState::MoveGesture::cancel() {
        m_state->clearPreview();
        m_state->setSelection(m_previous);
    }

    void PianoRollState::MoveGesture::start() {
        m_dragging = true;
        const auto selected = m_state->widget->selectedIndices();
        m_first = selected.isEmpty() ? m_pressed : selected.first();
        m_last = selected.isEmpty() ? m_pressed : selected.last();
        m_state->selectRange(m_first, m_last);
        m_destination = m_first;
    }

    void PianoRollState::LengthGesture::move(QPointF position, Qt::KeyboardModifiers modifiers) {
        const double tick = m_state->view->timeAxis().toTick(position.x());
        qint64 length = m_state->snapped(tick, modifiers) - m_start;
        if (length <= 0) {
            // The first grid line after the start, or one tick
            length = m_state->snaps(modifiers) ? m_state->snappedDown(double(m_start), modifiers) +
                                                     m_state->quantization - m_start
                                               : 1;
        }
        m_length = int(length);
        m_state->placements = m_state->layOut(m_state->identityOrder(), {
                                                                            {m_index, m_length}
        });
        m_state->view->viewport()->update();
    }

    void PianoRollState::LengthGesture::release(QPointF position, Qt::KeyboardModifiers modifiers) {
        move(position, modifiers);
        m_state->clearPreview();
        if (m_length == m_original) {
            return;
        }
        kit::DiagnosticList diagnostics;
        kit::ProjectEdits::setLength(m_state->notes().at(m_index), m_length, diagnostics);
        m_state->report(diagnostics);
    }

    void PianoRollState::LengthGesture::cancel() {
        m_state->clearPreview();
    }

    PianoRollState::BandGesture::BandGesture(PianoRollState *state, QPointF position,
                                             Qt::KeyboardModifiers modifiers)
        : m_state(state), m_origin(position), m_previous(state->selection),
          m_previousPoints(state->selectedPoints) {
        if (modifiers & Qt::ControlModifier) {
            m_base = state->selection;
            m_basePoints = state->selectedPoints;
        }
        state->setSelection(m_base);
        state->selectPoints(m_basePoints);
    }

    void PianoRollState::BandGesture::move(QPointF position, Qt::KeyboardModifiers modifiers) {
        Q_UNUSED(modifiers);
        const auto rect = QRectF(m_origin, position).normalized();
        const auto timeline = m_state->timeline;
        const auto &time = m_state->view->timeAxis();
        const auto [begin, end] =
            timeline->notesBetween(time.toTick(rect.left()), time.toTick(rect.right()));
        m_state->band = rect;
        m_state->view->viewport()->update();

        // With the pitch shown, the points in the rectangle if there are any, among them
        // those of the notes beside it, which may lie beyond their notes
        if (m_state->pointsShown()) {
            auto points = m_basePoints;
            bool found = false;
            const auto refs = m_state->notes();
            for (int i = std::max(0, begin - 1); i < std::min(timeline->noteCount(), end + 1);
                 ++i) {
                if (timeline->note(i).rest) {
                    continue;
                }
                const auto values = m_state->pointsOf(i);
                const auto list = refs.at(i).portamento();
                for (int j = 0; j < values.size() && j < list.size(); ++j) {
                    if (rect.contains(m_state->positionOf(i, j, values[j]))) {
                        points.insert(list.at(j).id());
                        found = true;
                    }
                }
            }
            if (found || !m_basePoints.isEmpty()) {
                m_state->selectPoints(points);
                return;
            }
        }

        auto ids = m_base;
        for (int i = begin; i < end; ++i) {
            const auto &note = timeline->note(i);
            if (m_state->rectOf(note.start, note.length, note.key).intersects(rect)) {
                ids.insert(note.id);
            }
        }
        m_state->selectPoints({});
        m_state->setSelection(ids);
    }

    void PianoRollState::BandGesture::release(QPointF position, Qt::KeyboardModifiers modifiers) {
        move(position, modifiers);
        m_state->clearPreview();
    }

    void PianoRollState::BandGesture::cancel() {
        m_state->clearPreview();
        m_state->setSelection(m_previous);
        m_state->selectPoints(m_previousPoints);
    }

    PianoRollState::SpanGesture::SpanGesture(PianoRollState *state, QPointF position,
                                             Qt::KeyboardModifiers modifiers)
        : m_state(state), m_origin(position.x()), m_previous(state->selection),
          m_previousPoints(state->selectedPoints) {
        if (modifiers & Qt::ControlModifier) {
            m_base = state->selection;
        }
        state->selectPoints({});
        move(position, modifiers);
    }

    void PianoRollState::SpanGesture::move(QPointF position, Qt::KeyboardModifiers modifiers) {
        Q_UNUSED(modifiers);
        const double left = std::min(m_origin, position.x());
        const double right = std::max(m_origin, position.x());
        const auto &time = m_state->view->timeAxis();
        const auto timeline = m_state->timeline;
        const auto [begin, end] = timeline->notesBetween(time.toTick(left), time.toTick(right));
        auto ids = m_base;
        for (int i = begin; i < end; ++i) {
            ids.insert(timeline->note(i).id);
        }
        m_state->setSelection(ids);
        // Over the whole height, its top and bottom edges out of sight
        m_state->band =
            QRectF(QPointF(left, -1), QPointF(right, m_state->view->viewport()->height() + 1));
        m_state->view->viewport()->update();
    }

    void PianoRollState::SpanGesture::release(QPointF position, Qt::KeyboardModifiers modifiers) {
        move(position, modifiers);
        m_state->clearPreview();
    }

    void PianoRollState::SpanGesture::cancel() {
        m_state->clearPreview();
        m_state->setSelection(m_previous);
        m_state->selectPoints(m_previousPoints);
    }

    PianoRollState::DrawGesture::DrawGesture(PianoRollState *state, QPointF position,
                                             Qt::KeyboardModifiers modifiers)
        : m_state(state) {
        const auto timeline = state->timeline;
        const auto &keys = state->view->keyAxis();
        m_key = std::clamp(keys.keyAt(position.y()), kit::lowestNoteNum, kit::highestNoteNum);
        const double tick = state->view->timeAxis().toTick(position.x());
        const int count = timeline->noteCount();
        m_index = std::clamp(timeline->noteAt(tick), 0, count);
        m_from = m_index < count ? timeline->note(m_index).start : timeline->length();
        m_fills = modifiers & Qt::ShiftModifier;
        m_splits = m_fills && m_index < count && timeline->note(m_index).rest;
        m_start = m_fills ? std::max(m_from, state->snappedDown(tick, modifiers)) : m_from;
        const qint64 reach = state->snapped(tick, modifiers) - m_start;
        update(!m_fills && reach > 0 ? int(reach) : state->widget->quantizedLength());
    }

    void PianoRollState::DrawGesture::move(QPointF position, Qt::KeyboardModifiers modifiers) {
        const double tick = m_state->view->timeAxis().toTick(position.x());
        const qint64 length = m_state->snapped(tick, modifiers) - m_start;
        update(length > 0 ? int(length) : (m_state->snaps(modifiers) ? m_state->quantization : 1));
    }

    void PianoRollState::DrawGesture::release(QPointF position, Qt::KeyboardModifiers modifiers) {
        move(position, modifiers);
        m_state->clearPreview();

        const auto notes = m_state->notes();
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
        auto transaction = m_state->session->transaction(PianoRoll::tr("Insert Note"));
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
            m_state->anchor = id;
            m_state->setSelection({id});
        }
        m_state->report(diagnostics);
    }

    void PianoRollState::DrawGesture::cancel() {
        m_state->clearPreview();
    }

    void PianoRollState::DrawGesture::update(int length) {
        m_length = length;
        m_state->drawn = Placement{-1, m_start, m_length, m_key};
        const auto timeline = m_state->timeline;
        m_state->placements.clear();
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
                m_state->placements.push_back(placement);
            }
        }
        m_state->view->viewport()->update();
    }

    void PianoRollState::PointGesture::move(QPointF position, Qt::KeyboardModifiers modifiers) {
        if (!m_dragging) {
            if ((position - m_origin).manhattanLength() < QApplication::startDragDistance()) {
                return;
            }
            start();
        }
        const auto &time = m_state->view->timeAxis();
        const auto &keys = m_state->view->keyAxis();
        double ticks = time.toTick(position.x()) - time.toTick(m_origin.x());
        double cents = (keys.toKey(position.y()) - keys.toKey(m_origin.y())) * 100;

        const auto &pressedNote = m_original[m_index];
        const auto &pressed = pressedNote[m_point];
        if (modifiers & Qt::ShiftModifier) {
            const double at = m_state->ticksOf(pressed.x, m_index);
            std::optional<double> nearest;
            for (int j = 0; j < pressedNote.size(); ++j) {
                const double other = m_state->ticksOf(pressedNote[j].x, m_index);
                if (!m_moving[m_index].contains(j) &&
                    (!nearest || std::abs(other - at - ticks) < std::abs(*nearest - at - ticks))) {
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
        m_state->pointPreview.clear();
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
                        std::round((point.x + m_state->millisecondsOf(ticks, index)) * 10) / 10;
                    if (!m_state->heightFixed(index, j, count)) {
                        point.y = std::round(point.y + cents);
                    }
                }
                points.push_back({point, moving});
            }
            std::stable_sort(points.begin(), points.end(),
                             [](const auto &a, const auto &b) { return a.first.x < b.first.x; });
            QList<kit::PortamentoPoint> sorted;
            for (int j = 0; j < count; ++j) {
                sorted.push_back(points[j].first);
                if (points[j].second) {
                    m_moved[index].insert(j);
                }
            }
            // As it will be written
            endAtPitch(sorted);
            m_state->pointPreview.insert(index, sorted);
        }
        m_state->view->viewport()->update();
    }

    void PianoRollState::PointGesture::release(QPointF position, Qt::KeyboardModifiers modifiers) {
        move(position, modifiers);
        if (!m_dragging) {
            return;
        }
        QHash<int, QList<kit::PortamentoPoint>> changed;
        for (auto it = m_state->pointPreview.begin(); it != m_state->pointPreview.end(); ++it) {
            if (it.value() != m_original[it.key()]) {
                changed.insert(it.key(), it.value());
            }
        }
        m_state->pointPreview.clear();
        m_state->view->viewport()->update();
        if (changed.isEmpty()) {
            return;
        }
        kit::DiagnosticList diagnostics;
        if (!m_state->writePoints(PianoRoll::tr("Move Pitch Points"), changed, diagnostics)) {
            m_state->report(diagnostics);
            return;
        }
        // The same points stay selected where the order put them.
        QSet<kit::edit::NodeId> ids;
        const auto refs = m_state->notes();
        for (auto it = m_moved.begin(); it != m_moved.end(); ++it) {
            const auto list = refs.at(it.key()).portamento();
            for (const int j : it.value()) {
                ids.insert(list.at(j).id());
            }
        }
        m_state->selectPoints(ids);
    }

    void PianoRollState::PointGesture::cancel() {
        m_state->pointPreview.clear();
        m_state->view->viewport()->update();
    }

    void PianoRollState::PointGesture::start() {
        m_dragging = true;
        m_moving = m_state->selectedPointIndices();
        m_moving[m_index].insert(m_point);
        for (auto it = m_moving.begin(); it != m_moving.end(); ++it) {
            m_original.insert(it.key(), m_state->pointsOf(it.key()));
        }
    }

    void PianoRollState::VibratoGesture::move(QPointF position, Qt::KeyboardModifiers modifiers) {
        Q_UNUSED(modifiers);
        const auto &time = m_state->view->timeAxis();
        const auto &keys = m_state->view->keyAxis();
        const double ticks = time.toTick(position.x()) - time.toTick(m_origin.x());
        const double length = m_state->timeline->note(m_index).length;
        const double span = m_original.length / 100 * length;
        const double period = m_state->ticksOf(m_original.period, m_index);
        const auto percent = [](double value) { return std::clamp(std::round(value), 0.0, 100.0); };

        auto vibrato = m_original;
        switch (m_part) {
            case PianoRoll::VibratoStart:
                vibrato.length = percent((span - ticks) / length * 100);
                break;
            case PianoRoll::VibratoFadeIn:
                if (span > 0) {
                    vibrato.attack = percent(m_original.attack + ticks / span * 100);
                }
                break;
            case PianoRoll::VibratoFadeOut:
                if (span > 0) {
                    vibrato.release = percent(m_original.release - ticks / span * 100);
                }
                break;
            case PianoRoll::VibratoDepth:
                vibrato.amplitude =
                    std::max(0.0, std::round(m_original.amplitude +
                                             (keys.toKey(position.y()) - keys.toKey(m_origin.y())) *
                                                 VibratoCentsPerRow));
                break;
            case PianoRoll::VibratoPeriod: {
                // The right edge of the box follows the pointer; the box starts at the
                // phase, a share of the period itself.
                const double share = 1 + m_original.phase / 100;
                vibrato.period = std::max(1.0, std::round(m_state->millisecondsOf(
                                                   (share * period + ticks) / share, m_index)));
                break;
            }
            case PianoRoll::VibratoPhase:
                if (period > 0) {
                    vibrato.phase = percent(m_original.phase + ticks / period * 100);
                }
                break;
            default:
                break;
        }
        m_state->vibratoPreview.insert(m_index, vibrato);
        m_state->view->viewport()->update();
    }

    void PianoRollState::VibratoGesture::release(QPointF position,
                                                 Qt::KeyboardModifiers modifiers) {
        move(position, modifiers);
        const auto vibrato = m_state->vibratoPreview.take(m_index);
        m_state->view->viewport()->update();
        if (vibrato != m_original) {
            kit::DiagnosticList diagnostics;
            kit::ProjectEdits::setVibrato({m_state->notes().at(m_index)}, vibrato, diagnostics);
            m_state->report(diagnostics);
        }
    }

    void PianoRollState::VibratoGesture::cancel() {
        m_state->vibratoPreview.remove(m_index);
        m_state->view->viewport()->update();
    }

    PianoRollState::BendGesture::BendGesture(PianoRollState *state, QPointF position, bool erases)
        : m_state(state), m_erases(erases) {
        add(position);
        update();
    }

    void PianoRollState::BendGesture::move(QPointF position, Qt::KeyboardModifiers modifiers) {
        Q_UNUSED(modifiers);
        add(position);
        update();
    }

    void PianoRollState::BendGesture::release(QPointF position, Qt::KeyboardModifiers modifiers) {
        move(position, modifiers);
        m_state->bendPreview.clear();
        m_state->view->viewport()->update();
        if (m_drawn.isEmpty()) {
            return;
        }
        kit::DiagnosticList diagnostics;
        auto transaction = m_state->session->transaction(m_erases ? PianoRoll::tr("Reset Pitch")
                                                                  : PianoRoll::tr("Draw Pitch"));
        // From the last note back, so that each note fills its gaps from the curve of the
        // previous note as it was
        auto indices = m_drawn.keys();
        std::sort(indices.begin(), indices.end(), std::greater<>());
        const auto notes = m_state->notes();
        for (const int index : std::as_const(indices)) {
            const auto &[tick, values] = m_drawn[index];
            kit::ProjectEdits::drawPitchBend(notes, index, tick, values, diagnostics);
        }
        transaction.commit(diagnostics);
        m_state->report(diagnostics);
    }

    void PianoRollState::BendGesture::cancel() {
        m_state->bendPreview.clear();
        m_state->view->viewport()->update();
    }

    void PianoRollState::BendGesture::add(QPointF position) {
        const auto &time = m_state->view->timeAxis();
        const auto &keys = m_state->view->keyAxis();
        m_path.push_back({time.toTick(position.x()), keys.toKey(position.y())});
    }

    std::optional<double> PianoRollState::BendGesture::keyAt(double tick) const {
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

    void PianoRollState::BendGesture::update() {
        m_state->bendPreview.clear();
        m_drawn.clear();
        double low = m_path.first().x();
        double high = low;
        for (const auto &point : std::as_const(m_path)) {
            low = std::min(low, point.x());
            high = std::max(high, point.x());
        }

        // The notes along the path, and the next one, which reads its curve before it
        const auto timeline = m_state->timeline;
        const int count = timeline->noteCount();
        auto [begin, end] = timeline->notesBetween(low, high);
        end = std::min(count, end + 1);
        const auto &timings = m_state->sampleTimings();
        const auto refs = m_state->notes();
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
                hasValues ? m_state->ticksOf(bend->start.value_or(0), i) : firstReading;
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
            m_state->bendPreview.insert(i,
                                        kit::PitchBend::drawn(bend, previous, tempo, tick, values));
        }
        m_state->view->viewport()->update();
    }

}
