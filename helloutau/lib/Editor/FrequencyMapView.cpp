#include "FrequencyMapView.h"

#include <algorithm>
#include <cmath>

#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>

#include <stdcorelib/pimpl.h>

#include <hellokit/AudioFile/Spectrogram.h>
#include <hellokit/VoiceBank/FrequencyMapRevisionBuffer.h>

#include <helloutau/Widgets/SceneGestures.h>
#include <helloutau/Widgets/SceneLayer.h>

#include "FrequencyViewKeyboardModifiers.h"

namespace hello::daw {

    namespace {

        double noteOf(double frequency) {
            return 69 + 12 * std::log2(frequency / 440);
        }

        double frequencyOf(double note) {
            return 440 * std::exp2((note - 69) / 12);
        }

        using Role = FrequencyViewKeyboardModifiers::Role;

    }

    class FrequencyMapView::Impl {
    public:
        using Decl = FrequencyMapView;

        explicit Impl(Decl *decl) : _decl(decl) {
        }

        Decl *_decl;

        kit::FrequencyMapRevisionBuffer *buffer = nullptr;
        std::shared_ptr<const kit::Spectrogram> spectrogram;
        KeyboardModifierBindings bindings{FrequencyViewKeyboardModifiers::scheme()};
        std::optional<std::pair<std::size_t, std::size_t>> selection;
        // Set until the view has a size to fit the map into
        bool fitPending = false;
        QColor frequencyColor, savedFrequencyColor, spectrumColor, selectionColor;

        // The spectrogram drawn for the axes and the size it was drawn for
        struct SpectrumImage {
            QImage image;
            TimeAxis time;
            KeyAxis keys;
            QSize size;
            const kit::Spectrogram *spectrogram = nullptr;
            QRgb background = 0;
            QRgb color = 0;
        } spectrumImage;

        const std::vector<kit::FrequencyMap::Frame> *frames() const {
            return buffer ? &buffer->map().frames : nullptr;
        }

        // Returns the time in milliseconds after the last frame, the end of the scene.
        double endTime() const {
            const auto list = frames();
            if (!list || list->empty()) {
                return 1000;
            }
            const double last = list->back().time;
            const double spacing = list->size() > 1 ? last - (*list)[list->size() - 2].time : 10;
            return last + std::max(spacing, 1.0);
        }

        // Fits the whole map across and its voiced notes up and down, an octave beyond each end.
        void fit() {
            stdc_decl_t;
            const auto viewport = decl.viewport();
            if (viewport->width() <= 0 || viewport->height() <= 0) {
                fitPending = true;
                return;
            }
            fitPending = false;
            const double end = endTime();
            decl.setTickRange(0, end);
            double low = 1e9;
            double high = -1e9;
            if (const auto list = frames()) {
                for (const auto &frame : *list) {
                    if (frame.frequency > kit::FrequencyMapRevisionBuffer::lowestFrequency) {
                        low = std::min(low, noteOf(frame.frequency));
                        high = std::max(high, noteOf(frame.frequency));
                    }
                }
            }
            if (low > high) {
                low = 48;
                high = 72;
            }
            low = std::max(double(lowestNote), std::floor(low) - 12);
            high = std::min(double(highestNote + 1), std::ceil(high) + 12);
            decl.setTimeAxis({viewport->width() / end, 0});
            decl.setKeyAxis({viewport->height() / (high - low), high});
        }

