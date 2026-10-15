#include <cmath>
#include <memory>

#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <hellokit/VoiceBank/FrequencyMapRevisionBuffer.h>

#include <helloutau/Editor/FrequencyMapView.h>

using namespace hello::daw;
using hello::kit::FrequencyMap;
using hello::kit::FrequencyMapRevisionBuffer;

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

    bool near(double a, double b, double tolerance) {
        return std::abs(a - b) <= tolerance;
    }

}

class test_FrequencyMapView : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<FrequencyMapView> m_view;
    std::unique_ptr<FrequencyMapRevisionBuffer> m_buffer;

    // The point of frame at frequency in the view
    QPoint pointOf(int frame, double frequency) const {
        return QPoint(int(std::lround(m_view->xOf(frame * 10))),
                      int(std::lround(m_view->yOf(frequency))));
    }

    // Sent directly with the button held, which QTest::mouseMove does not hold
    void moveTo(QPoint position, Qt::MouseButton button, Qt::KeyboardModifiers modifiers = {}) {
        QMouseEvent event(QEvent::MouseMove, position, m_view->viewport()->mapToGlobal(position),
                          Qt::NoButton, button, modifiers);
        QApplication::sendEvent(m_view->viewport(), &event);
    }

    void drag(QPoint from, QPoint to, Qt::MouseButton button,
              Qt::KeyboardModifiers modifiers = {}) {
        QTest::mousePress(m_view->viewport(), button, modifiers, from);
        moveTo(from + QPoint(1, 0), button, modifiers);
        moveTo(to, button, modifiers);
        QTest::mouseRelease(m_view->viewport(), button, modifiers, to);
    }

private Q_SLOTS:
    void init() {
        m_buffer = std::make_unique<FrequencyMapRevisionBuffer>(flatMap());
        m_view = std::make_unique<FrequencyMapView>();
        m_view->resize(800, 400);
        m_view->show();
        m_view->setBuffer(m_buffer.get());
    }

    void cleanup() {
        m_view.reset();
        m_buffer.reset();
    }

    // The view fits the map across, and maps positions to frames and frequencies and back.
    void positions_map_to_frames_and_frequencies() {
        QVERIFY(near(m_view->xOf(1000), m_view->viewport()->width(), 1e-6));
        QCOMPARE(m_view->frameAt(m_view->xOf(42)), std::optional<std::size_t>(4));
        QCOMPARE(m_view->frameAt(-100), std::optional<std::size_t>(0));
        QCOMPARE(m_view->frameAt(1e6), std::optional<std::size_t>(99));
        QVERIFY(near(m_view->frequencyAt(m_view->yOf(330)), 330, 1e-9));
        // 200 Hz is in view, an octave of room beyond the voiced notes.
        QVERIFY(m_view->yOf(200) > 0 && m_view->yOf(200) < m_view->viewport()->height());

        m_view->setBuffer(nullptr);
        QCOMPARE(m_view->frameAt(0), std::nullopt);
    }

    // A left drag draws the frames under the pointer as one step.
    void a_left_drag_draws_one_step() {
        QSignalSpy edited(m_view.get(), &FrequencyMapView::edited);
        drag(pointOf(10, 400), pointOf(20, 400), Qt::LeftButton);
        QCOMPARE(edited.count(), 1);
        for (int i = 10; i <= 20; ++i) {
            // Within a pixel of the view
            QVERIFY2(near(m_buffer->map().frames[i].frequency, 400, 400 * 0.02),
                     qPrintable(QString::number(m_buffer->map().frames[i].frequency)));
        }
        QCOMPARE(m_buffer->map().frames[9].frequency, 200.0);
        QCOMPARE(m_buffer->map().frames[21].frequency, 200.0);
        QVERIFY(m_buffer->undo());
        QVERIFY(!m_buffer->isModified());
    }

    // Escape cancels a stroke in progress, which leaves no step.
    void escape_cancels_a_stroke() {
        QSignalSpy edited(m_view.get(), &FrequencyMapView::edited);
        QTest::mousePress(m_view->viewport(), Qt::LeftButton, {}, pointOf(10, 400));
        moveTo(pointOf(20, 400), Qt::LeftButton);
        QVERIFY(m_buffer->isStroking());
        QTest::keyClick(m_view.get(), Qt::Key_Escape);
        QTest::mouseRelease(m_view->viewport(), Qt::LeftButton, {}, pointOf(20, 400));
        QVERIFY(!m_buffer->isStroking());
        QVERIFY(!m_buffer->isModified());
        QVERIFY(!m_buffer->canUndo());
        QCOMPARE(edited.count(), 0);
    }

    // A left drag with Ctrl selects the frames between the press and the pointer.
    void a_ctrl_drag_selects_frames() {
        QSignalSpy changed(m_view.get(), &FrequencyMapView::selectionChanged);
        drag(pointOf(40, 200), pointOf(30, 300), Qt::LeftButton, Qt::ControlModifier);
        QVERIFY(changed.count() > 0);
        QCOMPARE(m_view->selection(), (std::optional<std::pair<std::size_t, std::size_t>>{
                                          {30, 11}
        }));
        QVERIFY(!m_buffer->isModified());
        m_view->selectAll();
        QCOMPARE(m_view->selection(), (std::optional<std::pair<std::size_t, std::size_t>>{
                                          {0, 100}
        }));
        m_view->clearSelection();
        QCOMPARE(m_view->selection(), std::nullopt);
    }

    // A right drag unvoices the frames under the pointer.
    void a_right_drag_unvoices() {
        drag(pointOf(50, 200), pointOf(55, 600), Qt::RightButton);
        for (int i = 50; i <= 55; ++i) {
            QCOMPARE(m_buffer->map().frames[i].frequency, 0.0);
        }
        QCOMPARE(m_buffer->map().frames[56].frequency, 200.0);
    }
};

QTEST_MAIN(test_FrequencyMapView)

#include "test_FrequencyMapView.moc"
