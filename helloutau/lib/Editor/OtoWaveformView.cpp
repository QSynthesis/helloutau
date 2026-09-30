#include "OtoWaveformView.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

#include <QtGui/QImage>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QAbstractSpinBox>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QTextEdit>

#include <stdcorelib/pimpl.h>

namespace hello::daw {

    namespace {

        // The height of the time axis above the waveform
        constexpr int axisHeight = 18;

        // The largest zoom, some samples of a 44.1 kHz file per pixel
        constexpr double maximumScale = 40;

        double rounded(double value) {
            // + 0.0 turns a negative zero into zero, which writes without a sign.
            return std::round(value * 1000) / 1000 + 0.0;
        }

        constexpr std::array<OtoWaveformView::Value, 5> allValues = {
            OtoWaveformView::Offset, OtoWaveformView::Overlap, OtoWaveformView::PreUtterance,
            OtoWaveformView::Consonant, OtoWaveformView::Cutoff};

    }

    class OtoWaveformView::Impl {
    public:
        using Decl = OtoWaveformView;

        explicit Impl(Decl *decl) : _decl(decl) {
        }

        Decl *_decl;
        std::shared_ptr<const kit::WaveAudio> audio;
        // The audio of all channels mixed, one sample per frame
        std::vector<float> mono;
        std::optional<kit::VoiceOtoEntry> entry;

        double scale = 1;
        double viewStart = 0;
        // Whether the whole audio is shown, which a resize keeps
        bool fitted = true;
        bool syncing = false;

        std::optional<double> pointerX;
        std::optional<Value> hovered;
        // The drag in progress: the value it moves and the entry before it
        std::optional<Value> dragging;
        kit::VoiceOtoEntry original;

        std::optional<double> playhead;
        double grip = 5;

        QColor waveColor, maskColor, consonantColor, preUtteranceColor, overlapColor, boundaryColor,
            envelopeColor, playheadColor, frequencyColor, spectrumColor;

        std::shared_ptr<const kit::Spectrogram> spectrogram;
        std::optional<kit::FrequencyTable> table;
        // The pitch axis, see OtoWaveformView::pitchRange()
        std::pair<double, double> range{36, 84};

        // The spectrogram drawn for the view it was drawn in
        struct SpectrumImage {
            QImage image;
            double scale = 0;
            double viewStart = 0;
            QSize size;
            std::pair<double, double> range;
            const kit::Spectrogram *spectrogram = nullptr;
            QRgb background = 0;
            QRgb color = 0;
        } spectrumImage;

        static double noteOf(double frequency) {
            return 69 + 12 * std::log2(frequency / 440);
        }

        void updateRange() {
            double low = std::numeric_limits<double>::infinity();
            double high = -low;
            if (table) {
                for (const auto &frame : table->frames) {
                    if (frame.frequency > 0 && std::isfinite(frame.frequency)) {
                        low = std::min(low, noteOf(frame.frequency));
                        high = std::max(high, noteOf(frame.frequency));
                    }
                }
            }
            if (low > high) {
                range = {36, 84};
                return;
            }
            // Within C1 and the top of B7, the keys of prefix.map
            range = {std::max(24.0, std::floor(low) - 12), std::min(108.0, std::ceil(high) + 12)};
            if (range.second <= range.first) {
                range = {36, 84};
            }
        }

