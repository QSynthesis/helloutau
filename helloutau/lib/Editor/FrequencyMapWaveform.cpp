#include "FrequencyMapWaveform.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtGui/QPainter>
#include <QtGui/QWheelEvent>

#include <stdcorelib/pimpl.h>

#include "FrequencyMapView.h"

namespace hello::daw {

    class FrequencyMapWaveform::Impl {
    public:
        using Decl = FrequencyMapWaveform;

        Impl(Decl *decl, FrequencyMapView *view) : _decl(decl), view(view) {
        }

        Decl *_decl;

        QPointer<FrequencyMapView> view;
        std::shared_ptr<const kit::WaveAudio> audio;
        std::optional<double> playhead;
        QColor waveColor, playheadColor;

        // The peaks drawn for the state they were computed for
        struct Peaks {
            const kit::WaveAudio *audio = nullptr;
            double start = 0;
            double scale = 0;
            int width = 0;
            std::vector<std::vector<std::pair<float, float>>> columns;
        } peaks;

        // Returns the x of the widget of the x \a x of the viewport of the view.
        double fromViewport(double x) const {
            const auto viewport = view->viewport();
            return _decl->mapFromGlobal(viewport->mapToGlobal(QPointF(x, 0))).x();
        }

        const std::vector<std::vector<std::pair<float, float>>> &columns(double start, int width) {
            const double scale = view->timeAxis().pixelsPerTick;
            if (peaks.audio != audio.get() || peaks.start != start || peaks.scale != scale ||
                peaks.width != width) {
                peaks = {audio.get(), start, scale, width,
                         audio ? audio->peaks(start, scale, width)
                               : std::vector<std::vector<std::pair<float, float>>>()};
            }
            return peaks.columns;
        }
    };

    FrequencyMapWaveform::FrequencyMapWaveform(FrequencyMapView *view, QWidget *parent)
        : QFrame(parent), _impl(std::make_unique<Impl>(this, view)) {
        setFrameStyle(view->frameStyle());
        connect(view, &SceneView::timeAxisChanged, this, qOverload<>(&QWidget::update));
        // The viewport of the view moves or changes its width with the scroll bar and the column
        // of labels.
        view->viewport()->installEventFilter(this);
    }

    FrequencyMapWaveform::~FrequencyMapWaveform() = default;

    std::shared_ptr<const kit::WaveAudio> FrequencyMapWaveform::audio() const {
        stdc_impl_t;
        return impl.audio;
    }

    void FrequencyMapWaveform::setAudio(std::shared_ptr<const kit::WaveAudio> audio) {
        stdc_impl_t;
        impl.audio = std::move(audio);
        update();
    }

    std::optional<double> FrequencyMapWaveform::playhead() const {
        stdc_impl_t;
        return impl.playhead;
    }

    void FrequencyMapWaveform::setPlayhead(std::optional<double> time) {
        stdc_impl_t;
        if (impl.playhead != time) {
            impl.playhead = time;
            update();
        }
    }

    double FrequencyMapWaveform::xOf(double time) const {
        stdc_impl_t;
        return impl.fromViewport(impl.view->xOf(time));
    }

    QColor FrequencyMapWaveform::waveColor() const {
        stdc_impl_t;
        return impl.waveColor.isValid() ? impl.waveColor : QColor(0x3a, 0x7b, 0xd5);
    }

    void FrequencyMapWaveform::setWaveColor(const QColor &color) {
        stdc_impl_t;
        impl.waveColor = color;
        update();
    }

    QColor FrequencyMapWaveform::playheadColor() const {
        stdc_impl_t;
        return impl.playheadColor.isValid() ? impl.playheadColor : QColor(0xff, 0x80, 0x00);
    }

    void FrequencyMapWaveform::setPlayheadColor(const QColor &color) {
        stdc_impl_t;
        impl.playheadColor = color;
        update();
    }

    QSize FrequencyMapWaveform::sizeHint() const {
        return QSize(200, 100);
    }

    QSize FrequencyMapWaveform::minimumSizeHint() const {
        return QSize(50, 30);
    }

    void FrequencyMapWaveform::paintEvent(QPaintEvent *event) {
        stdc_impl_t;
        QFrame::paintEvent(event);
        const auto view = impl.view.data();
        if (!view) {
            return;
        }
        QPainter painter(this);
        const QRect area = contentsRect();
        painter.setClipRect(area);
        painter.fillRect(area, view->whiteRowColor());

        // Below the plot of the view only, from the right of the column of labels to the right of
        // the viewport
        const auto viewport = view->viewport();
        const int left =
            std::max(area.left(), int(std::lround(impl.fromViewport(view->labelWidth()))));
        const int right =
            std::min(area.right() + 1, int(std::lround(impl.fromViewport(viewport->width()))));
        if (right <= left) {
            return;
        }
        painter.setClipRect(QRect(left, area.top(), right - left, area.height()));
        const double start = view->timeAxis().toTick(view->labelWidth());
        const auto &columns = impl.columns(start, right - left);
        const int lanes = std::max(1, int(columns.size()));
        const double laneHeight = double(area.height()) / lanes;
        const auto yOf = [&](int lane, double sample) {
            return area.top() + laneHeight * (lane + (1 - sample) / 2);
        };
        for (int c = 0; c < int(columns.size()); ++c) {
            painter.setPen(view->rowLineColor());
            painter.drawLine(QPointF(left, yOf(c, 0)), QPointF(right, yOf(c, 0)));
            painter.setPen(QPen(waveColor(), 1));
            const auto &lane = columns[size_t(c)];
            for (int x = 0; x < int(lane.size()); ++x) {
                const auto [low, high] = lane[size_t(x)];
                painter.drawLine(QPointF(left + x + 0.5, yOf(c, high)),
                                 QPointF(left + x + 0.5, yOf(c, low)));
            }
        }
        painter.setPen(view->rowLineColor());
        for (int c = 1; c < lanes; ++c) {
            const double y = std::round(area.top() + c * laneHeight) + 0.5;
            painter.drawLine(QPointF(left, y), QPointF(right, y));
        }
        if (impl.playhead) {
            const double x = std::floor(xOf(*impl.playhead)) + 0.5;
            painter.setPen(QPen(playheadColor(), 1));
            painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom() + 1));
        }
    }

    void FrequencyMapWaveform::wheelEvent(QWheelEvent *event) {
        stdc_impl_t;
        const auto view = impl.view.data();
        if (!view) {
            QFrame::wheelEvent(event);
            return;
        }
        // At the same x as the event and in the middle of the viewport, so that a zoom of the
        // keys keeps the middle in place
        const auto viewport = view->viewport();
        QPointF position = viewport->mapFromGlobal(event->globalPosition());
        position.setY(viewport->height() / 2.0);
        QWheelEvent forwarded(position, viewport->mapToGlobal(position), event->pixelDelta(),
                              event->angleDelta(), event->buttons(), event->modifiers(),
                              event->phase(), event->inverted(), event->source());
        QCoreApplication::sendEvent(viewport, &forwarded);
        event->accept();
    }

    bool FrequencyMapWaveform::eventFilter(QObject *watched, QEvent *event) {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Move) {
            update();
        }
        return QFrame::eventFilter(watched, event);
    }

}
