#include "FrequencyMapView.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <QtCore/QSignalBlocker>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtWidgets/QMenu>
#include <QtWidgets/QSlider>
#include <QtWidgets/QToolButton>

#include <stdcorelib/pimpl.h>

#include <hellokit/AudioFile/Spectrogram.h>
#include <hellokit/VoiceBank/FrequencyMapRevisionBuffer.h>

#include <helloutau/Widgets/SceneGestures.h>
#include <helloutau/Widgets/SceneLayer.h>

#include "FrequencyViewKeyboardModifiers.h"

namespace hello::daw {

    namespace {

        using Buffer = kit::FrequencyMapRevisionBuffer;
        using Role = FrequencyViewKeyboardModifiers::Role;

        double noteOf(double frequency) {
            return 69 + 12 * std::log2(frequency / 440);
        }

        double frequencyOf(double note) {
            return 440 * std::exp2((note - 69) / 12);
        }

        bool voiced(double frequency) {
            return frequency > Buffer::lowestFrequency && std::isfinite(frequency);
        }

        QColor blend(const QColor &from, const QColor &to, double amount) {
            const auto mix = [amount](double a, double b) { return a + (b - a) * amount; };
            return QColor::fromRgbF(float(mix(from.redF(), to.redF())),
                                    float(mix(from.greenF(), to.greenF())),
                                    float(mix(from.blueF(), to.blueF())));
        }

        // The frames between two lines of the frame grid, as in frqeditor
        constexpr int framesPerLine = 17;

        // The milliseconds of a frame without frames to measure, 256 samples at 44100 Hz
        constexpr double defaultFrameTime = 256 * 1000.0 / 44100;

        // Returns the level of a cell of the spectrogram from 0 to 255 for the magnitude of
        // kit::Spectrogram, as frqeditor computes it from its own magnitude. frqeditor scales the
        // windowed samples by 1 / 0.46 / 2048, and kit::Spectrogram by 2 / (0.54 * 2048), the
        // inverse of half the sum of the Hamming window.
        int levelOf(double magnitude, int brightness) {
            constexpr double scale = 0.54 / 0.92;
            // The offsets of the brightness -3 to 3
            constexpr int offsets[] = {150, 160, 185, 200, 210, 225, 230};
            const int level = int(std::pow(magnitude * scale, 0.4) * 512 + 128);
            const int index = std::clamp(brightness, FrequencyMapView::lowestBrightness,
                                         FrequencyMapView::highestBrightness) -
                              FrequencyMapView::lowestBrightness;
            return std::clamp((level - offsets[index]) * 5, 0, 255);
        }

        // Returns the color of a level in a scheme, as in frqeditor. Each channel rises over a
        // third of the levels, and is halved and raised by 128, a blend with white.
        QColor colorOf(int level, int scheme) {
            const int rising = level * 3;
            const int middle = (level - 85) * 3;
            const int late = (level - 170) * 3;
            const int channels[FrequencyMapView::colorSchemeCount][3] = {
                {late,   middle, rising},
                {middle, late,   rising},
                {late,   rising, middle},
                {middle, rising, late  },
                {rising, middle, late  },
                {rising, late,   middle},
            };
            const auto &c = channels[std::clamp(scheme, 0, FrequencyMapView::colorSchemeCount - 1)];
            const auto channel = [](int value) { return 128 + std::clamp(value, 0, 255) / 2; };
            return QColor(channel(c[0]), channel(c[1]), channel(c[2]));
        }

        // The parts of a hit of the curve layer
        enum Part {
            PlotPart,
            LabelPart,
            KeyLinePart,
        };

    }

    class FrequencyMapView::Impl {
    public:
        using Decl = FrequencyMapView;

        explicit Impl(Decl *decl) : _decl(decl) {
        }

        Decl *_decl;

        Buffer *buffer = nullptr;
        std::shared_ptr<const kit::Spectrogram> spectrogram;
        QString title;
        ViewMode viewMode = KeyRowsView;
        int brightness = 0;
        int colorScheme = 0;
        KeyboardModifierBindings bindings{FrequencyViewKeyboardModifiers::scheme()};
        std::optional<std::pair<std::size_t, std::size_t>> selection;
        // The key while the key line is dragged
        std::optional<double> keyPreview;
        // Set once the view has a size to place level 0 in
        bool placed = false;
        // The note at the bottom of the view, which stays in place when the view is resized, as
        // the bottom note of frqeditor does
        double bottom = lowestNote;
        // Set while a resize moves the top of the key axis
        bool keepingBottom = false;
        // Set while the range of keys follows the key axis
        bool updatingKeys = false;
        // Set while the range of ticks follows the time axis
        bool updatingRange = false;
        // The milliseconds of a frame that the time axis was set for
        double frameTime = defaultFrameTime;
        QColor whiteRowColor, blackRowColor, rowLineColor, labelColor, frameLineColor, keyLineColor,
            curveColor, selectedCurveColor, savedColor, amplitudeColor;