        // The spectrogram of the view in area of the viewport
        const QImage &spectrumImageOf(const QRect &area) {
            stdc_decl_t;
            const auto background = decl.palette().color(QPalette::Base).rgb();
            const auto color = decl.spectrumColor().rgb();
            auto &cached = spectrumImage;
            if (cached.spectrogram == spectrogram.get() && cached.scale == scale &&
                cached.viewStart == viewStart && cached.size == area.size() &&
                cached.range == range && cached.background == background && cached.color == color) {
                return cached.image;
            }
            cached = {QImage(area.size(), QImage::Format_RGB32),
                      scale,
                      viewStart,
                      area.size(),
                      range,
                      spectrogram.get(),
                      background,
                      color};
            const auto &data = *spectrogram;
            const double peak = std::max(1e-9f, data.peak());
            // The bin of each row, the top first
            std::vector<int> bins(size_t(std::max(0, area.height())));
            for (int y = 0; y < area.height(); ++y) {
                const double note =
                    range.second - (y + 0.5) / area.height() * (range.second - range.first);
                bins[size_t(y)] =
                    int(std::lround(data.binOf(440 * std::pow(2.0, (note - 69) / 12))));
            }
            const auto blend = [&](double level) {
                const auto mix = [level](int a, int b) { return int(a + (b - a) * level); };
                return qRgb(mix(qRed(background), qRed(color)),
                            mix(qGreen(background), qGreen(color)),
                            mix(qBlue(background), qBlue(color)));
            };
            for (int x = 0; x < area.width(); ++x) {
                const int frame = int(std::lround(decl.timeAt(x + 0.5) * data.sampleRate() / 1000 /
                                                  kit::Spectrogram::hopSize));
                for (int y = 0; y < area.height(); ++y) {
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

        // The smallest and largest sample under each pixel column, for the view they were
        // taken in
        struct Peaks {
            double scale = 0;
            double viewStart = 0;
            int width = 0;
            std::vector<std::pair<float, float>> columns;
        } peaks;

        double duration() const {
            return audio ? audio->duration() : 0;
        }

        double minimumScale() const {
            stdc_decl_t;
            const double length = duration();
            return length > 0 ? std::min(maximumScale, decl.viewport()->width() / length) : 1;
        }

        // Keeps the view within the audio and the scroll bar in step with it.
        void clampView() {
            stdc_decl_t;
            scale = std::clamp(scale, minimumScale(), maximumScale);
            const double visible = decl.viewport()->width() / scale;
            viewStart = std::clamp(viewStart, 0.0, std::max(0.0, duration() - visible));
            syncing = true;
            const auto bar = decl.horizontalScrollBar();
            bar->setRange(
                0, int(std::ceil(std::max(0.0, duration() * scale - decl.viewport()->width()))));
            bar->setPageStep(decl.viewport()->width());
            bar->setSingleStep(std::max(1, decl.viewport()->width() / 20));
            bar->setValue(int(std::lround(viewStart * scale)));
            syncing = false;
            decl.viewport()->update();
        }

        bool showsValues() const {
            return entry && audio;
        }

        // The value whose boundary lies within the grip of x, the nearest, the first in the
        // order of the keys among equals
        std::optional<Value> valueAt(double x) const {
            stdc_decl_t;
            if (!showsValues()) {
                return std::nullopt;
            }
            std::optional<Value> found;
            double nearest = grip;
            for (const auto value : allValues) {
                const double distance =
                    std::abs(decl.xOf(positionOf(*entry, value, duration())) - x);
                if (distance <= nearest && (!found || distance < nearest)) {
                    found = value;
                    nearest = distance;
                }
            }
            return found;
        }

        void updateHover() {
            stdc_decl_t;
            const auto value = dragging ? dragging : pointerX ? valueAt(*pointerX) : std::nullopt;
            if (value != hovered) {
                hovered = value;
                decl.viewport()->setCursor(hovered ? Qt::SizeHorCursor : Qt::ArrowCursor);
                decl.viewport()->update();
            }
        }

        const std::vector<std::pair<float, float>> &columns() {
            stdc_decl_t;
            const int width = decl.viewport()->width();
            if (peaks.scale == scale && peaks.viewStart == viewStart && peaks.width == width) {
                return peaks.columns;
            }
            peaks = {scale, viewStart, width, {}};
            if (!audio || mono.empty()) {
                return peaks.columns;
            }
            const double perMs = audio->sampleRate / 1000.0;
            const auto count = qint64(mono.size());
            peaks.columns.resize(size_t(std::max(0, width)), {0.0f, 0.0f});
            for (int x = 0; x < width; ++x) {
                const auto first = qint64(std::floor(decl.timeAt(x) * perMs));
                const auto last =
                    std::max(first + 1, qint64(std::floor(decl.timeAt(x + 1) * perMs)));
                if (first >= count || last <= 0) {
                    continue;
                }
                float low = 1;
                float high = -1;
                for (auto i = std::max<qint64>(0, first); i < std::min(last, count); ++i) {
                    low = std::min(low, mono[size_t(i)]);
                    high = std::max(high, mono[size_t(i)]);
                }
                if (low <= high) {
                    peaks.columns[size_t(x)] = {low, high};
                }
            }
            return peaks.columns;
        }

        // A step of the time axis that leaves some 80 pixels between labels
        double axisStep() const {
            for (const double step : {1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0,
                                      2000.0, 5000.0, 10000.0}) {
                if (step * scale >= 80) {
                    return step;
                }
            }
            return 20000;
        }
    };

    OtoWaveformView::OtoWaveformView(QWidget *parent)
        : QAbstractScrollArea(parent), _impl(std::make_unique<Impl>(this)) {
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setFocusPolicy(Qt::StrongFocus);
        viewport()->setMouseTracking(true);
        setMinimumHeight(120);
    }

    OtoWaveformView::~OtoWaveformView() = default;

    std::shared_ptr<const kit::WaveAudio> OtoWaveformView::audio() const {
        stdc_impl_t;
        return impl.audio;
    }

    void OtoWaveformView::setAudio(std::shared_ptr<const kit::WaveAudio> audio) {
        stdc_impl_t;
        impl.audio = std::move(audio);
        impl.mono.clear();
        if (impl.audio && impl.audio->channels > 0) {
            const int channels = impl.audio->channels;
            const auto frames = impl.audio->frameCount();
            impl.mono.resize(size_t(frames));
            for (qsizetype i = 0; i < frames; ++i) {
                float sum = 0;
                for (int c = 0; c < channels; ++c) {
                    sum += impl.audio->samples[size_t(i * channels + c)];
                }
                impl.mono[size_t(i)] = sum / float(channels);
            }
        }
        impl.peaks = {};
        impl.dragging.reset();
        fit();
        impl.updateHover();
    }

    double OtoWaveformView::duration() const {
        stdc_impl_t;
        return impl.duration();
    }

    std::optional<kit::VoiceOtoEntry> OtoWaveformView::entry() const {
        stdc_impl_t;
        return impl.entry;
    }

    void OtoWaveformView::setEntry(const std::optional<kit::VoiceOtoEntry> &entry) {
        stdc_impl_t;
        impl.entry = entry;
        impl.dragging.reset();
        impl.updateHover();
        viewport()->update();
    }

    double OtoWaveformView::scale() const {
        stdc_impl_t;
        return impl.scale;
    }

    double OtoWaveformView::viewStart() const {
        stdc_impl_t;
        return impl.viewStart;
    }

    void OtoWaveformView::zoom(double factor, double x) {
        stdc_impl_t;
        const double time = timeAt(x);
        impl.scale = std::clamp(impl.scale * factor, impl.minimumScale(), maximumScale);
        impl.viewStart = time - x / impl.scale;
        impl.fitted = impl.scale <= impl.minimumScale();
        impl.clampView();
    }

    void OtoWaveformView::fit() {
        stdc_impl_t;
        impl.fitted = true;
        impl.scale = impl.minimumScale();
        impl.viewStart = 0;
        impl.clampView();
    }

    double OtoWaveformView::timeAt(double x) const {
        stdc_impl_t;
        return impl.viewStart + x / impl.scale;
    }

    double OtoWaveformView::xOf(double time) const {
        stdc_impl_t;
        return (time - impl.viewStart) * impl.scale;
    }

    std::optional<double> OtoWaveformView::pointerTime() const {
        stdc_impl_t;
        if (!impl.pointerX) {
            return std::nullopt;
        }
        return timeAt(*impl.pointerX);
    }

    std::optional<OtoWaveformView::Value> OtoWaveformView::activeValue() const {
        stdc_impl_t;
        return impl.hovered;
    }

    bool OtoWaveformView::setValueAt(Value value, double time) {
        stdc_impl_t;
        if (!impl.showsValues() || impl.dragging) {
            return false;
        }
        const auto edited = moved(*impl.entry, value, std::round(time), impl.duration());
        if (edited == *impl.entry) {
            return false;
        }
        impl.entry = edited;
        viewport()->update();
        Q_EMIT entryEdited(edited);
        return true;
    }

    void OtoWaveformView::setPlayhead(std::optional<double> time) {
        stdc_impl_t;
        if (impl.playhead != time) {
            impl.playhead = time;
            viewport()->update();
        }
    }

    std::shared_ptr<const kit::Spectrogram> OtoWaveformView::spectrogram() const {
        stdc_impl_t;
        return impl.spectrogram;
    }

    void OtoWaveformView::setSpectrogram(std::shared_ptr<const kit::Spectrogram> spectrogram) {
        stdc_impl_t;
        impl.spectrogram = std::move(spectrogram);
        viewport()->update();
    }

    std::optional<kit::FrequencyTable> OtoWaveformView::frequencyTable() const {
        stdc_impl_t;
        return impl.table;
    }

    void OtoWaveformView::setFrequencyTable(const std::optional<kit::FrequencyTable> &table) {
        stdc_impl_t;
        impl.table = table;
        impl.updateRange();
        viewport()->update();
    }

    std::pair<double, double> OtoWaveformView::pitchRange() const {
        stdc_impl_t;
        return impl.range;
    }

    double OtoWaveformView::yOfNote(double note) const {
        stdc_impl_t;
        const double height = std::max(1, viewport()->height() - axisHeight);
        const auto [low, high] = impl.range;
        return axisHeight + (high - note) / (high - low) * height;
    }

    double OtoWaveformView::grip() const {
        stdc_impl_t;
        return impl.grip;
    }

    void OtoWaveformView::setGrip(double grip) {
        stdc_impl_t;
        impl.grip = grip;
    }

    double OtoWaveformView::positionOf(const kit::VoiceOtoEntry &entry, Value value,
                                       double duration) {
        switch (value) {
            case Offset:
                return entry.offset;
            case Overlap:
                return entry.offset + entry.voiceOverlap;
            case PreUtterance:
                return entry.offset + entry.preUtterance;
            case Consonant:
                return entry.offset + entry.consonant;
            case Cutoff:
                return entry.cutoff < 0 ? entry.offset - entry.cutoff : duration - entry.cutoff;
        }
        return 0;
    }

    kit::VoiceOtoEntry OtoWaveformView::moved(const kit::VoiceOtoEntry &entry, Value value,
                                              double time, double duration) {
        const double t = std::clamp(time, 0.0, std::max(0.0, duration));
        const double consonantEnd = positionOf(entry, Consonant, duration);
        const double cutoffEnd = positionOf(entry, Cutoff, duration);
        auto result = entry;
        // Writes the cutoff that ends at end, in the sign it had; zero is written negative.
        const auto setCutoffEnd = [&](double end) {
            if (entry.cutoff > 0) {
                result.cutoff = rounded(duration - end);
            } else {
                result.cutoff = rounded(-std::max(end - result.offset, 1.0));
            }
        };
        switch (value) {
            case Offset: {
                result.offset = rounded(t);
                const double newConsonantEnd = std::max(consonantEnd, t);
                result.consonant = rounded(newConsonantEnd - t);
                result.preUtterance = rounded(positionOf(entry, PreUtterance, duration) - t);
                result.voiceOverlap = rounded(positionOf(entry, Overlap, duration) - t);
                // The cutoff stays at its time: a negative one is measured from the offset.
                if (entry.cutoff < 0 || cutoffEnd < newConsonantEnd) {
                    setCutoffEnd(std::max(cutoffEnd, newConsonantEnd));
                }
                break;
            }
            case Consonant: {
                const double end = std::max(t, entry.offset);
                result.consonant = rounded(end - entry.offset);
                if (end > cutoffEnd) {
                    setCutoffEnd(end);
                }
                break;
            }
            case Cutoff:
                setCutoffEnd(std::max(t, consonantEnd));
                break;
            case PreUtterance:
                result.preUtterance = rounded(t - entry.offset);
                break;
            case Overlap:
                result.voiceOverlap = rounded(t - entry.offset);
                break;
        }
        return result;
    }

    QString OtoWaveformView::nameOf(Value value) {
        switch (value) {
            case Offset:
                return tr("Offset");
            case Overlap:
                return tr("Overlap");
            case PreUtterance:
                return tr("Pre-utterance");
            case Consonant:
                return tr("Consonant");
            case Cutoff:
                return tr("Cutoff");
        }
        return {};
    }

    void OtoWaveformView::paintEvent(QPaintEvent *event) {
        Q_UNUSED(event)
        stdc_impl_t;
        QPainter painter(viewport());
        const auto area = viewport()->rect();
        painter.fillRect(area, palette().color(QPalette::Base));
        const QRectF wave(0, axisHeight, area.width(), std::max(1, area.height() - axisHeight));
        const double middle = wave.center().y();
        const double half = wave.height() / 2 - 2;

        // The time axis
        const auto text = palette().color(QPalette::Text);
        painter.setPen(palette().color(QPalette::Mid));
        painter.drawLine(QPointF(0, axisHeight - 0.5), QPointF(area.width(), axisHeight - 0.5));
        if (impl.audio) {
            const double step = impl.axisStep();
            painter.setFont(font());
            for (double time = std::floor(impl.viewStart / step) * step;
                 time <= timeAt(area.width()); time += step) {
                const double x = std::round(xOf(time)) + 0.5;
                painter.setPen(palette().color(QPalette::Mid));
                painter.drawLine(QPointF(x, axisHeight - 5), QPointF(x, axisHeight));
                painter.setPen(text);
                painter.drawText(QPointF(x + 3, axisHeight - 6), QString::number(time));
            }
        } else {
            painter.setPen(palette().color(QPalette::PlaceholderText));
            painter.drawText(wave, Qt::AlignCenter, tr("No audio"));
            return;
        }

        const double length = impl.duration();
        const bool values = impl.showsValues();
        const auto at = [&](Value value) {
            return values ? xOf(positionOf(*impl.entry, value, length)) : 0.0;
        };

        // The spectrogram in place of the waveform
        const double end = xOf(length);
        if (impl.spectrogram) {
            const QRect rect(0, axisHeight, area.width(), int(wave.height()));
            painter.drawImage(rect.topLeft(), impl.spectrumImageOf(rect));
        }

        // The consonant under the waveform
        if (values) {
            painter.fillRect(
                QRectF(QPointF(at(Offset), wave.top()), QPointF(at(Consonant), wave.bottom())),
                consonantColor());
        }

        // The waveform, one column of peaks per pixel
        if (!impl.spectrogram) {
            painter.setPen(QPen(waveColor(), 1));
            const auto &columns = impl.columns();
            for (int x = 0; x < int(columns.size()) && x < end; ++x) {
                const auto [low, high] = columns[size_t(x)];
                painter.drawLine(QPointF(x + 0.5, middle - high * half),
                                 QPointF(x + 0.5, middle - low * half));
            }
            painter.setPen(palette().color(QPalette::Mid));
            painter.drawLine(QPointF(0, middle),
                             QPointF(std::min<double>(end, area.width()), middle));
        }

        // What the offset and the cutoff leave out
        if (values) {
            painter.fillRect(
                QRectF(QPointF(xOf(0), wave.top()), QPointF(at(Offset), wave.bottom())),
                maskColor());
            painter.fillRect(QRectF(QPointF(at(Cutoff), wave.top()), QPointF(end, wave.bottom())),
                             maskColor());
        }

        // The pitch axis, a line at each C, and the curve of the frequency table
        if (impl.spectrogram || impl.table) {
            painter.setRenderHint(QPainter::Antialiasing, false);
            for (int note = int(std::ceil(impl.range.first / 12)) * 12; note < impl.range.second;
                 note += 12) {
                const double y = std::round(yOfNote(note)) + 0.5;
                painter.setPen(QPen(palette().color(QPalette::Mid), 1, Qt::DotLine));
                painter.drawLine(QPointF(0, y), QPointF(area.width(), y));
                painter.setPen(text);
                painter.drawText(QRectF(0, y - 14, area.width() - 3, 13), Qt::AlignRight,
                                 QStringLiteral("C%1").arg(note / 12 - 1));
            }
        }
        if (impl.table) {
            QPainterPath curve;
            bool drawing = false;
            for (const auto &frame : impl.table->frames) {
                if (!(frame.frequency > 0) || !std::isfinite(frame.frequency)) {
                    drawing = false;
                    continue;
                }
                const QPointF point(xOf(frame.time), yOfNote(Impl::noteOf(frame.frequency)));
                if (drawing) {
                    curve.lineTo(point);
                } else {
                    curve.moveTo(point);
                    drawing = true;
                }
            }
            painter.save();
            painter.setClipRect(wave);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QPen(frequencyColor(), 1.5));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(curve);
            painter.restore();
        }
        if (!values) {
            return;
        }

        // The envelope: in over the overlap, out at the cutoff
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(envelopeColor(), 1.5));
        const double top = wave.top() + 3;
        const double bottom = wave.bottom() - 1;
        const double in = std::max(at(Offset), at(Overlap));
        const QPointF envelope[] = {
            {at(Offset), bottom},
            {std::min(in, at(Cutoff)), top},
            {at(Cutoff), top},
            {at(Cutoff), bottom}
        };
        painter.drawPolyline(envelope, 4);
        painter.setRenderHint(QPainter::Antialiasing, false);

        // The boundaries, the active one wider, labeled with the key that sets it
        for (const auto value : allValues) {
            const QColor color = value == PreUtterance ? preUtteranceColor()
                                 : value == Overlap    ? overlapColor()
                                                       : boundaryColor();
            const double x = std::round(at(value)) + 0.5;
            painter.setPen(QPen(color, impl.hovered == value ? 3 : 1));
            painter.drawLine(QPointF(x, wave.top()), QPointF(x, wave.bottom()));
            painter.setPen(color);
            painter.drawText(QPointF(x + 3, wave.top() + 12 + 12 * int(value)),
                             QString::number(int(value) + 1));
        }

        if (impl.playhead) {
            const double x = std::round(xOf(*impl.playhead)) + 0.5;
            painter.setPen(QPen(playheadColor(), 1));
            painter.drawLine(QPointF(x, 0), QPointF(x, area.height()));
        }
    }

