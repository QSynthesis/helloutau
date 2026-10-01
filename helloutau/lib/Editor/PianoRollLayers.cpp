#include "PianoRollLayers_p.h"

#include <algorithm>
#include <cmath>

#include <QtWidgets/QMenu>

#include <stdutau/utaconst.h>

#include <hellokit/Synth/PitchCurve.h>

#include <helloutau/Widgets/PianoKeyboard.h>

#include "FindSupport_p.h"
#include "PianoRollGestures_p.h"

namespace hello::daw {

    void PianoRollState::GridLayer::paint(QPainter &painter, const QRect &exposed) {
        const auto &keys = view()->keyAxis();
        const auto &time = view()->timeAxis();
        const auto decl = m_state->widget;

        painter.fillRect(exposed, decl->whiteRowColor());
        const int highest = keys.keyAt(exposed.top());
        const int lowest = keys.keyAt(exposed.bottom());
        painter.setPen(decl->lineColor());
        for (int key = lowest; key <= highest; ++key) {
            const QRectF row(exposed.left(), keys.toY(key + 1), exposed.width(), keys.pixelsPerKey);
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

    std::optional<SceneHit> PianoRollState::GridLayer::hitTest(QPointF position) const {
        SceneHit hit;
        hit.part = PianoRoll::Background;
        Q_UNUSED(position);
        if (m_state->tool == PianoRoll::PenTool || m_state->drawsBend(Qt::LeftButton)) {
            hit.cursor = Qt::CrossCursor;
        }
        return hit;
    }

    bool PianoRollState::GridLayer::doubleClick(const SceneHit &hit, QPointF position) {
        Q_UNUSED(hit);
        return m_state->insertPointAt(position);
    }

    void PianoRollState::NoteLayer::paint(QPainter &painter, const QRect &exposed) {
        const auto timeline = m_state->timeline;
        painter.setRenderHint(QPainter::Antialiasing);
        if (m_state->placements.isEmpty()) {
            const auto &time = view()->timeAxis();
            const auto [begin, end] = timeline->notesBetween(time.toTick(exposed.left()),
                                                             time.toTick(exposed.right() + 1));
            for (int i = begin; i < end; ++i) {
                const auto &note = timeline->note(i);
                paintNote(painter, exposed, {i, note.start, note.length, note.key});
            }
        } else {
            for (const auto &placement : std::as_const(m_state->placements)) {
                paintNote(painter, exposed, placement);
            }
        }
        if (m_state->drawn) {
            paintNote(painter, exposed, *m_state->drawn);
        }
    }

    std::optional<SceneHit> PianoRollState::NoteLayer::hitTest(QPointF position) const {
        const auto timeline = m_state->timeline;
        const int index = timeline->noteAt(view()->timeAxis().toTick(position.x()));
        if (index < 0 || index >= timeline->noteCount()) {
            return std::nullopt;
        }
        const auto &note = timeline->note(index);
        const auto rect = m_state->rectOf(note.start, note.length, note.key);
        if (!rect.contains(position)) {
            return std::nullopt;
        }
        SceneHit hit;
        hit.node = note.id;
        hit.part = PianoRoll::NoteBody;
        if (m_state->drawsBend(Qt::LeftButton)) {
            hit.cursor = Qt::CrossCursor;
        } else if (rect.width() >= MinimumGripWidth && position.x() >= rect.right() - EndGrip) {
            hit.part = PianoRoll::NoteEnd;
            hit.cursor = Qt::SizeHorCursor;
        }
        return hit;
    }

    bool PianoRollState::NoteLayer::doubleClick(const SceneHit &hit, QPointF position) {
        // The pitch tool only draws.
        if (m_state->drawsBend(Qt::LeftButton)) {
            return true;
        }
        // On the portamento a double click inserts a point.
        if (m_state->insertPointAt(position)) {
            return true;
        }
        const int index = m_state->indexOf(hit.node);
        if (index < 0) {
            return false;
        }
        m_state->startEditing(index);
        return true;
    }

    void PianoRollState::NoteLayer::paintNote(QPainter &painter, const QRect &exposed,
                                              const Placement &placement) {
        const auto decl = m_state->widget;
        const auto rect = m_state->rectOf(placement.start, placement.length, placement.key);
        if (!rect.intersects(exposed)) {
            return;
        }
        // A note being drawn has no index, and is a sung note with the default lyric.
        const bool drawn = placement.index < 0;
        const auto *note = drawn ? nullptr : &m_state->timeline->note(placement.index);
        const bool rest = note && note->rest;
        const bool unsampled = note && decl->lacksSample(placement.index);
        const bool selected = drawn || m_state->selection.contains(note->id);

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
            painter.drawRoundedRect(body.adjusted(0.5, 0.5, -0.5, -0.5), NoteRadius, NoteRadius);
        }

        // The lyric under the editor is not drawn.
        if (note && note->id == m_state->editing) {
            return;
        }
        const auto textRect = rect.adjusted(LyricPadding, 0, -LyricPadding, 0);
        const auto lyric = drawn ? QString::fromLatin1(kit::defaultLyric) : note->lyric;
        QList<QRectF> matches;
        if (note && m_state->lyricSearch.isValid()) {
            matches = FindSupport::matchRects(QFontMetricsF(painter.font()), textRect,
                                              Qt::AlignLeft | Qt::AlignVCenter, lyric,
                                              m_state->lyricSearch.matchesIn(lyric));
        }
        for (const auto &match : std::as_const(matches)) {
            painter.fillRect(match & textRect, decl->findMatchColor());
        }
        painter.setPen(unsampled ? decl->unsampledLyricColor() : decl->lyricColor());
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, lyric);
        // The characters of a match are drawn again in their own color, clipped to the match,
        // so that they stand at the same positions as the rest of the lyric.
        for (const auto &match : std::as_const(matches)) {
            painter.save();
            painter.setClipRect(match & textRect, Qt::IntersectClip);
            painter.setPen(decl->findMatchTextColor());
            painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, lyric);
            painter.restore();
        }
    }

    void PianoRollState::NoteEnvelopeLayer::paint(QPainter &painter, const QRect &exposed) {
        // While notes are dragged, their fragments are not yet known.
        if (!m_state->envelopesVisible || !m_state->placements.isEmpty()) {
            return;
        }
        const auto decl = m_state->widget;
        const auto timeline = m_state->timeline;
        const auto &time = view()->timeAxis();
        // Fragments reach before and after their notes.
        const auto [first, last] =
            timeline->notesBetween(time.toTick(exposed.left()), time.toTick(exposed.right() + 1));
        const int begin = std::max(0, first - 1);
        const int end = std::min(timeline->noteCount(), last + 1);
        painter.setRenderHint(QPainter::Antialiasing);
        auto fill = decl->envelopeColor();
        fill.setAlphaF(fill.alphaF() * 0.15f);
        const auto refs = m_state->notes();
        for (int i = begin; i < end; ++i) {
            const auto &note = timeline->note(i);
            if (note.rest) {
                continue;
            }
            const auto outline = outlineOf(i);
            painter.setPen(QPen(decl->envelopeColor(), 1));
            painter.setBrush(fill);
            painter.drawPolygon(outline);
            painter.setPen(decl->envelopeColor());
            painter.drawText(outline.first() + QPointF(2, -view()->keyAxis().pixelsPerKey - 2),
                             QString::number(refs.at(i).intensity().value_or(100)));
        }
    }

    std::optional<SceneHit> PianoRollState::NoteEnvelopeLayer::hitTest(QPointF position) const {
        Q_UNUSED(position);
        return std::nullopt;
    }

    QPolygonF PianoRollState::NoteEnvelopeLayer::outlineOf(int index) const {
        const auto &note = m_state->timeline->note(index);
        const double base = m_state->rectOf(note.start, note.length, note.key).top();
        const double row = view()->keyAxis().pixelsPerKey;
        const auto fragment = m_state->fragmentOf(index);
        const double start = fragment.first;
        const double length = fragment.second;
        const auto envelope = m_state->envelopeOf(index);
        const auto times = anchorTimes(envelope, length);
        const auto anchors = envelope.anchorsInTimeOrder();
        const auto &map = m_state->timeline->tempoMap();
        // A volume of 100 at an intensity of 100 is one key high. The intensity scales the
        // height, as in UTAU.
        const double intensity =
            std::max(0.0, m_state->notes().at(index).intensity().value_or(100)) / 100;
        const auto pointAt = [&](double milliseconds, double volume) {
            return QPointF(view()->timeAxis().toX(map.tickOf(start + milliseconds)),
                           base - volume / 100 * intensity * row);
        };
        QPolygonF outline{pointAt(0, 0)};
        for (qsizetype k = 0; k < times.size(); ++k) {
            outline.push_back(pointAt(times[k], anchors[k].y));
        }
        outline.push_back(pointAt(length, 0));
        return outline;
    }

    void PianoRollState::NoteParameterLayer::paint(QPainter &painter, const QRect &exposed) {
        // While notes are dragged, their rows are not yet known.
        if (!m_state->parametersVisible || !m_state->placements.isEmpty()) {
            return;
        }
        const auto timeline = m_state->timeline;
        const auto &time = view()->timeAxis();
        const auto [begin, end] =
            timeline->notesBetween(time.toTick(exposed.left()), time.toTick(exposed.right() + 1));
        const auto refs = m_state->notes();
        painter.setPen(m_state->widget->palette().color(QPalette::Active, QPalette::Text));
        for (int i = begin; i < end; ++i) {
            const auto &note = timeline->note(i);
            if (note.rest) {
                continue;
            }
            const auto ref = refs.at(i);
            const auto textIn = [&](int key, const QString &text) {
                const auto rect = m_state->rectOf(note.start, note.length, key).toAlignedRect();
                painter.drawText(rect.adjusted(LyricPadding, 0, -LyricPadding, 0),
                                 Qt::AlignLeft | Qt::AlignVCenter, text);
            };
            textIn(note.key - 1, QStringLiteral("mod %1").arg(
                                     ref.modulation().value_or(utau::DEFAULT_VALUE_MODULATION)));
            if (const auto flags = ref.flags(); !flags.isEmpty()) {
                textIn(note.key - 2, flags);
            }
        }
    }

    std::optional<SceneHit> PianoRollState::NoteParameterLayer::hitTest(QPointF position) const {
        Q_UNUSED(position);
        return std::nullopt;
    }

    void PianoRollState::RenderedPitchLayer::paint(QPainter &painter, const QRect &exposed) {
        // While notes are dragged, their timings are not yet known.
        if (!m_state->renderedPitchVisible || !m_state->placements.isEmpty()) {
            return;
        }
        const auto timeline = m_state->timeline;
        const auto &time = view()->timeAxis();
        const auto &keys = view()->keyAxis();
        const int count = timeline->noteCount();
        auto [begin, end] =
            timeline->notesBetween(time.toTick(exposed.left()), time.toTick(exposed.right() + 1));
        // The sample of a note starts before the note, and ends after the next note starts.
        begin = std::max(0, begin - 1);
        end = std::min(count, end + 1);
        if (begin >= end) {
            return;
        }

        // The curve of a note reads the two notes before it and the one after.
        const int first = std::max(0, begin - 2);
        const auto notes = m_state->previewedNotes(first, std::min(count, end + 1));
        const auto &timings = m_state->sampleTimings();
        const bool mode1 = m_state->mode1();
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(m_state->widget->renderedPitchColor(), 1, Qt::DashLine));
        for (int i = begin; i < end; ++i) {
            const auto &entry = timeline->note(i);
            if (entry.rest) {
                continue;
            }
            kit::PitchCurve::Timing timing;
            timing.preUtterance = timings[i].preUtterance;
            timing.startPoint = timings[i].startPoint;
            if (i + 1 < count) {
                timing.nextPreUtterance = timings[i + 1].preUtterance;
                timing.nextOverlap = timings[i + 1].voiceOverlap;
            }
            const kit::PitchCurve curve(notes, i - first, timeline->tempoMap().tempo(i));
            const auto ticks = curve.readingTicks(timing);
            const auto values = mode1 ? curve.mode1Values(timing) : curve.values(timing);
            QPolygonF line;
            for (qsizetype k = 0; k < ticks.size(); ++k) {
                line.push_back(QPointF(time.toX(double(entry.start) + ticks[k]),
                                       keys.toY(entry.key + 0.5 + values[k] / 100.0)));
            }
            painter.drawPolyline(line);
        }
    }

    std::optional<SceneHit> PianoRollState::RenderedPitchLayer::hitTest(QPointF position) const {
        Q_UNUSED(position);
        return std::nullopt;
    }

    void PianoRollState::PitchLayer::paint(QPainter &painter, const QRect &exposed) {
        // While notes are dragged, their curves are not yet known.
        if (!m_state->pitchVisible || !m_state->placements.isEmpty()) {
            return;
        }
        const auto timeline = m_state->timeline;
        const auto &time = view()->timeAxis();
        const auto &keys = view()->keyAxis();
        const double left = time.toTick(exposed.left());
        const double right = time.toTick(exposed.right() + 1);
        const auto [begin, end] = timeline->notesBetween(left, right);
        if (begin >= end) {
            return;
        }

        // The curves of the note before the exposed notes and of the note after them may reach
        // into the exposed range. The curve of each note reads the note before it.
        const int first = std::max(0, begin - 2);
        const int last = std::min(timeline->noteCount(), end + 1);
        const auto refs = m_state->notes();
        const auto notes = m_state->previewedNotes(first, last);
        if (m_state->mode1()) {
            paintBends(painter, exposed, notes, first, begin, end);
            return;
        }

        const auto decl = m_state->widget;
        const QPen portamentoPen(decl->pitchColor(), 1.5);
        const QPen vibratoPen(decl->vibratoColor(), 1);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(Qt::NoBrush);
        const double step = std::max(1.0, CurveStep / time.pixelsPerTick);
        // Each note draws the curve of its own points alone, which crosses the curves of its
        // neighbours if its points lie among theirs. The portamento runs from the first point
        // to the last point only, and the vibrato within the note. A note without points has
        // no portamento drawn, as in UTAU, but its vibrato.
        for (int i = std::max(0, begin - 1); i < last; ++i) {
            const auto &entry = timeline->note(i);
            if (entry.rest) {
                continue;
            }
            const kit::PitchCurve curve(notes, i - first, timeline->tempoMap().tempo(i));
            const double low = left - double(entry.start) - step;
            const double high = right - double(entry.start) + step;
            const auto pointAt = [&](double tick, double cents) {
                return QPointF(time.toX(double(entry.start) + tick),
                               keys.toY(entry.key + 0.5 + cents / 100));
            };
            // Visits the ticks from from to to, both included, step apart
            const auto sample = [step](double from, double to, const auto &visit) {
                for (double tick = from;; tick = std::min(tick + step, to)) {
                    visit(tick);
                    if (tick >= to) {
                        break;
                    }
                }
            };

            QList<QPolygonF> vibrato;
            bool vibrating = false;
            if (const double from = std::max(0.0, low), to = std::min(double(entry.length), high);
                from < to) {
                sample(from, to, [&](double tick) {
                    const double v = curve.ownVibratoAt(tick);
                    if (v != 0) {
                        if (!vibrating) {
                            vibrato.push_back({});
                        }
                        vibrato.last().push_back(pointAt(tick, v));
                    }
                    vibrating = v != 0;
                });
            }
            painter.setPen(vibratoPen);
            for (const auto &run : std::as_const(vibrato)) {
                painter.drawPolyline(run);
            }

            if (notes.at(i - first).portamento.isEmpty()) {
                continue;
            }
            const auto [firstPoint, lastPoint] = curve.pointSpan();
            if (const double from = std::max(firstPoint, low), to = std::min(lastPoint, high);
                from < to) {
                QPolygonF portamento;
                sample(from, to, [&](double tick) {
                    portamento.push_back(pointAt(tick, curve.ownPortamentoAt(tick)));
                });
                painter.setPen(portamentoPen);
                painter.drawPolyline(portamento);
            }
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
            const bool faint = i != m_state->hovered;
            const auto &points = notes.at(i - first).portamento;
            const auto list = refs.at(i).portamento();
            for (int j = 0; j < points.size(); ++j) {
                const auto center = m_state->positionOf(i, j, points[j]);
                const bool selected =
                    j < list.size() && m_state->selectedPoints.contains(list.at(j).id());
                if (faint) {
                    painter.setPen(
                        QPen(selected ? decl->selectionColor() : decl->faintPointColor(), 1));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawEllipse(center, FaintPointRadius, FaintPointRadius);
                    continue;
                }
                const bool fixed = m_state->heightFixed(i, j, int(points.size()));
                painter.setPen(QPen(selected ? decl->selectionColor() : decl->pitchColor(), 1.5));
                painter.setBrush(selected ? decl->selectionColor()
                                          : (fixed ? decl->pitchColor() : decl->whiteRowColor()));
                painter.drawEllipse(center, PointRadius, PointRadius);
            }
        }

        // The handles of the vibratos: the trapezoid with its top edge stressed, and the
        // box of one period
        for (int i = begin; i < end; ++i) {
            const auto shape = m_state->vibratoShapeOf(i);
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

    std::optional<SceneHit> PianoRollState::PitchLayer::vibratoHitAt(QPointF position, int begin,
                                                                     int end) const {
        const auto grip = m_state->pointGrip;
        const auto near = [grip, position](QPointF handle) {
            return std::hypot(handle.x() - position.x(), handle.y() - position.y()) <= grip;
        };
        for (int i = begin; i < end; ++i) {
            const auto shape = m_state->vibratoShapeOf(i);
            if (!shape) {
                continue;
            }
            SceneHit hit;
            hit.node = m_state->timeline->note(i).id;
            hit.cursor = Qt::SizeHorCursor;
            const auto period = shape->period;
            const double left = std::min(shape->fadeIn.x(), shape->fadeOut.x());
            const double right = std::max(shape->fadeIn.x(), shape->fadeOut.x());
            if (near(shape->start)) {
                hit.part = PianoRoll::VibratoStart;
            } else if (near(shape->fadeIn)) {
                hit.part = PianoRoll::VibratoFadeIn;
            } else if (near(shape->fadeOut)) {
                hit.part = PianoRoll::VibratoFadeOut;
            } else if (position.x() >= left && position.x() <= right &&
                       std::abs(position.y() - shape->fadeIn.y()) <= grip) {
                hit.part = PianoRoll::VibratoDepth;
                hit.cursor = Qt::SizeVerCursor;
            } else if (std::abs(position.x() - period.right()) <= grip &&
                       position.y() >= period.top() - grip &&
                       position.y() <= period.bottom() + grip) {
                hit.part = PianoRoll::VibratoPeriod;
            } else if (period.contains(position)) {
                hit.part = PianoRoll::VibratoPhase;
            } else {
                continue;
            }
            return hit;
        }
        return std::nullopt;
    }

    std::optional<SceneHit> PianoRollState::PitchLayer::hitTest(QPointF position) const {
        if (!m_state->pointsShown() || !m_state->placements.isEmpty()) {
            return std::nullopt;
        }
        // The notes around the position, and the next one, whose points may lie before it
        const auto timeline = m_state->timeline;
        const auto &time = view()->timeAxis();
        const double grip = m_state->pointGrip / time.pixelsPerTick;
        auto [begin, end] = timeline->notesBetween(time.toTick(position.x()) - grip,
                                                   time.toTick(position.x()) + grip);
        begin = std::max(0, begin - 1);
        end = std::min(timeline->noteCount(), end + 1);

        std::optional<SceneHit> nearest;
        double distance = m_state->pointGrip;
        for (int i = begin; i < end; ++i) {
            if (timeline->note(i).rest) {
                continue;
            }
            const auto points = m_state->pointsOf(i);
            for (int j = 0; j < points.size(); ++j) {
                const auto offset = m_state->positionOf(i, j, points[j]) - position;
                const double d = std::hypot(offset.x(), offset.y());
                if (d <= distance) {
                    distance = d;
                    SceneHit hit;
                    hit.node = timeline->note(i).id;
                    hit.part = PianoRoll::PitchPoint;
                    hit.index = j;
                    hit.cursor = m_state->heightFixed(i, j, int(points.size())) ? Qt::SizeHorCursor
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

    void PianoRollState::PitchLayer::paintBends(QPainter &painter, const QRect &exposed,
                                                const QList<kit::Note> &notes, int first, int begin,
                                                int end) {
        const auto timeline = m_state->timeline;
        const auto &time = view()->timeAxis();
        const auto &keys = view()->keyAxis();
        const double left = time.toTick(exposed.left());
        const double right = time.toTick(exposed.right() + 1);
        const double step = std::max(1.0, CurveStep / time.pixelsPerTick);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(m_state->widget->pitchColor(), 1.5));
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
                from = std::min(0.0, m_state->ticksOf(bend->start.value_or(0), i));
            }
            from = std::max(from, left - double(entry.start) - step);
            const double to = std::min(double(entry.length), right - double(entry.start) + step);
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

    void PianoRollState::PitchLayer::showMenu(int index, int j, QPointF position) {
        const auto points = m_state->pointsOf(index);
        QMenu menu(view());
        const std::pair<kit::PortamentoPoint::Type, const char *> shapes[] = {
            {kit::PortamentoPoint::S,      QT_TRANSLATE_NOOP("hello::daw::PianoRoll", "S-Curve")},
            {kit::PortamentoPoint::Linear, QT_TRANSLATE_NOOP("hello::daw::PianoRoll", "Linear") },
            {kit::PortamentoPoint::R,      QT_TRANSLATE_NOOP("hello::daw::PianoRoll", "R-Curve")},
            {kit::PortamentoPoint::J,      QT_TRANSLATE_NOOP("hello::daw::PianoRoll", "J-Curve")},
        };
        for (const auto &[type, name] : shapes) {
            const auto action = menu.addAction(PianoRoll::tr(name));
            action->setCheckable(true);
            action->setChecked(points[j].type == type);
            // The shape is that of the segment that ends at a point, which the first lacks.
            action->setEnabled(j > 0);
            QObject::connect(action, &QAction::triggered, view(), [this, index, j, type = type] {
                auto changed = m_state->pointsOf(index);
                if (j >= changed.size() || changed[j].type == type) {
                    return;
                }
                changed[j].type = type;
                kit::DiagnosticList diagnostics;
                m_state->writePoints(PianoRoll::tr("Change Pitch Point"),
                                     {
                                         {index, changed}
                },
                                     diagnostics);
                m_state->report(diagnostics);
            });
        }
        menu.addSeparator();
        const auto remove = menu.addAction(PianoRoll::tr("Delete Point"));
        remove->setEnabled(points.size() > 2);
        QObject::connect(remove, &QAction::triggered, view(), [this, index, j] {
            kit::DiagnosticList diagnostics;
            m_state->removePoints(
                {
                    {index, {j}}
            },
                diagnostics);
            m_state->report(diagnostics);
        });
        menu.exec(view()->viewport()->mapToGlobal(position.toPoint()));
    }

    bool PianoRollState::PitchLayer::doubleClick(const SceneHit &hit, QPointF position) {
        Q_UNUSED(position);
        if (hit.part != PianoRoll::PitchPoint) {
            return false;
        }
        const int index = m_state->indexOf(hit.node);
        if (index < 0) {
            return false;
        }
        if (m_state->pointsOf(index).size() <= 2) {
            Q_EMIT m_state->widget->editRefused(
                PianoRoll::tr("A note keeps at least two pitch points."));
            return true;
        }
        kit::DiagnosticList diagnostics;
        m_state->removePoints(
            {
                {index, {hit.index}}
        },
            diagnostics);
        m_state->report(diagnostics);
        return true;
    }

    void PianoRollState::OverlayLayer::paint(QPainter &painter, const QRect &exposed) {
        if (m_state->band) {
            auto color = m_state->widget->selectionColor();
            painter.setPen(QPen(color, 1));
            color.setAlphaF(0.15f);
            painter.setBrush(color);
            painter.drawRect(*m_state->band);
        }
        // The playhead where playback is, or at rest
        if (m_state->playhead || m_state->cursorEnabled) {
            const double x = view()->timeAxis().toX(m_state->playhead.value_or(m_state->cursor));
            painter.setPen(QPen(m_state->widget->playheadColor(), 1));
            painter.drawLine(QPointF(x, exposed.top()), QPointF(x, exposed.bottom() + 1));
        }
    }

    std::unique_ptr<SceneGesture>
        PianoRollState::PitchLayer::press(const SceneHit &hit, QPointF position,
                                          Qt::MouseButton button, Qt::KeyboardModifiers modifiers) {
        m_state->finishEditing(true);
        const int index = m_state->indexOf(hit.node);
        if (index < 0) {
            return nullptr;
        }
        if (hit.part != PianoRoll::PitchPoint) {
            if (button != Qt::LeftButton) {
                return nullptr;
            }
            return std::make_unique<VibratoGesture>(m_state, index, hit.part, position);
        }
        const auto list = m_state->notes().at(index).portamento();
        if (hit.index < 0 || hit.index >= list.size()) {
            return nullptr;
        }
        const auto id = list.at(hit.index).id();
        if (button == Qt::RightButton) {
            if (!m_state->selectedPoints.contains(id)) {
                m_state->selectPoints({id});
            }
            showMenu(index, hit.index, position);
            return nullptr;
        }
        if (button != Qt::LeftButton) {
            return nullptr;
        }
        if (modifiers & Qt::ControlModifier) {
            auto ids = m_state->selectedPoints;
            if (!ids.remove(id)) {
                ids.insert(id);
            }
            m_state->selectPoints(ids);
            return nullptr;
        }
        if (!m_state->selectedPoints.contains(id)) {
            m_state->selectPoints({id});
        }
        return std::make_unique<PointGesture>(m_state, index, hit.index, position);
    }

    std::unique_ptr<SceneGesture>
        PianoRollState::GridLayer::press(const SceneHit &hit, QPointF position,
                                         Qt::MouseButton button, Qt::KeyboardModifiers modifiers) {
        Q_UNUSED(hit);
        if (m_state->drawsBend(button)) {
            return m_state->bendGesture(position, button);
        }
        if (button == Qt::RightButton) {
            m_state->finishEditing(true);
            return std::make_unique<SpanGesture>(m_state, position, modifiers);
        }
        if (button != Qt::LeftButton) {
            return nullptr;
        }
        m_state->finishEditing(true);
        if (m_state->tool == PianoRoll::PenTool) {
            m_state->selectPoints({});
            return std::make_unique<DrawGesture>(m_state, position, modifiers);
        }
        return std::make_unique<BandGesture>(m_state, position, modifiers);
    }

    std::unique_ptr<SceneGesture>
        PianoRollState::NoteLayer::press(const SceneHit &hit, QPointF position,
                                         Qt::MouseButton button, Qt::KeyboardModifiers modifiers) {
        if (m_state->drawsBend(button)) {
            return m_state->bendGesture(position, button);
        }
        if (button == Qt::RightButton) {
            m_state->finishEditing(true);
            return std::make_unique<SpanGesture>(m_state, position, modifiers);
        }
        if (button != Qt::LeftButton) {
            return nullptr;
        }
        m_state->finishEditing(true);
        m_state->selectPoints({});
        const int index = m_state->indexOf(hit.node);
        if (index < 0) {
            return nullptr;
        }
        if (hit.part == PianoRoll::NoteEnd) {
            m_state->selectOnly(index);
            return std::make_unique<LengthGesture>(m_state, index);
        }
        if (modifiers & Qt::ControlModifier) {
            auto ids = m_state->selection;
            const auto id = m_state->timeline->note(index).id;
            if (!ids.remove(id)) {
                ids.insert(id);
            }
            m_state->anchor = id;
            m_state->setSelection(ids);
            return nullptr;
        }
        if (modifiers & Qt::ShiftModifier) {
            const int anchor = m_state->indexOf(m_state->anchor);
            m_state->selectRange(anchor < 0 ? index : anchor, index);
            return nullptr;
        }
        const bool wasSelected = m_state->isSelected(index);
        if (!wasSelected) {
            m_state->selectOnly(index);
        }
        return std::make_unique<MoveGesture>(m_state, index, position, wasSelected);
    }

}