        QToolButton *viewButton = nullptr;
        QToolButton *brightnessButton = nullptr;
        QSlider *brightnessSlider = nullptr;
        QMenu *colorMenu = nullptr;

        // The spectrogram drawn for the state it was drawn for
        struct SpectrumImage {
            QImage image;
            TimeAxis time;
            KeyAxis keys;
            QSize size;
            const kit::Spectrogram *spectrogram = nullptr;
            const Buffer *buffer = nullptr;
            int brightness = 0;
            int scheme = 0;
        } spectrumImage;

        const std::vector<kit::FrequencyMap::Frame> *frames() const {
            return buffer ? &buffer->map().frames : nullptr;
        }

        // Returns the milliseconds between two frames of the map.
        double measuredFrameTime() const {
            const auto list = frames();
            if (!list || list->size() < 2) {
                return defaultFrameTime;
            }
            return std::max(1e-3, (*list)[1].time - (*list)[0].time);
        }

        // Returns the time in milliseconds after the last frame, or the end of the spectrogram
        // without frames.
        double endTime() const {
            const auto list = frames();
            if (list && !list->empty()) {
                return list->back().time + measuredFrameTime();
            }
            if (spectrogram && spectrogram->frameCount() > 0) {
                return spectrogram->timeOf(spectrogram->frameCount() - 1);
            }
            return 1000;
        }

        // Keeps the scene beginning at the right edge of the labels, which take a fixed width in
        // pixels, so that the first frame is not hidden behind the labels.
        void updateRange() {
            stdc_decl_t;
            if (updatingRange) {
                return;
            }
            updatingRange = true;
            decl.setTickRange(-decl.labelWidth() / decl.timeAxis().pixelsPerTick, endTime());
            updatingRange = false;
        }

        // Sets both axes to the pixels of level, with time at the right edge of the labels and
        // the note bottom at the bottom of the view.
        void place(int level, double time, double bottom) {
            stdc_decl_t;
            level = std::clamp(level, 0, zoomLevelCount - 1);
            frameTime = measuredFrameTime();
            TimeAxis timeAxis;
            timeAxis.pixelsPerTick = framePixels[level] / frameTime;
            timeAxis.left = time - decl.labelWidth() / timeAxis.pixelsPerTick;
            KeyAxis keyAxis;
            keyAxis.pixelsPerKey = semitonePixels[level];
            keyAxis.top = bottom + decl.viewport()->height() / keyAxis.pixelsPerKey;
            // The range follows the new scale before the left edge is clamped to it.
            decl.setTimeAxis(timeAxis);
            updateRange();
            decl.setTimeAxis(timeAxis);
            updateKeyRange(keyAxis.pixelsPerKey);
            decl.setKeyAxis(keyAxis);
        }

        // Lets the bottom note scroll from C1 to B8, as in frqeditor, with the notes above it in
        // view whatever their number.
        void updateKeyRange(double pixelsPerKey) {
            stdc_decl_t;
            decl.setKeyRange(lowestNote,
                             highestBottomNote +
                                 int(std::ceil(decl.viewport()->height() / pixelsPerKey)));
        }

        // The highest note at the bottom of the view, B8 as in frqeditor
        static constexpr int highestBottomNote = 119;

        double firstVisibleTime() const {
            stdc_decl_t;
            return decl.timeAxis().toTick(decl.labelWidth());
        }

        double bottomNote() const {
            stdc_decl_t;
            return decl.keyAxis().toKey(decl.viewport()->height());
        }

        bool keyEditable() const {
            return buffer && buffer->map().averageFrequency.has_value();
        }