    void OtoWaveformView::resizeEvent(QResizeEvent *event) {
        stdc_impl_t;
        QAbstractScrollArea::resizeEvent(event);
        if (impl.fitted) {
            impl.scale = impl.minimumScale();
            impl.viewStart = 0;
        }
        impl.clampView();
    }

    void OtoWaveformView::scrollContentsBy(int dx, int dy) {
        Q_UNUSED(dx)
        Q_UNUSED(dy)
        stdc_impl_t;
        if (impl.syncing) {
            return;
        }
        impl.viewStart = horizontalScrollBar()->value() / impl.scale;
        impl.updateHover();
        viewport()->update();
    }

    bool OtoWaveformView::viewportEvent(QEvent *event) {
        stdc_impl_t;
        switch (event->type()) {
            case QEvent::Enter: {
                // Takes the keys 1 to 5 over the waveform, unless text is being typed
                const auto focus = QApplication::focusWidget();
                if (!qobject_cast<QLineEdit *>(focus) && !qobject_cast<QAbstractSpinBox *>(focus) &&
                    !qobject_cast<QTextEdit *>(focus) && !qobject_cast<QPlainTextEdit *>(focus)) {
                    setFocus(Qt::MouseFocusReason);
                }
                break;
            }
            case QEvent::Leave:
                impl.pointerX.reset();
                impl.updateHover();
                Q_EMIT pointerMoved();
                break;
            default:
                break;
        }
        return QAbstractScrollArea::viewportEvent(event);
    }

