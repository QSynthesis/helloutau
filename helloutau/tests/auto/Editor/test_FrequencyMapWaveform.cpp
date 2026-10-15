#include <cmath>
#include <memory>

#include <QtGui/QWheelEvent>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/VoiceBank/FrequencyMapRevisionBuffer.h>

#include <helloutau/Editor/FrequencyMapView.h>
#include <helloutau/Editor/FrequencyMapWaveform.h>

using namespace hello::daw;
using hello::kit::FrequencyMap;
using hello::kit::FrequencyMapRevisionBuffer;
using hello::kit::WaveAudio;

namespace {

    // A map of 100 frames 10 milliseconds apart at 200 Hz
    FrequencyMap flatMap() {
        FrequencyMap map;
        map.hop = 441;
        for (int i = 0; i < 100; ++i) {
            FrequencyMap::Frame frame;
            frame.time = i * 10;
            frame.frequency = 200;
            frame.amplitude = 1;
            map.frames.push_back(frame);
        }
        return map;
    }

    // A second at 1000 Hz, silent for the first half and alternating between -0.8 and 0.8 in the
    // second half
    std::shared_ptr<const WaveAudio> halfSilentAudio() {
        auto audio = std::make_shared<WaveAudio>();
        audio->sampleRate = 1000;
        audio->channels = 1;
        for (int i = 0; i < 1000; ++i) {
            audio->samples.push_back(i < 500 ? 0.0f : (i % 2 ? 0.8f : -0.8f));
        }
        return audio;
    }

}

class test_FrequencyMapWaveform : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QWidget> m_window;
    FrequencyMapView *m_view = nullptr;
    FrequencyMapWaveform *m_waveform = nullptr;
    std::unique_ptr<FrequencyMapRevisionBuffer> m_buffer;

    // The color drawn at time, halfway between the zero line and the top of the lane
    QColor colorAt(double time) const {
        const auto image = m_waveform->grab().toImage();
        const QRect area = m_waveform->contentsRect();
        return image.pixelColor(int(std::floor(m_waveform->xOf(time))),
                                area.top() + area.height() / 4);
    }

    // Returns whether the column of time has the color of the waveform off the zero line, where
    // silence is drawn.
    bool waveAt(double time) const {
        const auto image = m_waveform->grab().toImage();
        const QRect area = m_waveform->contentsRect();
        const int x = int(std::floor(m_waveform->xOf(time)));
        const double zero = area.top() + area.height() / 2.0;
        for (int y = area.top(); y <= area.bottom(); ++y) {
            if (std::abs(y + 0.5 - zero) > 2 && image.pixelColor(x, y) == m_waveform->waveColor()) {
                return true;
            }
        }
        return false;
    }

private Q_SLOTS:
    void init() {
        m_buffer = std::make_unique<FrequencyMapRevisionBuffer>(flatMap());
        m_window = std::make_unique<QWidget>();
        auto layout = new QVBoxLayout(m_window.get());
        layout->setContentsMargins(0, 0, 0, 0);
        m_view = new FrequencyMapView();
        m_waveform = new FrequencyMapWaveform(m_view);
        layout->addWidget(m_view, 3);
        layout->addWidget(m_waveform, 1);
        m_window->resize(800, 500);
        m_window->show();
        m_view->setBuffer(m_buffer.get());
        m_waveform->setAudio(halfSilentAudio());
    }

    void cleanup() {
        m_window.reset();
        m_buffer.reset();
    }

    // A time is drawn at the x of the time in the view, at each zoom level.
    void the_waveform_follows_the_time_axis_of_the_view() {
        const auto viewX = [this](double time) {
            return m_view->viewport()->mapTo(m_window.get(), QPointF(m_view->xOf(time), 0)).x();
        };
        const auto ownX = [this](double time) {
            return m_waveform->mapTo(m_window.get(), QPointF(m_waveform->xOf(time), 0)).x();
        };
        QCOMPARE(ownX(750), viewX(750));
        QVERIFY(waveAt(750));
        QVERIFY(!waveAt(250));

        m_view->setZoomLevel(3);
        QCOMPARE(ownX(520), viewX(520));
        QVERIFY(waveAt(520));
        QVERIFY(!waveAt(480));
    }

    // The playhead is a line at its time.
    void the_playhead_is_drawn_at_its_time() {
        QCOMPARE(m_waveform->playhead(), std::nullopt);
        m_waveform->setPlayhead(250);
        QCOMPARE(m_waveform->playhead(), std::optional<double>(250));
        QCOMPARE(colorAt(250), m_waveform->playheadColor());
        m_waveform->setPlayhead(std::nullopt);
        QCOMPARE(colorAt(250), m_view->whiteRowColor());
    }

    // A wheel step over the waveform scrolls and zooms the view, here a zoom of the time.
    void the_wheel_reaches_the_view() {
        const double scale = m_view->timeAxis().pixelsPerTick;
        const QPointF position(m_waveform->width() / 2.0, m_waveform->height() / 2.0);
        QWheelEvent event(position, m_waveform->mapToGlobal(position), QPoint(), QPoint(0, 120),
                          Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(m_waveform, &event);
        QVERIFY(m_view->timeAxis().pixelsPerTick != scale);
    }
};

QTEST_MAIN(test_FrequencyMapWaveform)

#include "test_FrequencyMapWaveform.moc"