        double shownKey() const {
            return keyPreview ? *keyPreview : (buffer ? buffer->key() : 0);
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

        bool isSelected(std::size_t frame) const {
            return selection && frame >= selection->first &&
                   frame < selection->first + selection->second;
        }

        static std::pair<std::size_t, std::size_t> between(std::size_t a, std::size_t b) {
            return {std::min(a, b), std::max(a, b) - std::min(a, b) + 1};
        }

        // Returns the left edges of the columns of the spectrogram in milliseconds: the frames
        // of the map, or the frames of the spectrogram without a map.
        std::vector<double> columnTimes() const {
            std::vector<double> times;
            if (const auto list = frames(); list && !list->empty()) {
                for (const auto &frame : *list) {
                    times.push_back(frame.time);
                }
            } else if (spectrogram) {
                for (int frame = 0; frame < spectrogram->frameCount(); ++frame) {
                    times.push_back(spectrogram->timeOf(frame));
                }
            }
            return times;
        }

        // Returns the average magnitude of frame over the fractional bins from low to high, the
        // ends weighted by their parts of a bin, as frqeditor averages the bins of a note.
        double averageMagnitude(int frame, double low, double high) const {
            const auto &data = *spectrogram;
            const int last = kit::Spectrogram::binCount - 1;
            low = std::clamp(low, 0.0, double(last));
            high = std::clamp(high, low, double(last));
            const auto at = [&](double bin) {
                const int index = int(bin);
                const double fraction = bin - index;
                const double a = data.magnitude(frame, index);
                return fraction == 0 || index >= last
                           ? a
                           : a + (data.magnitude(frame, index + 1) - a) * fraction;
            };
            if (high == low) {
                return at(low);
            }
            const int first = int(low);
            const int end = int(high);
            double sum = (first + 1 - low) * at(low) + (high - end) * at(high);
            for (int bin = first + 1; bin <= end; ++bin) {
                sum += data.magnitude(frame, bin);
            }
            return sum / (high - low);
        }

        const QImage &spectrumImageOf(const QSize &size) {
            stdc_decl_t;
            auto &cached = spectrumImage;
            const auto &time = decl.timeAxis();
            const auto &keys = decl.keyAxis();
            if (cached.spectrogram == spectrogram.get() && cached.buffer == buffer &&
                cached.time == time && cached.keys == keys && cached.size == size &&
                cached.brightness == brightness && cached.scheme == colorScheme &&
                !cached.image.isNull()) {
                return cached.image;
            }
            cached = {QImage(size, QImage::Format_RGB32),
                      time,
                      keys,
                      size,
                      spectrogram.get(),
                      buffer,
                      brightness,
                      colorScheme};
            cached.image.fill(colorOf(0, colorScheme));
            const auto &data = *spectrogram;
            if (data.frameCount() <= 0) {
                return cached.image;
            }
            QColor colors[256];
            for (int level = 0; level < 256; ++level) {
                colors[level] = colorOf(level, colorScheme);
            }
            const int lowest = int(std::floor(keys.toKey(size.height())));
            const int highest = int(std::ceil(keys.toKey(0)));
            // The bins of the semitone of each note, in the row of the note
            std::vector<std::pair<double, double>> bins;
            for (int note = lowest; note <= highest; ++note) {
                bins.push_back(
                    {data.binOf(frequencyOf(note - 0.5)), data.binOf(frequencyOf(note + 0.5))});
            }
            const auto times = columnTimes();
            const double width = measuredFrameTime();
            QPainter painter(&cached.image);
            for (std::size_t i = 0; i < times.size(); ++i) {
                const double left = time.toX(times[i]);
                const double right =
                    time.toX(i + 1 < times.size() ? times[i + 1] : times[i] + width);
                if (right < 0 || left > size.width()) {
                    continue;
                }
                const int frame = std::clamp(int(std::lround(times[i] * data.sampleRate() / 1000 /
                                                             kit::Spectrogram::hopSize)),
                                             0, data.frameCount() - 1);
                for (int note = lowest; note <= highest; ++note) {
                    const auto [low, high] = bins[std::size_t(note - lowest)];
                    const int level = levelOf(averageMagnitude(frame, low, high), brightness);
                    painter.fillRect(
                        QRectF(QPointF(left, keys.toY(note + 1)), QPointF(right, keys.toY(note))),
                        colors[level]);
                }
            }
            return cached.image;
        }

        void syncControls() {
            const QSignalBlocker blocker(viewButton);
            viewButton->setChecked(viewMode == SpectrumView);
            brightnessButton->setVisible(viewMode == SpectrumView);
            if (viewMode != SpectrumView) {
                brightnessSlider->hide();
            }
            const QSignalBlocker sliderBlocker(brightnessSlider);
            brightnessSlider->setValue(brightness);
        }

        void createControls() {
            stdc_decl_t;
            // The button 2003, the button 2004 and the trackbar 2005 of frqeditor over the top
            // left corner
            viewButton = new QToolButton(decl.viewport());
            viewButton->setText(QStringLiteral("s"));
            viewButton->setCheckable(true);
            viewButton->setToolTip(tr("Show Spectrogram"));
            viewButton->setContextMenuPolicy(Qt::CustomContextMenu);
            viewButton->setGeometry(18, 18, 20, 20);
            QObject::connect(viewButton, &QToolButton::toggled, &decl, [this](bool checked) {
                stdc_decl_t;
                decl.setViewMode(checked ? SpectrumView : KeyRowsView);
            });

            colorMenu = new QMenu(&decl);
            const char *names[colorSchemeCount] = {
                QT_TR_NOOP("&Cool"),   QT_TR_NOOP("&Purple"), QT_TR_NOOP("&Jade"),
                QT_TR_NOOP("&Grassy"), QT_TR_NOOP("&Flame"),  QT_TR_NOOP("&Ruby"),
            };
            for (int i = 0; i < colorSchemeCount; ++i) {
                auto action = colorMenu->addAction(tr(names[i]));
                action->setCheckable(true);
                QObject::connect(action, &QAction::triggered, &decl, [this, i] {
                    stdc_decl_t;
                    decl.setColorScheme(i);
                });
            }
            QObject::connect(viewButton, &QWidget::customContextMenuRequested, &decl, [this] {
                const auto list = colorMenu->actions();
                for (int i = 0; i < list.size(); ++i) {
                    list[i]->setChecked(i == colorScheme);
                }
                colorMenu->popup(QCursor::pos());
            });

            brightnessButton = new QToolButton(decl.viewport());
            brightnessButton->setText(QStringLiteral("*"));
            brightnessButton->setToolTip(tr("Brightness"));
            brightnessButton->setGeometry(41, 22, 16, 16);
            brightnessButton->hide();

            brightnessSlider = new QSlider(Qt::Vertical, decl.viewport());
            brightnessSlider->setRange(lowestBrightness, highestBrightness);
            brightnessSlider->setPageStep(1);
            brightnessSlider->setTickPosition(QSlider::TicksBothSides);
            // The brightest at the top, as on the trackbar of frqeditor
            brightnessSlider->setInvertedAppearance(true);
            // An opaque background, so that the ticks are not drawn over the spectrogram
            brightnessSlider->setAutoFillBackground(true);
            brightnessSlider->setGeometry(41, 38, 24, 70);
            brightnessSlider->hide();
            QObject::connect(brightnessButton, &QToolButton::clicked, &decl, [this] {
                brightnessSlider->setVisible(!brightnessSlider->isVisible());
                if (brightnessSlider->isVisible()) {
                    brightnessSlider->setFocus();
                }
            });
            QObject::connect(brightnessSlider, &QSlider::valueChanged, &decl, [this](int value) {
                stdc_decl_t;
                decl.setBrightness(value);
            });
            syncControls();
        }

        class StrokeGesture;
        class SelectGesture;
        class KeyGesture;
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

    // A drag of the key line, shown while it lasts and set as one step on the release
    class FrequencyMapView::Impl::KeyGesture : public SceneGesture {
    public:
        explicit KeyGesture(Impl *impl) : m_impl(impl) {
        }

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override {
            Q_UNUSED(modifiers);
            m_impl->keyPreview =
                std::max(Buffer::lowestFrequency, m_impl->_decl->frequencyAt(position.y()));
            m_impl->_decl->viewport()->update();
        }

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override {
            move(position, modifiers);
            const double key = *m_impl->keyPreview;
            m_impl->keyPreview.reset();
            if (m_impl->buffer->setKey(key)) {
                Q_EMIT m_impl->_decl->edited();
            }
            m_impl->_decl->viewport()->update();
        }

        void cancel() override {
            m_impl->keyPreview.reset();
            m_impl->_decl->viewport()->update();
        }

    private:
        Impl *m_impl;
    };

    // The frames of the map, the selected ones in their own color, and the title
    class FrequencyMapView::Impl::CurveLayer : public SceneLayer {
    public:
        explicit CurveLayer(Impl *impl) : m_impl(impl) {
        }

        // Draws a square at each voiced frame and joins consecutive voiced frames, as frqeditor
        // draws a curve, the selected frames in selected if impl is given.
        static void drawCurve(QPainter &painter, const FrequencyMapView &view,
                              const std::vector<kit::FrequencyMap::Frame> &frames,
                              const QColor &color, const QColor &selected, const Impl *impl) {
            if (frames.empty()) {
                return;
            }
            const double frameWidth = frames.size() > 1
                                          ? view.xOf(frames[1].time) - view.xOf(frames[0].time)
                                          : view.timeAxis().pixelsPerTick * defaultFrameTime;
            const int size = std::max(2, int(frameWidth * 0.8));
            const auto chosen = [&](std::size_t i) {
                return impl && impl->isSelected(i) ? selected : color;
            };
            const double left = -size;
            const double right = view.viewport()->width() + size;
            for (std::size_t i = 0; i < frames.size(); ++i) {
                const double x = view.xOf(frames[i].time);
                if (x < left || x > right || !voiced(frames[i].frequency)) {
                    continue;
                }
                const double y = view.yOf(frames[i].frequency);
                painter.fillRect(
                    QRectF(std::floor(x), std::floor(y) - double(size / 2), size, size), chosen(i));
            }
            for (std::size_t i = 1; i < frames.size(); ++i) {
                const double x = view.xOf(frames[i].time);
                if (x < left || view.xOf(frames[i - 1].time) > right ||
                    !voiced(frames[i].frequency) || !voiced(frames[i - 1].frequency)) {
                    continue;
                }
                painter.setPen(impl && (impl->isSelected(i) || impl->isSelected(i - 1)) ? selected
                                                                                        : color);
                painter.drawLine(
                    QPointF(std::floor(view.xOf(frames[i - 1].time)) + double(size / 2),
                            view.yOf(frames[i - 1].frequency)),
                    QPointF(std::floor(x) + double(size / 2), view.yOf(frames[i].frequency)));
            }
        }

        void paint(QPainter &painter, const QRect &exposed) override {
            Q_UNUSED(exposed);
            const auto impl = m_impl;
            const auto view = impl->_decl;
            const QRect area = view->viewport()->rect();
            if (impl->buffer) {
                painter.save();
                painter.setClipRect(
                    QRect(view->labelWidth(), 0, area.width() - view->labelWidth(), area.height()));
                drawCurve(painter, *view, impl->buffer->map().frames, view->curveColor(),
                          view->selectedCurveColor(), impl);
                painter.restore();
            }
            if (!impl->title.isEmpty()) {
                const auto rect = area.adjusted(1, 1, -1, -1);
                painter.setPen(view->labelColor());
                painter.setBackgroundMode(Qt::OpaqueMode);
                painter.setBackground(view->whiteRowColor());
                painter.drawText(
                    rect, Qt::AlignLeft | Qt::AlignTop | Qt::TextSingleLine,
                    view->fontMetrics().elidedText(impl->title, Qt::ElideRight, rect.width()));
                painter.setBackgroundMode(Qt::TransparentMode);
            }
        }

        std::optional<SceneHit> hitTest(QPointF position) const override {
            const auto impl = m_impl;
            if (!impl->buffer || impl->buffer->map().frames.empty()) {
                return std::nullopt;
            }
            SceneHit hit;
            if (position.x() < impl->_decl->labelWidth()) {
                const bool onKey = impl->keyEditable() && voiced(impl->shownKey()) &&
                                   std::abs(position.y() - impl->_decl->yOf(impl->shownKey())) <= 3;
                hit.part = onKey ? KeyLinePart : LabelPart;
                hit.cursor = onKey ? Qt::SizeVerCursor : Qt::ArrowCursor;
                return hit;
            }
            hit.part = PlotPart;
            hit.cursor = Qt::CrossCursor;
            return hit;
        }

        std::unique_ptr<SceneGesture> press(const SceneHit &hit, QPointF position,
                                            Qt::MouseButton button,
                                            Qt::KeyboardModifiers modifiers) override {
            Q_UNUSED(modifiers);
            const auto impl = m_impl;
            if (hit.part == LabelPart) {
                return nullptr;
            }
            if (hit.part == KeyLinePart) {
                if (button != Qt::LeftButton) {
                    return nullptr;
                }
                return std::make_unique<PressGesture>(
                    position,
                    [impl](Qt::KeyboardModifiers held) -> std::unique_ptr<SceneGesture> {
                        const auto activation = impl->bindings.activate(
                            FrequencyViewKeyboardModifiers::KeyLineScene, held);
                        if (!activation || activation->operation != Role::DragKey) {
                            return nullptr;
                        }
                        return std::make_unique<KeyGesture>(impl);
                    },
                    nullptr);
            }
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

        bool doubleClick(const SceneHit &hit, QPointF position) override {
            if (hit.part == PlotPart) {
                return false;
            }
            // A double click in the labels flattens the frames and the key, as in frqeditor.
            if (m_impl->buffer->flatten(m_impl->_decl->frequencyAt(position.y()))) {
                m_impl->_decl->viewport()->update();
                Q_EMIT m_impl->_decl->edited();
            }
            return true;
        }

    private:
        Impl *m_impl;
    };

    // The rows of the keys or the spectrogram, the labels, the frame grid, the key lines, the
    // amplitudes and the frames as last saved
    class FrequencyMapView::Impl::BackgroundLayer : public SceneLayer {
    public:
        explicit BackgroundLayer(Impl *impl) : m_impl(impl) {
        }

        void paint(QPainter &painter, const QRect &exposed) override {
            Q_UNUSED(exposed);
            const auto impl = m_impl;
            const auto view = impl->_decl;
            const QRect area = view->viewport()->rect();
            const int labels = view->labelWidth();
            const QRect plot(labels, 0, std::max(0, area.width() - labels), area.height());
            const auto &keys = view->keyAxis();
            const int lowest = int(std::floor(keys.toKey(area.height())));
            const int highest = int(std::ceil(keys.toKey(0)));
            painter.fillRect(area, view->whiteRowColor());

            painter.save();
            painter.setClipRect(plot);
            if (impl->viewMode == KeyRowsView) {
                painter.setPen(view->rowLineColor());
                for (int note = lowest; note <= highest; ++note) {
                    const QRectF row(QPointF(plot.left(), keys.toY(note + 1)),
                                     QPointF(plot.right() + 1, keys.toY(note)));
                    const int pitch = ((note % 12) + 12) % 12;
                    const bool black =
                        pitch == 1 || pitch == 3 || pitch == 6 || pitch == 8 || pitch == 10;
                    painter.fillRect(row, black ? view->blackRowColor() : view->whiteRowColor());
                    const double y = std::floor(row.bottom()) - 0.5;
                    painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right() + 1, y));
                }
            } else if (impl->spectrogram) {
                painter.drawImage(QPointF(0, 0), impl->spectrumImageOf(area.size()));
            } else {
                painter.fillRect(plot, colorOf(0, impl->colorScheme));
            }
            painter.restore();

            // A label at each C from C1 to C8, vertically centered on its row
            painter.setPen(view->labelColor());
            for (int note = std::max(lowestNote, (lowest + 11) / 12 * 12); note <= highest;
                 note += 12) {
                if (note >= 120) {
                    break;
                }
                const QRectF row(QPointF(0, keys.toY(note + 1)), QPointF(labels, keys.toY(note)));
                painter.drawText(row.adjusted(1, -20, 0, 20), Qt::AlignLeft | Qt::AlignVCenter,
                                 QStringLiteral("C%1").arg(note / 12 - 1));
            }

            const auto frames = impl->frames();
            painter.save();
            painter.setClipRect(plot);
            painter.setPen(view->frameLineColor());
            if (frames) {
                for (std::size_t i = 0; i < frames->size(); i += framesPerLine) {
                    const double x = std::floor(view->xOf((*frames)[i].time)) + 0.5;
                    if (x > plot.right() + 1) {
                        break;
                    }
                    painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom() + 1));
                }
            }
            painter.restore();
            if (!impl->buffer) {
                return;
            }