    void OtoWaveformView::mousePressEvent(QMouseEvent *event) {
        stdc_impl_t;
        impl.pointerX = event->position().x();
        if (event->button() == Qt::LeftButton && impl.showsValues()) {
            if (const auto value = impl.valueAt(event->position().x())) {
                impl.dragging = value;
                impl.original = *impl.entry;
                impl.updateHover();
                event->accept();
                return;
            }
        }
        QAbstractScrollArea::mousePressEvent(event);
    }

    void OtoWaveformView::mouseMoveEvent(QMouseEvent *event) {
        stdc_impl_t;
        impl.pointerX = event->position().x();
        if (impl.dragging) {
            impl.entry = moved(impl.original, *impl.dragging,
                               std::round(timeAt(event->position().x())), impl.duration());
            viewport()->update();
        }
        impl.updateHover();
        Q_EMIT pointerMoved();
    }

    void OtoWaveformView::mouseReleaseEvent(QMouseEvent *event) {
        stdc_impl_t;
        if (event->button() == Qt::LeftButton && impl.dragging) {
            impl.dragging.reset();
            impl.updateHover();
            if (impl.entry && *impl.entry != impl.original) {
                Q_EMIT entryEdited(*impl.entry);
            }
            return;
        }
        QAbstractScrollArea::mouseReleaseEvent(event);
    }