        const QImage &spectrumImageOf(const QSize &size) {
            stdc_decl_t;
            const auto background = decl.palette().color(QPalette::Base).rgb();
            const auto color = decl.spectrumColor().rgb();
            auto &cached = spectrumImage;
            const auto &time = decl.timeAxis();
            const auto &keys = decl.keyAxis();
            if (cached.spectrogram == spectrogram.get() && cached.time == time &&
                cached.keys == keys && cached.size == size && cached.background == background &&
                cached.color == color) {
                return cached.image;
            }
            cached = {QImage(size, QImage::Format_RGB32),
                      time,
                      keys,
                      size,
                      spectrogram.get(),
                      background,
                      color};
            const auto &data = *spectrogram;
            const double peak = std::max(1e-9f, data.peak());
            std::vector<int> bins(size_t(std::max(0, size.height())));
            for (int y = 0; y < size.height(); ++y) {
                bins[size_t(y)] =
                    int(std::lround(data.binOf(frequencyOf(keys.toKey(y + 0.5) - 0.5))));
            }
            const auto blend = [&](double level) {
                const auto mix = [level](int a, int b) { return int(a + (b - a) * level); };
                return qRgb(mix(qRed(background), qRed(color)),
                            mix(qGreen(background), qGreen(color)),
                            mix(qBlue(background), qBlue(color)));
            };
            for (int x = 0; x < size.width(); ++x) {
                const int frame = int(std::lround(time.toTick(x + 0.5) * data.sampleRate() / 1000 /
                                                  kit::Spectrogram::hopSize));
                for (int y = 0; y < size.height(); ++y) {
                    const float magnitude = data.magnitude(frame, bins[size_t(y)]);
                    // 60 decibels below the peak and down are the background.
                    const double level =
                        magnitude > 0
                            ? std::clamp((20 * std::log10(magnitude / peak) + 60) / 60, 0.0, 1.0)
                            : 0.0;
                    cached.image.setPixel(x, y, blend(level));
                }
            }
            return cached.image;
        }

        void setSelection(std::optional<std::pair<std::size_t, std::size_t>> value) {
            stdc_decl_t;
            if (value == selection) {
                return;
            }
            selection = value;
            decl.viewport()->update();
            Q_EMIT decl.selectionChanged();
        }

        // Returns the selection between two frames, both included.
        static std::pair<std::size_t, std::size_t> between(std::size_t a, std::size_t b) {
            return {std::min(a, b), std::max(a, b) - std::min(a, b) + 1};
        }

        class StrokeGesture;
        class SelectGesture;
        class BackgroundLayer;
        class CurveLayer;
    };

    // A stroke of the buffer, which draws the frequency under the pointer, or unvoices the frames
    // under the pointer
    class FrequencyMapView::Impl::StrokeGesture : public SceneGesture {
    public:
        StrokeGesture(Impl *impl, QPointF origin, bool unvoices)
            : m_impl(impl), m_unvoices(unvoices) {
            m_impl->buffer->beginStroke(*m_impl->_decl->frameAt(origin.x()), frequencyAt(origin));
        }

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override {
            Q_UNUSED(modifiers);
            m_impl->buffer->strokeTo(*m_impl->_decl->frameAt(position.x()), frequencyAt(position));
            m_impl->_decl->viewport()->update();
        }

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override {
            move(position, modifiers);
            if (m_impl->buffer->endStroke()) {
                Q_EMIT m_impl->_decl->edited();
            }
        }

        void cancel() override {
            m_impl->buffer->cancelStroke();
            m_impl->_decl->viewport()->update();
        }

        bool hasAutoScroll() const override {
            return true;
        }

    private:
        Impl *m_impl;
        bool m_unvoices;

        double frequencyAt(QPointF position) const {
            return m_unvoices ? 0 : m_impl->_decl->frequencyAt(position.y());
        }
    };

    // A drag that selects the frames between the press and the pointer
    class FrequencyMapView::Impl::SelectGesture : public SceneGesture {
    public:
        SelectGesture(Impl *impl, QPointF origin)
            : m_impl(impl), m_anchor(*impl->_decl->frameAt(origin.x())),
              m_previous(impl->selection) {
            m_impl->setSelection(between(m_anchor, m_anchor));
        }

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override {
            Q_UNUSED(modifiers);
            m_impl->setSelection(between(m_anchor, *m_impl->_decl->frameAt(position.x())));
        }

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override {
            move(position, modifiers);
        }

        void cancel() override {
            m_impl->setSelection(m_previous);
        }

        bool hasAutoScroll() const override {
            return true;
        }

    private:
        Impl *m_impl;
        std::size_t m_anchor;
        std::optional<std::pair<std::size_t, std::size_t>> m_previous;
    };

