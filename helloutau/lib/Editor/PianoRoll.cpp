#include "PianoRoll.h"

#include <algorithm>
#include <cmath>

#include <QtCore/QTimer>
#include <QtGui/QPainter>
#include <QtWidgets/QGridLayout>

#include <hellokit/Document/DocumentConstants.h>
#include <hellokit/Edit/ProjectSession.h>
#include <hellokit/Edit/TrackTimeline.h>
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

        QString tempoText(double tempo) {
            return QString::number(tempo, 'g', 6);
        }

        // The rows of the keys and the lines of the bars and beats
        class GridLayer : public SceneLayer {
        public:
            explicit GridLayer(const PianoRoll *roll) : m_roll(roll) {
            }

            void paint(QPainter &painter, const QRect &exposed) override {
                const auto &keys = view()->keyAxis();
                const auto &time = view()->timeAxis();

                painter.fillRect(exposed, m_roll->whiteRowColor());
                const int highest = keys.keyAt(exposed.top());
                const int lowest = keys.keyAt(exposed.bottom());
                painter.setPen(m_roll->lineColor());
                for (int key = lowest; key <= highest; ++key) {
                    const QRectF row(exposed.left(), keys.toY(key + 1), exposed.width(),
                                     keys.pixelsPerKey);
                    if (PianoKeyboard::isBlackKey(key)) {
                        painter.fillRect(row, m_roll->blackRowColor());
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
                    painter.setPen(bar ? m_roll->barLineColor() : m_roll->lineColor());
                    const double x = time.toX(double(beat) * kit::ticksPerQuarter);
                    painter.drawLine(QPointF(x, exposed.top()), QPointF(x, exposed.bottom() + 1));
                }
            }

        private:
            const PianoRoll *m_roll;
        };

        // The notes, each a bar in the row of its key with its lyric
        class NoteLayer : public SceneLayer {
        public:
            NoteLayer(const PianoRoll *roll, const kit::TrackTimeline *timeline)
                : m_roll(roll), m_timeline(timeline) {
            }

            void paint(QPainter &painter, const QRect &exposed) override {
                const auto &time = view()->timeAxis();
                const auto [begin, end] = m_timeline->notesBetween(
                    time.toTick(exposed.left()), time.toTick(exposed.right() + 1));
                painter.setRenderHint(QPainter::Antialiasing);
                for (int i = begin; i < end; ++i) {
                    const auto &note = m_timeline->note(i);
                    const auto rect = rectOf(note);
                    if (!rect.intersects(exposed)) {
                        continue;
                    }
                    const bool unsampled = m_roll->lacksSample(i);
                    if (unsampled) {
                        painter.setPen(QPen(m_roll->unsampledColor(), 1));
                        painter.setBrush(Qt::NoBrush);
                    } else {
                        painter.setPen(Qt::NoPen);
                        painter.setBrush(note.rest ? m_roll->restColor() : m_roll->noteColor());
                    }
                    painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), NoteRadius,
                                            NoteRadius);
                    painter.setPen(unsampled ? m_roll->unsampledLyricColor()
                                             : m_roll->lyricColor());
                    painter.drawText(rect.adjusted(LyricPadding, 0, -LyricPadding, 0),
                                     Qt::AlignLeft | Qt::AlignVCenter, note.lyric);
                }
            }

            std::optional<SceneHit> hitTest(QPointF position) const override {
                const int index = m_timeline->noteAt(view()->timeAxis().toTick(position.x()));
                if (index < 0 || index >= m_timeline->noteCount()) {
                    return std::nullopt;
                }
                const auto &note = m_timeline->note(index);
                if (!rectOf(note).contains(position)) {
                    return std::nullopt;
                }
                SceneHit hit;
                hit.node = note.id;
                hit.part = PianoRoll::NoteBody;
                return hit;
            }

        private:
            const PianoRoll *m_roll;
            const kit::TrackTimeline *m_timeline;

            QRectF rectOf(const kit::TrackTimeline::Note &note) const {
                const auto &time = view()->timeAxis();
                const auto &keys = view()->keyAxis();
                return {time.toX(double(note.start)), keys.toY(note.key + 1),
                        note.length * time.pixelsPerTick, keys.pixelsPerKey};
            }
        };

    }

    class PianoRoll::Impl {
    public:
        explicit Impl(PianoRoll *decl) : _decl(decl) {
        }

        PianoRoll *_decl;
        kit::TrackTimeline *timeline = nullptr;
        SceneView *view = nullptr;
        TimelineRuler *ruler = nullptr;
        PianoKeyboard *keyboard = nullptr;
        bool refreshPending = false;
        std::shared_ptr<const kit::VoiceBank> voiceBank;

        QColor noteColor;
        QColor restColor;
        QColor lyricColor;
        QColor unsampledColor;
        QColor unsampledLyricColor;
        QColor whiteRowColor;
        QColor blackRowColor;
        QColor lineColor;
        QColor barLineColor;

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
    };

    PianoRoll::PianoRoll(kit::ProjectSession *session, QWidget *parent)
        : QWidget(parent), _impl(std::make_unique<Impl>(this)) {
        _impl->timeline = new kit::TrackTimeline(session, 0, this);
        _impl->view = new SceneView();
        _impl->ruler = new TimelineRuler(_impl->view);
        _impl->keyboard = new PianoKeyboard(_impl->view);
        _impl->ruler->setTicksPerBeat(kit::ticksPerQuarter);
        _impl->ruler->setBeatsPerBar(BeatsPerBar);

        _impl->view->addLayer(std::make_unique<GridLayer>(this));
        _impl->view->addLayer(std::make_unique<NoteLayer>(this, _impl->timeline));

        auto layout = new QGridLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(_impl->ruler, 0, 1);
        layout->addWidget(_impl->keyboard, 1, 0);
        layout->addWidget(_impl->view, 1, 1);
        layout->setColumnStretch(1, 1);
        layout->setRowStretch(1, 1);

        connect(_impl->timeline, &kit::TrackTimeline::invalidated, this, [this] {
            _impl->view->viewport()->update();
            _impl->scheduleRefresh();
        });
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
