#include "FrequencyMapView.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <QtCore/QSignalBlocker>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
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
        Tool tool = PenTool;
        KeyboardModifierBindings bindings{FrequencyViewKeyboardModifiers::scheme()};
        std::optional<std::pair<std::size_t, std::size_t>> selection;
        // The corrections of the frames of the buffer, empty for none
        std::vector<kit::FrequencyMapCheck::Correction> corrections;
        // The press and the pointer in milliseconds while the select tool drags
        std::optional<std::pair<double, double>> band;
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
            curveColor, selectedCurveColor, savedColor, amplitudeColor, bandColor, correctionColor,
            selectedCorrectionColor, correctionBackgroundColor;

        // The bar of the title and the ruler of milliseconds below it in the top margin of the
        // viewport
        QWidget *titleBar = nullptr;
        QWidget *ruler = nullptr;
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

        // Lets the bottom note scroll from C0 to B8, with the notes above it in
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
            // left corner, placed by placeWidgets()
            viewButton = new QToolButton(decl.viewport());
            viewButton->setText(QStringLiteral("s"));
            viewButton->setCheckable(true);
            viewButton->setToolTip(tr("Show Spectrogram"));
            viewButton->setContextMenuPolicy(Qt::CustomContextMenu);
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
            brightnessButton->hide();

            brightnessSlider = new QSlider(Qt::Vertical, decl.viewport());
            brightnessSlider->setRange(lowestBrightness, highestBrightness);
            brightnessSlider->setPageStep(1);
            brightnessSlider->setTickPosition(QSlider::TicksBothSides);
            // The brightest at the top, as on the trackbar of frqeditor
            brightnessSlider->setInvertedAppearance(true);
            // An opaque background, so that the ticks are not drawn over the spectrogram
            brightnessSlider->setAutoFillBackground(true);
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

        // Returns the height of the title bar and of the time ruler, a line of text with a margin.
        int rowHeight() const {
            return _decl->fontMetrics().height() + 6;
        }

        // Places the title bar and the time ruler in the top margin of the viewport, the ruler
        // right above the viewport, and the controls of the view right of the labels with the
        // offsets of frqeditor.
        void placeWidgets() {
            const QRect area = _decl->viewport()->geometry();
            const int height = rowHeight();
            titleBar->setGeometry(area.left(), area.top() - 2 * height, area.width(), height);
            ruler->setGeometry(area.left(), area.top() - height, area.width(), height);
            const int left = _decl->labelWidth() + 4;
            viewButton->setGeometry(left, 18, 20, 20);
            brightnessButton->setGeometry(left + 23, 22, 16, 16);
            brightnessSlider->setGeometry(left + 23, 38, 24, 70);
        }

        class TitleBar;
        class TimeRuler;
        class StrokeGesture;
        class SelectGesture;
        class KeyGesture;
        class BackgroundLayer;
        class CurveLayer;
    };

    // The bar above the time ruler with the title, the name of the audio file
    class FrequencyMapView::Impl::TitleBar : public QWidget {
    public:
        TitleBar(Impl *impl, QWidget *parent) : QWidget(parent), m_impl(impl) {
        }

    protected:
        void paintEvent(QPaintEvent *event) override {
            Q_UNUSED(event);
            const auto view = m_impl->_decl;
            QPainter painter(this);
            painter.fillRect(rect(), view->whiteRowColor());
            painter.setPen(view->rowLineColor());
            painter.drawLine(QPointF(0, height() - 0.5), QPointF(width(), height() - 0.5));
            // The unit of the time ruler at the right end, and the title in the rest
            const QRect text = rect().adjusted(4, 0, -4, 0);
            const QString unit = FrequencyMapView::tr("ms");
            const int unitWidth = fontMetrics().horizontalAdvance(unit);
            painter.setPen(view->labelColor());
            painter.drawText(text, Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, unit);
            const QRect title = text.adjusted(0, 0, -unitWidth - fontMetrics().height(), 0);
            painter.setPen(palette().color(QPalette::Text));
            painter.drawText(
                title, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                fontMetrics().elidedText(m_impl->title, Qt::ElideRight, title.width()));
        }

    private:
        Impl *m_impl;
    };

    // The ruler of milliseconds above the viewport, with a labeled mark at each interval of
    // FrequencyMapView::rulerInterval() and unlabeled marks between them
    class FrequencyMapView::Impl::TimeRuler : public QWidget {
    public:
        TimeRuler(Impl *impl, QWidget *parent) : QWidget(parent), m_impl(impl) {
        }

    protected:
        void paintEvent(QPaintEvent *event) override {
            Q_UNUSED(event);
            const auto view = m_impl->_decl;
            QPainter painter(this);
            painter.fillRect(rect(), view->whiteRowColor());
            painter.setPen(view->rowLineColor());
            painter.drawLine(QPointF(0, height() - 0.5), QPointF(width(), height() - 0.5));
            const int labels = view->labelWidth();
            painter.setPen(view->labelColor());

            const double pixels = view->timeAxis().pixelsPerTick;
            const auto &metrics = fontMetrics();
            const int labelWidth =
                metrics.horizontalAdvance(QStringLiteral("00000")) + metrics.height();
            const double interval = rulerInterval(pixels, labelWidth);
            // Four marks between labeled marks 2 times a power of ten apart, and five otherwise
            const double mantissa = interval / std::pow(10, std::floor(std::log10(interval)));
            const int divisions = std::lround(mantissa) == 2 ? 4 : 5;
            const double step = interval / divisions;
            const bool unlabeled = step * pixels >= 4;
            // The decimals of a label, for an interval below a millisecond
            const int decimals = std::max(0, -int(std::floor(std::log10(interval) + 1e-9)));
            painter.setClipRect(QRect(labels, 0, width() - labels, height()));
            const double first = std::max(0.0, view->timeAxis().toTick(labels));
            const double last = view->timeAxis().toTick(width());
            for (auto i = qint64(std::ceil(first / step)); i * step <= last; ++i) {
                const bool labeled = i % divisions == 0;
                if (!labeled && !unlabeled) {
                    continue;
                }
                const double x = std::floor(view->xOf(i * step)) + 0.5;
                const double top = labeled ? height() * 0.4 : height() * 0.7;
                painter.drawLine(QPointF(x, top), QPointF(x, height()));
                if (labeled) {
                    painter.drawText(QRectF(x + 3, 0, labelWidth, height() * 0.75),
                                     Qt::AlignLeft | Qt::AlignVCenter,
                                     QString::number(i * step, 'f', decimals));
                }
            }
        }

    private:
        Impl *m_impl;
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

    // A drag that selects the frames between the press and the pointer and marks them with a band
    class FrequencyMapView::Impl::SelectGesture : public SceneGesture {
    public:
        SelectGesture(Impl *impl, QPointF origin)
            : m_impl(impl), m_anchor(*impl->_decl->frameAt(origin.x())),
              m_origin(impl->_decl->timeAxis().toTick(origin.x())), m_previous(impl->selection) {
            m_impl->setSelection(between(m_anchor, m_anchor));
        }

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override {
            Q_UNUSED(modifiers);
            const auto view = m_impl->_decl;
            m_impl->band = {m_origin, view->timeAxis().toTick(position.x())};
            m_impl->setSelection(between(m_anchor, *view->frameAt(position.x())));
            view->viewport()->update();
        }

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override {
            move(position, modifiers);
            clearBand();
        }

        void cancel() override {
            clearBand();
            m_impl->setSelection(m_previous);
        }

        bool hasAutoScroll() const override {
            return true;
        }

    private:
        Impl *m_impl;
        std::size_t m_anchor;
        double m_origin;
        std::optional<std::pair<std::size_t, std::size_t>> m_previous;

        void clearBand() {
            m_impl->band.reset();
            m_impl->_decl->viewport()->update();
        }
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

    // The frames of the map, the selected ones in their own color
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
                // The proposed frequencies of the corrections under the curve, as frqeditor draws
                // them
                if (!impl->corrections.empty()) {
                    auto proposed = impl->buffer->map().frames;
                    for (std::size_t i = 0; i < proposed.size(); ++i) {
                        proposed[i].frequency =
                            i < impl->corrections.size() && impl->corrections[i].reason !=
                                                                kit::FrequencyMapCheck::NoCorrection
                                ? impl->corrections[i].frequency
                                : 0;
                    }
                    drawCurve(painter, *view, proposed, view->correctionColor(),
                              view->selectedCorrectionColor(), impl);
                }
                drawCurve(painter, *view, impl->buffer->map().frames, view->curveColor(),
                          view->selectedCurveColor(), impl);
                // Over the whole height, its top and bottom edges out of sight
                if (impl->band) {
                    const double left = view->xOf(std::min(impl->band->first, impl->band->second));
                    const double right = view->xOf(std::max(impl->band->first, impl->band->second));
                    auto color = view->bandColor();
                    painter.setPen(QPen(color, 1));
                    color.setAlphaF(0.15f);
                    painter.setBrush(color);
                    painter.drawRect(QRectF(QPointF(left, -1), QPointF(right, area.height() + 1)));
                }
                painter.restore();
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
            hit.cursor = impl->tool == PenTool ? Qt::CrossCursor : Qt::ArrowCursor;
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
            // The tool of the press, which a shortcut may change before the drag starts
            const auto scene = impl->tool == PenTool
                                   ? FrequencyViewKeyboardModifiers::DrawDragScene
                                   : FrequencyViewKeyboardModifiers::SelectDragScene;
            return std::make_unique<PressGesture>(
                position,
                [impl, position,
                 scene](Qt::KeyboardModifiers held) -> std::unique_ptr<SceneGesture> {
                    const auto activation = impl->bindings.activate(scene, held);
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
            // The columns of the frames with a correction
            if (const auto corrected = impl->frames(); corrected && !impl->corrections.empty()) {
                const auto &all = *corrected;
                for (std::size_t i = 0; i < all.size() && i < impl->corrections.size(); ++i) {
                    if (impl->corrections[i].reason == kit::FrequencyMapCheck::NoCorrection) {
                        continue;
                    }
                    const double left = view->xOf(all[i].time);
                    const double right =
                        i + 1 < all.size()
                            ? view->xOf(all[i + 1].time)
                            : left + view->timeAxis().pixelsPerTick * impl->frameTime;
                    painter.fillRect(QRectF(QPointF(std::floor(left), plot.top()),
                                            QPointF(std::floor(right), plot.bottom() + 1)),
                                     view->correctionBackgroundColor());
                }
            }
            painter.restore();

            // A label at each C from C0 to C8, centered on its row and in the column
            painter.setPen(view->labelColor());
            for (int note = std::max(lowestNote, (lowest + 11) / 12 * 12); note <= highest;
                 note += 12) {
                if (note >= 120) {
                    break;
                }
                const QRectF row(QPointF(0, keys.toY(note + 1)), QPointF(labels, keys.toY(note)));
                painter.drawText(row.adjusted(0, -20, 0, 20), Qt::AlignHCenter | Qt::AlignVCenter,
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
        impl.titleBar = new Impl::TitleBar(&impl, this);
        impl.ruler = new Impl::TimeRuler(&impl, this);
        setViewportMargins(0, 2 * impl.rowHeight(), 0, 0);
        connect(this, &SceneView::timeAxisChanged, this, [this] {
            stdc_impl_t;
            impl.updateRange();
            impl.ruler->update();
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
        impl.corrections.clear();
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

    const std::vector<kit::FrequencyMapCheck::Correction> &FrequencyMapView::corrections() const {
        stdc_impl_t;
        return impl.corrections;
    }

    void FrequencyMapView::setCorrections(
        std::vector<kit::FrequencyMapCheck::Correction> corrections) {
        stdc_impl_t;
        impl.corrections = std::move(corrections);
        viewport()->update();
    }

    void FrequencyMapView::scrollToFrame(std::size_t frame) {
        stdc_impl_t;
        if (!impl.buffer || frame >= impl.buffer->map().frames.size()) {
            return;
        }
        TimeAxis axis = timeAxis();
        axis.left = impl.buffer->map().frames[frame].time - labelWidth() / axis.pixelsPerTick;
        setTimeAxis(axis);
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
        impl.titleBar->update();
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

    FrequencyMapView::Tool FrequencyMapView::tool() const {
        stdc_impl_t;
        return impl.tool;
    }

    void FrequencyMapView::setTool(Tool tool) {
        stdc_impl_t;
        impl.tool = tool;
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
        const auto &metrics = fontMetrics();
        return metrics.horizontalAdvance(QStringLiteral("C0")) + metrics.height();
    }

    QWidget *FrequencyMapView::titleBar() const {
        stdc_impl_t;
        return impl.titleBar;
    }

    QWidget *FrequencyMapView::timeRuler() const {
        stdc_impl_t;
        return impl.ruler;
    }

    double FrequencyMapView::rulerInterval(double pixelsPerMillisecond, double spacing) {
        const double least = spacing / pixelsPerMillisecond;
        double power = std::pow(10, std::floor(std::log10(least)));
        for (;;) {
            for (const int mantissa : {1, 2, 5}) {
                if (mantissa * power >= least) {
                    return mantissa * power;
                }
            }
            power *= 10;
        }
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
        impl.titleBar->update();                                                                   \
        impl.ruler->update();                                                                      \
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
    HELLO_COLOR_PROPERTY(bandColor, setBandColor, palette().color(QPalette::Highlight))
    HELLO_COLOR_PROPERTY(correctionColor, setCorrectionColor, QColor(128, 128, 255))
    HELLO_COLOR_PROPERTY(selectedCorrectionColor, setSelectedCorrectionColor, QColor(255, 128, 128))
    HELLO_COLOR_PROPERTY(correctionBackgroundColor, setCorrectionBackgroundColor,
                         QColor(255, 192, 192))

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
        // Level 0 with C0 at the bottom once the view has a size
        if (!impl.placed && viewport()->height() > 0 && viewport()->width() > 0) {
            impl.placed = true;
            impl.place(0, 0, lowestNote);
        }
        impl.placeWidgets();
    }

    void FrequencyMapView::changeEvent(QEvent *event) {
        stdc_impl_t;
        SceneView::changeEvent(event);
        if (event->type() == QEvent::FontChange) {
            setViewportMargins(0, 2 * impl.rowHeight(), 0, 0);
            impl.placeWidgets();
        }
    }

    QCursor FrequencyMapView::cursorOf(Qt::CursorShape shape) const {
        if (shape != Qt::CrossCursor) {
            return SceneView::cursorOf(shape);
        }
        constexpr int size = 21;
        constexpr double middle = size / 2.0;
        const qreal ratio = devicePixelRatioF();
        QPixmap pixmap(QSize(size, size) * ratio);
        pixmap.setDevicePixelRatio(ratio);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        // The white outline first, a pixel longer at each end
        painter.setPen(QPen(Qt::white, 3, Qt::SolidLine, Qt::SquareCap));
        painter.drawLine(QPointF(middle, 1.5), QPointF(middle, size - 1.5));
        painter.drawLine(QPointF(1.5, middle), QPointF(size - 1.5, middle));
        painter.setPen(QPen(Qt::black, 1, Qt::SolidLine, Qt::FlatCap));
        painter.drawLine(QPointF(middle, 1), QPointF(middle, size - 1));
        painter.drawLine(QPointF(1, middle), QPointF(size - 1, middle));
        painter.end();
        return QCursor(pixmap, size / 2, size / 2);
    }

}