    void OtoWaveformView::mouseDoubleClickEvent(QMouseEvent *event) {
        stdc_impl_t;
        if (event->button() == Qt::LeftButton && impl.audio &&
            !impl.valueAt(event->position().x())) {
            Q_EMIT playRequested(std::clamp(timeAt(event->position().x()), 0.0, impl.duration()));
            return;
        }
        QAbstractScrollArea::mouseDoubleClickEvent(event);
    }

    void OtoWaveformView::wheelEvent(QWheelEvent *event) {
        stdc_impl_t;
        const auto delta = event->angleDelta();
        if (event->modifiers() & Qt::ControlModifier) {
            zoom(std::pow(1.2, delta.y() / 120.0), event->position().x());
        } else {
            const int steps = delta.x() != 0 ? delta.x() : delta.y();
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() -
                                            steps * viewport()->width() / 8 / 120);
        }
        event->accept();
    }

    void OtoWaveformView::keyPressEvent(QKeyEvent *event) {
        stdc_impl_t;
        if (event->key() == Qt::Key_Escape && impl.dragging) {
            impl.entry = impl.original;
            impl.dragging.reset();
            impl.updateHover();
            viewport()->update();
            return;
        }
        QAbstractScrollArea::keyPressEvent(event);
    }

    QColor OtoWaveformView::waveColor() const {
        stdc_impl_t;
        return impl.waveColor.isValid() ? impl.waveColor : QColor(0x3a, 0x7b, 0xd5);
    }