            // The key lines across the labels as well, the saved one first
            const auto keyLine = [&](double key, const QColor &color) {
                if (!voiced(key)) {
                    return;
                }
                const double y = std::floor(view->yOf(key)) + 0.5;
                painter.setPen(color);
                painter.drawLine(QPointF(area.left(), y), QPointF(area.right() + 1, y));
            };
            const auto &saved = impl->buffer->savedMap();
            keyLine(saved.averageFrequency ? *saved.averageFrequency : saved.pitchAverage(),
                    view->savedColor());
            keyLine(impl->shownKey(), view->keyLineColor());

            painter.save();
            painter.setClipRect(plot);
            if (impl->viewMode == KeyRowsView) {
                const double frameWidth =
                    view->timeAxis().pixelsPerTick * impl->measuredFrameTime();
                const double py = keys.pixelsPerKey;
                for (const auto &frame : *frames) {
                    if (!frame.amplitude) {
                        continue;
                    }
                    const double x = view->xOf(frame.time);
                    const double height = *frame.amplitude * py / 1500;
                    painter.fillRect(QRectF(x, area.height() - height, frameWidth * 0.8, height),
                                     view->amplitudeColor());
                }
            }
            CurveLayer::drawCurve(painter, *view, saved.frames, view->savedColor(),
                                  view->savedColor(), nullptr);
            painter.restore();
        }