    // The background, the spectrogram if any, and a line at each C
    class FrequencyMapView::Impl::BackgroundLayer : public SceneLayer {
    public:
        explicit BackgroundLayer(Impl *impl) : m_impl(impl) {
        }

        void paint(QPainter &painter, const QRect &exposed) override {
            Q_UNUSED(exposed);
            const auto view = m_impl->_decl;
            const QRect area = view->viewport()->rect();
            painter.fillRect(area, view->palette().color(QPalette::Base));
            if (m_impl->spectrogram) {
                painter.drawImage(area.topLeft(), m_impl->spectrumImageOf(area.size()));
            }
            const auto &keys = view->keyAxis();
            const int low = int(std::floor(keys.toKey(area.height())));
            const int high = int(std::ceil(keys.toKey(0)));
            painter.setRenderHint(QPainter::Antialiasing, false);
            for (int note = (low + 11) / 12 * 12; note <= high; note += 12) {
                const double y = std::round(keys.toY(note + 0.5)) + 0.5;
                painter.setPen(QPen(view->palette().color(QPalette::Mid), 1, Qt::DotLine));
                painter.drawLine(QPointF(0, y), QPointF(area.width(), y));
                painter.setPen(view->palette().color(QPalette::Text));
                painter.drawText(QRectF(0, y - 14, area.width() - 3, 13), Qt::AlignRight,
                                 QStringLiteral("C%1").arg(note / 12 - 1));
            }
        }

    private:
        Impl *m_impl;
    };

    // The selection, the curve as last saved while the map is modified, and the curve, with a
    // dot at each frame once the frames are far enough apart
    class FrequencyMapView::Impl::CurveLayer : public SceneLayer {
    public:
        explicit CurveLayer(Impl *impl) : m_impl(impl) {
        }

        void paint(QPainter &painter, const QRect &exposed) override {
            Q_UNUSED(exposed);
            const auto view = m_impl->_decl;
            const auto buffer = m_impl->buffer;
            if (!buffer) {
                return;
            }
            const QRect area = view->viewport()->rect();
            const auto &frames = buffer->map().frames;
            if (const auto selection = m_impl->selection; selection && !frames.empty()) {
                const auto first = frames[selection->first].time;
                const auto last = frames[selection->first + selection->second - 1].time;
                const double spacing = frames.size() > 1 ? frames[1].time - frames[0].time : 0;
                painter.fillRect(QRectF(QPointF(view->xOf(first - spacing / 2), 0),
                                        QPointF(view->xOf(last + spacing / 2), area.height())),
                                 view->selectionColor());
            }
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setBrush(Qt::NoBrush);
            if (buffer->isModified()) {
                painter.setPen(QPen(view->savedFrequencyColor(), 1, Qt::DashLine));
                painter.drawPath(curveOf(buffer->savedMap().frames));
            }
            painter.setPen(QPen(view->frequencyColor(), 1.5));
            painter.drawPath(curveOf(frames));
            const double spacing =
                frames.size() > 1 ? view->xOf(frames[1].time) - view->xOf(frames[0].time) : 0;
            if (spacing >= 6) {
                painter.setBrush(view->frequencyColor());
                painter.setPen(Qt::NoPen);
                for (const auto &frame : frames) {
                    if (voiced(frame)) {
                        painter.drawEllipse(
                            QPointF(view->xOf(frame.time), view->yOf(frame.frequency)), 2, 2);
                    }
                }
            }
        }

        std::optional<SceneHit> hitTest(QPointF position) const override {
            Q_UNUSED(position);
            if (!m_impl->buffer || m_impl->buffer->map().frames.empty()) {
                return std::nullopt;
            }
            SceneHit hit;
            hit.cursor = Qt::CrossCursor;
            return hit;
        }