    void OtoWaveformView::setWaveColor(const QColor &color) {
        stdc_impl_t;
        impl.waveColor = color;
        viewport()->update();
    }

    QColor OtoWaveformView::maskColor() const {
        stdc_impl_t;
        return impl.maskColor.isValid() ? impl.maskColor : QColor(128, 128, 128, 110);
    }

    void OtoWaveformView::setMaskColor(const QColor &color) {
        stdc_impl_t;
        impl.maskColor = color;
        viewport()->update();
    }

    QColor OtoWaveformView::consonantColor() const {
        stdc_impl_t;
        return impl.consonantColor.isValid() ? impl.consonantColor : QColor(80, 160, 255, 50);
    }

    void OtoWaveformView::setConsonantColor(const QColor &color) {
        stdc_impl_t;
        impl.consonantColor = color;
        viewport()->update();
    }

    QColor OtoWaveformView::preUtteranceColor() const {
        stdc_impl_t;
        return impl.preUtteranceColor.isValid() ? impl.preUtteranceColor : QColor(0xe0, 0x40, 0x40);
    }

    void OtoWaveformView::setPreUtteranceColor(const QColor &color) {
        stdc_impl_t;
        impl.preUtteranceColor = color;
        viewport()->update();
    }

    QColor OtoWaveformView::overlapColor() const {
        stdc_impl_t;
        return impl.overlapColor.isValid() ? impl.overlapColor : QColor(0x30, 0xa0, 0x50);
    }