    private:
        Impl *m_impl;
    };

    FrequencyMapView::FrequencyMapView(QWidget *parent)
        : SceneView(parent), _impl(std::make_unique<Impl>(this)) {
        stdc_impl_t;
        setKeyRange(lowestNote, 127);
        setTimeScaleRange(0.01, 200);
        setKeyScaleRange(1, 200);
        addLayer(std::make_unique<Impl::BackgroundLayer>(&impl));
        addLayer(std::make_unique<Impl::CurveLayer>(&impl));
        impl.createControls();
        connect(this, &SceneView::timeAxisChanged, this, [this] {
            stdc_impl_t;
            impl.updateRange();
        });
        connect(this, &SceneView::keyAxisChanged, this, [this] {
            stdc_impl_t;
            if (!impl.keepingBottom) {
                impl.bottom = impl.bottomNote();
            }
            // The range follows a zoom of the wheel as well.
            if (!impl.updatingKeys) {
                impl.updatingKeys = true;
                impl.updateKeyRange(keyAxis().pixelsPerKey);
                impl.updatingKeys = false;
            }
        });
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
        // The pixels of a frame stay, for the time of a frame of the new map.
        const double pixelsPerFrame = timeAxis().pixelsPerTick * impl.frameTime;
        impl.buffer = buffer;
        impl.keyPreview.reset();
        impl.setSelection(std::nullopt);
        impl.frameTime = impl.measuredFrameTime();
        if (impl.placed) {
            TimeAxis axis;
            axis.pixelsPerTick = pixelsPerFrame / impl.frameTime;
            axis.left = -labelWidth() / axis.pixelsPerTick;
            setTimeAxis(axis);
            impl.updateRange();
            setTimeAxis(axis);
        }
        viewport()->update();
    }

    std::shared_ptr<const kit::Spectrogram> FrequencyMapView::spectrogram() const {
        stdc_impl_t;
        return impl.spectrogram;
    }

    void FrequencyMapView::setSpectrogram(std::shared_ptr<const kit::Spectrogram> spectrogram) {
        stdc_impl_t;
        impl.spectrogram = std::move(spectrogram);
        impl.updateRange();
        viewport()->update();
    }

    QString FrequencyMapView::title() const {
        stdc_impl_t;
        return impl.title;
    }

    void FrequencyMapView::setTitle(const QString &title) {
        stdc_impl_t;
        impl.title = title;
        viewport()->update();
    }

    FrequencyMapView::ViewMode FrequencyMapView::viewMode() const {
        stdc_impl_t;
        return impl.viewMode;
    }

    void FrequencyMapView::setViewMode(ViewMode mode) {
        stdc_impl_t;
        if (mode == impl.viewMode) {
            return;
        }
        impl.viewMode = mode;
        impl.syncControls();
        viewport()->update();
        Q_EMIT viewModeChanged();
    }

    int FrequencyMapView::brightness() const {
        stdc_impl_t;
        return impl.brightness;
    }

    void FrequencyMapView::setBrightness(int brightness) {
        stdc_impl_t;
        brightness = std::clamp(brightness, lowestBrightness, highestBrightness);
        if (brightness == impl.brightness) {
            return;
        }
        impl.brightness = brightness;
        impl.syncControls();
        viewport()->update();
        Q_EMIT spectrumChanged();
    }

    int FrequencyMapView::colorScheme() const {
        stdc_impl_t;
        return impl.colorScheme;
    }

    void FrequencyMapView::setColorScheme(int scheme) {
        stdc_impl_t;
        scheme = std::clamp(scheme, 0, colorSchemeCount - 1);
        if (scheme == impl.colorScheme) {
            return;
        }
        impl.colorScheme = scheme;
        viewport()->update();
        Q_EMIT spectrumChanged();
    }

    int FrequencyMapView::zoomLevel() const {
        stdc_impl_t;
        const double pixels = timeAxis().pixelsPerTick * impl.measuredFrameTime();
        int nearest = 0;
        for (int level = 1; level < zoomLevelCount; ++level) {
            if (std::abs(framePixels[level] - pixels) < std::abs(framePixels[nearest] - pixels)) {
                nearest = level;
            }
        }
        return nearest;
    }

    void FrequencyMapView::setZoomLevel(int level) {
        stdc_impl_t;
        impl.place(level, impl.firstVisibleTime(), impl.bottomNote());
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
        // The frame whose column contains the time, as frqeditor finds the frame of a pixel
        const double time = timeAxis().toTick(x);
        const auto next =
            std::upper_bound(frames->begin(), frames->end(), time,
                             [](double value, const auto &frame) { return value < frame.time; });
        if (next == frames->begin()) {
            return 0;
        }
        return std::size_t(std::prev(next) - frames->begin());
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

    int FrequencyMapView::labelWidth() const {
        return fontMetrics().horizontalAdvance(QStringLiteral("C0"));
    }

    QToolButton *FrequencyMapView::viewButton() const {
        stdc_impl_t;
        return impl.viewButton;
    }

    QToolButton *FrequencyMapView::brightnessButton() const {
        stdc_impl_t;
        return impl.brightnessButton;
    }

    QSlider *FrequencyMapView::brightnessSlider() const {
        stdc_impl_t;
        return impl.brightnessSlider;
    }

// The getter and the setter of a stylable color, with the default given
#define HELLO_COLOR_PROPERTY(NAME, SETTER, DEFAULT)                                                \
    QColor FrequencyMapView::NAME() const {                                                        \
        stdc_impl_t;                                                                               \
        return impl.NAME.isValid() ? impl.NAME : (DEFAULT);                                        \
    }                                                                                              \
                                                                                                   \
    void FrequencyMapView::SETTER(const QColor &color) {                                           \
        stdc_impl_t;                                                                               \
        impl.NAME = color;                                                                         \
        viewport()->update();                                                                      \
    }

    // The rows and the labels follow the palette, which in a light theme gives the white and
    // the grays of frqeditor. The curves and the lines have the colors of frqeditor.
    HELLO_COLOR_PROPERTY(whiteRowColor, setWhiteRowColor, palette().color(QPalette::Base))
    HELLO_COLOR_PROPERTY(blackRowColor, setBlackRowColor,
                         blend(palette().color(QPalette::Base), palette().color(QPalette::Text),
                               0.25))
    HELLO_COLOR_PROPERTY(rowLineColor, setRowLineColor,
                         blend(palette().color(QPalette::Base), palette().color(QPalette::Text),
                               0.12))
    HELLO_COLOR_PROPERTY(labelColor, setLabelColor,
                         blend(palette().color(QPalette::Base), palette().color(QPalette::Text),
                               0.5))
    HELLO_COLOR_PROPERTY(frameLineColor, setFrameLineColor, QColor(0, 255, 255))
    HELLO_COLOR_PROPERTY(keyLineColor, setKeyLineColor, QColor(255, 0, 0))
    HELLO_COLOR_PROPERTY(curveColor, setCurveColor, QColor(0, 0, 255))
    HELLO_COLOR_PROPERTY(selectedCurveColor, setSelectedCurveColor, QColor(255, 0, 0))
    HELLO_COLOR_PROPERTY(savedColor, setSavedColor, QColor(185, 122, 87))
    HELLO_COLOR_PROPERTY(amplitudeColor, setAmplitudeColor, QColor(0, 255, 0))

#undef HELLO_COLOR_PROPERTY

    void FrequencyMapView::resizeEvent(QResizeEvent *event) {
        stdc_impl_t;
        impl.keepingBottom = true;
        SceneView::resizeEvent(event);
        if (impl.placed) {
            KeyAxis axis = keyAxis();
            axis.top = impl.bottom + viewport()->height() / axis.pixelsPerKey;
            impl.updateKeyRange(axis.pixelsPerKey);
            setKeyAxis(axis);
        }
        impl.keepingBottom = false;
        // Level 0 with C1 at the bottom, as frqeditor starts, once the view has a size
        if (!impl.placed && viewport()->height() > 0 && viewport()->width() > 0) {
            impl.placed = true;
            impl.place(0, 0, lowestNote);
        }
    }

}