        std::unique_ptr<SceneGesture> press(const SceneHit &hit, QPointF position,
                                            Qt::MouseButton button,
                                            Qt::KeyboardModifiers modifiers) override {
            Q_UNUSED(hit);
            Q_UNUSED(modifiers);
            const auto impl = m_impl;
            if (button == Qt::RightButton) {
                return std::make_unique<PressGesture>(
                    position,
                    [impl, position](Qt::KeyboardModifiers held) -> std::unique_ptr<SceneGesture> {
                        const auto activation = impl->bindings.activate(
                            FrequencyViewKeyboardModifiers::RightDragScene, held);
                        if (!activation || activation->operation != Role::Unvoice) {
                            return nullptr;
                        }
                        return std::make_unique<StrokeGesture>(impl, position, true);
                    },
                    nullptr);
            }
            if (button != Qt::LeftButton) {
                return nullptr;
            }
            return std::make_unique<PressGesture>(
                position,
                [impl, position](Qt::KeyboardModifiers held) -> std::unique_ptr<SceneGesture> {
                    const auto activation = impl->bindings.activate(
                        FrequencyViewKeyboardModifiers::LeftDragScene, held);
                    if (!activation) {
                        return nullptr;
                    }
                    switch (activation->operation) {
                        case Role::Draw:
                            return std::make_unique<StrokeGesture>(impl, position, false);
                        case Role::SelectFrames:
                            return std::make_unique<SelectGesture>(impl, position);
                        case Role::DragZoom:
                            return std::make_unique<SceneZoomGesture>(
                                impl->_decl, position,
                                [impl, activation =
                                           *activation](Qt::KeyboardModifiers modifiers) mutable {
                                    impl->bindings.updateToggles(activation, modifiers);
                                    return activation.isOn(Role::DragZoomAxisLock);
                                });
                        default:
                            return nullptr;
                    }
                },
                nullptr);
        }

    private:
        Impl *m_impl;

        static bool voiced(const kit::FrequencyMap::Frame &frame) {
            return frame.frequency > kit::FrequencyMapRevisionBuffer::lowestFrequency &&
                   std::isfinite(frame.frequency);
        }

        // Returns the curve of frames, broken at each unvoiced frame.
        QPainterPath curveOf(const std::vector<kit::FrequencyMap::Frame> &frames) const {
            const auto view = m_impl->_decl;
            QPainterPath curve;
            bool drawing = false;
            for (const auto &frame : frames) {
                if (!voiced(frame)) {
                    drawing = false;
                    continue;
                }
                const QPointF point(view->xOf(frame.time), view->yOf(frame.frequency));
                if (drawing) {
                    curve.lineTo(point);
                } else {
                    curve.moveTo(point);
                    drawing = true;
                }
            }
            return curve;
        }
    };

    FrequencyMapView::FrequencyMapView(QWidget *parent)
        : SceneView(parent), _impl(std::make_unique<Impl>(this)) {
        stdc_impl_t;
        setKeyRange(lowestNote, highestNote);
        setTimeScaleRange(0.01, 20);
        setKeyScaleRange(2, 64);
        addLayer(std::make_unique<Impl::BackgroundLayer>(&impl));
        addLayer(std::make_unique<Impl::CurveLayer>(&impl));
        setWheelActions([this](Qt::KeyboardModifiers modifiers) -> std::optional<WheelAction> {
            stdc_impl_t;
            const auto activation =
                impl.bindings.activate(FrequencyViewKeyboardModifiers::WheelScene, modifiers);
            if (!activation) {
                return std::nullopt;
            }
            switch (activation->operation) {
                case Role::VerticalScroll:
                    return VerticalScroll;
                case Role::HorizontalScroll:
                    return HorizontalScroll;
                case Role::TimeZoom:
                    return TimeZoom;
                case Role::KeyZoom:
                    return KeyZoom;
                default:
                    return std::nullopt;
            }
        });
    }

    FrequencyMapView::~FrequencyMapView() = default;

    kit::FrequencyMapRevisionBuffer *FrequencyMapView::buffer() const {
        stdc_impl_t;
        return impl.buffer;
    }

    void FrequencyMapView::setBuffer(kit::FrequencyMapRevisionBuffer *buffer) {
        stdc_impl_t;
        Q_ASSERT(!hasGesture());
        impl.buffer = buffer;
        impl.setSelection(std::nullopt);
        impl.fit();
        viewport()->update();
    }

    std::shared_ptr<const kit::Spectrogram> FrequencyMapView::spectrogram() const {
        stdc_impl_t;
        return impl.spectrogram;
    }

