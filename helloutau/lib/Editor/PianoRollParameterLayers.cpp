#include "PianoRollParameterLayers_p.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <hellokit/Edit/ProjectEdits.h>

namespace hello::daw {

    void PianoRollState::EnvelopeLayer::paint(QPainter &painter, const QRect &exposed) {
        const auto decl = m_state->widget;
        const auto &keys = view()->keyAxis();
        const auto lane = m_state->lane;
        const double fallback = keys.toY(keyOf(lane, rangeOf(lane).fallback));
        painter.fillRect(exposed, decl->whiteRowColor());
        painter.setPen(QPen(decl->lineColor(), 1, Qt::DashLine));
        painter.drawLine(QPointF(exposed.left(), fallback), QPointF(exposed.right() + 1, fallback));
        if (m_state->lane != PianoRoll::EnvelopeLane) {
            return;
        }

        painter.setRenderHint(QPainter::Antialiasing);
        auto fill = decl->envelopeColor();
        fill.setAlphaF(fill.alphaF() * 0.25f);
        const auto [begin, end] = visibleNotes(exposed);
        for (int i = begin; i < end; ++i) {
            if (m_state->timeline->note(i).rest) {
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

    std::optional<SceneHit> PianoRollState::EnvelopeLayer::hitTest(QPointF position) const {
        if (m_state->lane != PianoRoll::EnvelopeLane) {
            return std::nullopt;
        }
        SceneHit hit;
        hit.part = PianoRoll::Background;
        double distance = m_state->pointGrip;
        const auto [begin, end] = visibleNotes(QRect(position.toPoint(), QSize(1, 1)));
        for (int i = begin; i < end; ++i) {
            if (m_state->timeline->note(i).rest) {
                continue;
            }
            const auto outline = outlineOf(i);
            for (qsizetype k = 1; k + 1 < outline.size(); ++k) {
                const auto offset = outline[k] - position;
                const double d = std::hypot(offset.x(), offset.y());
                if (d <= distance) {
                    distance = d;
                    hit.node = m_state->timeline->note(i).id;
                    hit.part = PianoRoll::EnvelopePoint;
                    hit.index = int(k - 1);
                    hit.cursor = Qt::SizeAllCursor;
                }
            }
        }
        return hit;
    }

    bool PianoRollState::EnvelopeLayer::doubleClick(const SceneHit &hit, QPointF position) {
        if (hit.part == PianoRoll::EnvelopePoint) {
            const int index = m_state->indexOf(hit.node);
            auto envelope = m_state->envelopeOf(index);
            if (!envelope.hasMiddle || hit.index != 2) {
                return false;
            }
            envelope.hasMiddle = false;
            write(index, envelope);
            return true;
        }
        const auto [begin, end] = visibleNotes(QRect(position.toPoint(), QSize(1, 1)));
        for (int i = begin; i < end; ++i) {
            if (m_state->timeline->note(i).rest) {
                continue;
            }
            auto envelope = m_state->envelopeOf(i);
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
            if (std::abs(on.y() - position.y()) > m_state->curveGrip) {
                continue;
            }
            const auto [start, length] = m_state->fragmentOf(i);
            const auto times = anchorTimes(envelope, length);
            const double at =
                m_state->timeline->tempoMap().timeOf(view()->timeAxis().toTick(position.x())) -
                start;
            envelope.hasMiddle = true;
            envelope.anchors[2].x = std::round((at - times[1]) * 10) / 10;
            envelope.anchors[2].y = std::round(view()->keyAxis().toKey(on.y()));
            write(i, envelope);
            return true;
        }
        return false;
    }

    void PianoRollState::EnvelopeLayer::write(int index, const kit::Envelope &envelope) {
        kit::DiagnosticList diagnostics;
        kit::ProjectEdits::setEnvelope({m_state->notes().at(index)}, envelope, diagnostics);
        m_state->report(diagnostics);
    }

    std::pair<int, int> PianoRollState::EnvelopeLayer::visibleNotes(const QRect &rect) const {
        const auto &time = view()->timeAxis();
        const auto [begin, end] = m_state->timeline->notesBetween(time.toTick(rect.left()),
                                                                  time.toTick(rect.right() + 1));
        return {std::max(0, begin - 1), std::min(m_state->timeline->noteCount(), end + 1)};
    }

    QPolygonF PianoRollState::EnvelopeLayer::outlineOf(int index) const {
        const auto envelope = m_state->envelopeOf(index);
        const auto length = m_state->fragmentOf(index).second;
        const auto times = anchorTimes(envelope, length);
        const auto anchors = envelope.anchorsInTimeOrder();
        QPolygonF outline{m_state->envelopePointOf(index, 0, 0)};
        for (qsizetype k = 0; k < times.size(); ++k) {
            outline.push_back(m_state->envelopePointOf(index, times[k], anchors[k].y));
        }
        outline.push_back(m_state->envelopePointOf(index, length, 0));
        return outline;
    }

    PianoRollState::EnvelopeGesture::EnvelopeGesture(PianoRollState *state, int index, int anchor,
                                                     QPointF position)
        : m_state(state), m_index(index), m_anchor(anchor), m_origin(position),
          m_original(state->envelopeOf(index)) {
        m_length = state->fragmentOf(index).second;
    }

    void PianoRollState::EnvelopeGesture::move(QPointF position, Qt::KeyboardModifiers modifiers) {
        Q_UNUSED(modifiers);
        const auto view = m_state->parameters;
        const auto &map = m_state->timeline->tempoMap();
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

        m_state->envelopePreview.insert(m_index, *kit::Envelope::fromTimeOrder(anchors));
        view->viewport()->update();
    }

    void PianoRollState::EnvelopeGesture::release(QPointF position,
                                                  Qt::KeyboardModifiers modifiers) {
        move(position, modifiers);
        const auto envelope = m_state->envelopePreview.take(m_index);
        m_state->parameters->viewport()->update();
        if (envelope != m_original) {
            kit::DiagnosticList diagnostics;
            kit::ProjectEdits::setEnvelope({m_state->notes().at(m_index)}, envelope, diagnostics);
            m_state->report(diagnostics);
        }
    }

    void PianoRollState::EnvelopeGesture::cancel() {
        m_state->envelopePreview.remove(m_index);
        m_state->parameters->viewport()->update();
    }

    // A press on an anchor drags it; the right button sets its volume to 100%.
    std::unique_ptr<SceneGesture>
        PianoRollState::EnvelopeLayer::press(const SceneHit &hit, QPointF position,
                                             Qt::MouseButton button,
                                             Qt::KeyboardModifiers modifiers) {
        Q_UNUSED(modifiers);
        if (hit.part != PianoRoll::EnvelopePoint) {
            return nullptr;
        }
        m_state->finishEditing(true);
        const int index = m_state->indexOf(hit.node);
        if (index < 0) {
            return nullptr;
        }
        if (button == Qt::RightButton) {
            auto anchors = m_state->envelopeOf(index).anchorsInTimeOrder();
            if (anchors[hit.index].y != 100) {
                anchors[hit.index].y = 100;
                write(index, *kit::Envelope::fromTimeOrder(anchors));
            }
            return nullptr;
        }
        if (button != Qt::LeftButton) {
            return nullptr;
        }
        return std::make_unique<EnvelopeGesture>(m_state, index, hit.index, position);
    }

    void PianoRollState::ValueLayer::paint(QPainter &painter, const QRect &exposed) {
        if (m_state->lane == PianoRoll::EnvelopeLane) {
            return;
        }
        const auto decl = m_state->widget;
        const double bottom = view()->keyAxis().toY(rangeOf(m_state->lane).minimum);
        painter.setRenderHint(QPainter::Antialiasing);
        const auto [begin, end] = visibleNotes(exposed);
        for (int i = begin; i < end; ++i) {
            if (m_state->timeline->note(i).rest) {
                continue;
            }
            const auto [point, right] = handleOf(i);
            const bool stored =
                m_state->valuePreview.contains(i) || m_state->storedValueOf(i).has_value();
            const auto color = stored ? decl->parameterColor() : decl->faintPointColor();
            painter.setPen(QPen(color, 1.5));
            painter.drawLine(point, QPointF(point.x(), bottom));
            painter.drawLine(point, QPointF(right, point.y()));
            painter.setBrush(m_state->isSelected(i) ? color : decl->whiteRowColor());
            painter.drawEllipse(point, PointRadius, PointRadius);
        }
    }

    std::optional<SceneHit> PianoRollState::ValueLayer::hitTest(QPointF position) const {
        if (m_state->lane == PianoRoll::EnvelopeLane) {
            return std::nullopt;
        }
        std::optional<SceneHit> hit;
        double distance = std::numeric_limits<double>::infinity();
        const auto [begin, end] = visibleNotes(QRect(position.toPoint(), QSize(1, 1)));
        for (int i = begin; i < end; ++i) {
            if (m_state->timeline->note(i).rest) {
                continue;
            }
            const auto [point, right] = handleOf(i);
            const auto offset = point - position;
            double d = std::hypot(offset.x(), offset.y());
            if (d > m_state->pointGrip) {
                d = position.x() >= point.x() && position.x() <= right
                        ? std::abs(position.y() - point.y())
                        : std::numeric_limits<double>::infinity();
                if (d > m_state->curveGrip) {
                    continue;
                }
            }
            if (d < distance) {
                distance = d;
                hit = SceneHit();
                hit->node = m_state->timeline->note(i).id;
                hit->part = PianoRoll::ParameterHandle;
                hit->cursor = Qt::SizeVerCursor;
            }
        }
        return hit;
    }

    std::pair<int, int> PianoRollState::ValueLayer::visibleNotes(const QRect &rect) const {
        const auto &time = view()->timeAxis();
        return m_state->timeline->notesBetween(time.toTick(rect.left()),
                                               time.toTick(rect.right() + 1));
    }

    std::pair<QPointF, double> PianoRollState::ValueLayer::handleOf(int index) const {
        const auto &note = m_state->timeline->note(index);
        const auto &time = view()->timeAxis();
        const auto lane = m_state->lane;
        const auto range = rangeOf(lane);
        const double key =
            std::clamp(keyOf(lane, m_state->valueOf(index)), range.minimum, range.maximum);
        return {QPointF(time.toX(double(note.start)), view()->keyAxis().toY(key)),
                time.toX(double(note.start + note.length))};
    }

    void PianoRollState::ValueGesture::move(QPointF position, Qt::KeyboardModifiers modifiers) {
        Q_UNUSED(modifiers);
        const auto &keys = m_state->parameters->keyAxis();
        const auto lane = m_state->lane;
        const auto range = rangeOf(lane);
        const double key =
            std::clamp(keyOf(lane, m_start) + keys.toKey(position.y()) - keys.toKey(m_origin.y()),
                       range.minimum, range.maximum);
        const double value = std::round(valueAt(lane, key));
        for (const int i : std::as_const(m_targets)) {
            m_state->valuePreview.insert(i, value);
        }
        m_state->parameters->viewport()->update();
    }

    void PianoRollState::ValueGesture::release(QPointF position, Qt::KeyboardModifiers modifiers) {
        move(position, modifiers);
        const double value = m_state->valuePreview.value(m_targets.first());
        m_state->valuePreview.clear();
        m_state->parameters->viewport()->update();
        m_state->writeValue(m_targets, value);
    }

    void PianoRollState::ValueGesture::cancel() {
        m_state->valuePreview.clear();
        m_state->parameters->viewport()->update();
    }

    // A press on a handle drags it; the right button removes the value, which leaves the default
    // of UTAU. Either applies to the selected notes if the note is selected.
    std::unique_ptr<SceneGesture>
        PianoRollState::ValueLayer::press(const SceneHit &hit, QPointF position,
                                          Qt::MouseButton button, Qt::KeyboardModifiers modifiers) {
        Q_UNUSED(modifiers);
        m_state->finishEditing(true);
        const int index = m_state->indexOf(hit.node);
        if (hit.part != PianoRoll::ParameterHandle || index < 0) {
            return nullptr;
        }
        if (button == Qt::RightButton) {
            m_state->writeValue(m_state->valueTargets(index), std::nullopt);
            return nullptr;
        }
        if (button != Qt::LeftButton) {
            return nullptr;
        }
        return std::make_unique<ValueGesture>(m_state, index, position);
    }

}