    void OtoWaveformView::setOverlapColor(const QColor &color) {
        stdc_impl_t;
        impl.overlapColor = color;
        viewport()->update();
    }

    QColor OtoWaveformView::boundaryColor() const {
        stdc_impl_t;
        return impl.boundaryColor.isValid() ? impl.boundaryColor : QColor(0x80, 0x80, 0x80);
    }

    void OtoWaveformView::setBoundaryColor(const QColor &color) {
        stdc_impl_t;
        impl.boundaryColor = color;
        viewport()->update();
    }

    QColor OtoWaveformView::envelopeColor() const {
        stdc_impl_t;
        return impl.envelopeColor.isValid() ? impl.envelopeColor : QColor(255, 170, 0, 170);
    }

    void OtoWaveformView::setEnvelopeColor(const QColor &color) {
        stdc_impl_t;
        impl.envelopeColor = color;
        viewport()->update();
    }

    QColor OtoWaveformView::playheadColor() const {
        stdc_impl_t;
        return impl.playheadColor.isValid() ? impl.playheadColor : QColor(0xff, 0x80, 0x00);
    }

    void OtoWaveformView::setPlayheadColor(const QColor &color) {
        stdc_impl_t;
        impl.playheadColor = color;
        viewport()->update();
    }

    QColor OtoWaveformView::frequencyColor() const {
        stdc_impl_t;
        return impl.frequencyColor.isValid() ? impl.frequencyColor : QColor(0xe0, 0x30, 0xa0);
    }

    void OtoWaveformView::setFrequencyColor(const QColor &color) {
        stdc_impl_t;
        impl.frequencyColor = color;
        viewport()->update();
    }

    QColor OtoWaveformView::spectrumColor() const {
        stdc_impl_t;
        return impl.spectrumColor.isValid() ? impl.spectrumColor : QColor(0x20, 0x60, 0xd0);
    }

    void OtoWaveformView::setSpectrumColor(const QColor &color) {
        stdc_impl_t;
        impl.spectrumColor = color;
        viewport()->update();
    }

}