    void FrequencyMapView::setSpectrogram(std::shared_ptr<const kit::Spectrogram> spectrogram) {
        stdc_impl_t;
        impl.spectrogram = std::move(spectrogram);
        viewport()->update();
    }

    void FrequencyMapView::setKeyboardModifierBindings(const KeyboardModifierBindings &bindings) {
        stdc_impl_t;
        impl.bindings = bindings;
    }

    std::optional<std::pair<std::size_t, std::size_t>> FrequencyMapView::selection() const {
        stdc_impl_t;
        return impl.selection;
    }

    void FrequencyMapView::setSelection(std::size_t first, std::size_t count) {
        stdc_impl_t;
        const auto frames = impl.frames();
        const std::size_t size = frames ? frames->size() : 0;
        if (first >= size || count == 0) {
            impl.setSelection(std::nullopt);
            return;
        }
        impl.setSelection(std::pair(first, std::min(count, size - first)));
    }

    void FrequencyMapView::selectAll() {
        const auto frames = buffer() ? buffer()->map().frames.size() : 0;
        setSelection(0, frames);
    }

    void FrequencyMapView::clearSelection() {
        stdc_impl_t;
        impl.setSelection(std::nullopt);
    }

    std::optional<std::size_t> FrequencyMapView::frameAt(double x) const {
        stdc_impl_t;
        const auto frames = impl.frames();
        if (!frames || frames->empty()) {
            return std::nullopt;
        }
        const double time = timeAxis().toTick(x);
        const auto next =
            std::lower_bound(frames->begin(), frames->end(), time,
                             [](const auto &frame, double value) { return frame.time < value; });
        if (next == frames->begin()) {
            return 0;
        }
        if (next == frames->end()) {
            return frames->size() - 1;
        }
        const auto previous = std::prev(next);
        const auto nearer = time - previous->time <= next->time - time ? previous : next;
        return std::size_t(nearer - frames->begin());
    }

    double FrequencyMapView::frequencyAt(double y) const {
        // The row of a note is centered on the note.
        return frequencyOf(keyAxis().toKey(y) - 0.5);
    }

    double FrequencyMapView::yOf(double frequency) const {
        return keyAxis().toY(noteOf(frequency) + 0.5);
    }

    double FrequencyMapView::xOf(double time) const {
        return timeAxis().toX(time);
    }

    QColor FrequencyMapView::frequencyColor() const {
        stdc_impl_t;
        return impl.frequencyColor.isValid() ? impl.frequencyColor : QColor(0xe0, 0x30, 0xa0);
    }

    void FrequencyMapView::setFrequencyColor(const QColor &color) {
        stdc_impl_t;
        impl.frequencyColor = color;
        viewport()->update();
    }

    QColor FrequencyMapView::savedFrequencyColor() const {
        stdc_impl_t;
        return impl.savedFrequencyColor.isValid() ? impl.savedFrequencyColor
                                                  : QColor(0x80, 0x80, 0x80);
    }

    void FrequencyMapView::setSavedFrequencyColor(const QColor &color) {
        stdc_impl_t;
        impl.savedFrequencyColor = color;
        viewport()->update();
    }

    QColor FrequencyMapView::spectrumColor() const {
        stdc_impl_t;
        return impl.spectrumColor.isValid() ? impl.spectrumColor : QColor(0x20, 0x60, 0xd0);
    }

    void FrequencyMapView::setSpectrumColor(const QColor &color) {
        stdc_impl_t;
        impl.spectrumColor = color;
        viewport()->update();
    }

    QColor FrequencyMapView::selectionColor() const {
        stdc_impl_t;
        return impl.selectionColor.isValid() ? impl.selectionColor : QColor(0x30, 0x80, 0xe0, 0x40);
    }

    void FrequencyMapView::setSelectionColor(const QColor &color) {
        stdc_impl_t;
        impl.selectionColor = color;
        viewport()->update();
    }

    void FrequencyMapView::resizeEvent(QResizeEvent *event) {
        stdc_impl_t;
        SceneView::resizeEvent(event);
        if (impl.fitPending) {
            impl.fit();
        }
    }

}
