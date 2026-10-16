#include <cmath>
#include <memory>

#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QSlider>
#include <QtWidgets/QToolButton>

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

    // Returns the point of the frame at frequency in the view.
    QPoint pointOf(int frame, double frequency) const {
        return QPoint(int(std::lround(m_view->xOf(frame * 10))),
                      int(std::lround(m_view->yOf(frequency))));
    }

    // Sends the move directly with the button held, which QTest::mouseMove does not hold.
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

    // The view starts at zoom level 0 with C0 at the bottom and the first frame at the right of
    // the labels, and maps positions to frames and frequencies and back.
    void the_view_starts_as_frqeditor() {
        QCOMPARE(m_view->zoomLevel(), 0);
        QCOMPARE(m_view->keyAxis().pixelsPerKey, double(FrequencyMapView::semitonePixels[0]));
        QVERIFY(near(m_view->xOf(10) - m_view->xOf(0), FrequencyMapView::framePixels[0], 1e-9));
        QVERIFY(near(m_view->keyAxis().toKey(m_view->viewport()->height()),
                     FrequencyMapView::lowestNote, 1e-9));
        QVERIFY(near(m_view->xOf(0), m_view->labelWidth(), 1e-9));
        // C8 is in view.
        QVERIFY(m_view->yOf(4186.01) > 0);

        // The frame whose column holds the position
        QCOMPARE(m_view->frameAt(m_view->xOf(42)), std::optional<std::size_t>(4));
        QCOMPARE(m_view->frameAt(-100), std::optional<std::size_t>(0));
        QCOMPARE(m_view->frameAt(1e6), std::optional<std::size_t>(99));
        QVERIFY(near(m_view->frequencyAt(m_view->yOf(330)), 330, 1e-9));

        m_view->setBuffer(nullptr);
        QCOMPARE(m_view->frameAt(0), std::nullopt);
    }

    // A zoom level sets both axes, keeps the first frame and the lowest note, and stays when
    // another buffer is set.
    void the_zoom_levels_set_both_axes() {
        m_view->setZoomLevel(6);
        QCOMPARE(m_view->zoomLevel(), 6);
        QCOMPARE(m_view->keyAxis().pixelsPerKey, double(FrequencyMapView::semitonePixels[6]));
        QVERIFY(near(m_view->xOf(10) - m_view->xOf(0), FrequencyMapView::framePixels[6], 1e-9));
        QVERIFY(near(m_view->keyAxis().toKey(m_view->viewport()->height()),
                     FrequencyMapView::lowestNote, 1e-9));
        QVERIFY(near(m_view->xOf(0), m_view->labelWidth(), 1e-9));
        m_view->setZoomLevel(9);
        QCOMPARE(m_view->zoomLevel(), 6);

        FrequencyMapRevisionBuffer other(flatMap());
        m_view->setBuffer(&other);
        QCOMPARE(m_view->zoomLevel(), 6);
        m_view->setBuffer(m_buffer.get());
    }

    // The \c s button switches the view, and the brightness and the color scheme are limited.
    void the_controls_switch_the_view() {
        QSignalSpy modes(m_view.get(), &FrequencyMapView::viewModeChanged);
        QSignalSpy spectrum(m_view.get(), &FrequencyMapView::spectrumChanged);
        QCOMPARE(m_view->viewMode(), FrequencyMapView::KeyRowsView);
        QVERIFY(m_view->brightnessButton()->isHidden());
        m_view->viewButton()->click();
        QCOMPARE(m_view->viewMode(), FrequencyMapView::SpectrumView);
        QCOMPARE(modes.count(), 1);
        QVERIFY(!m_view->brightnessButton()->isHidden());
        m_view->brightnessButton()->click();
        QVERIFY(!m_view->brightnessSlider()->isHidden());
        m_view->brightnessSlider()->setValue(-2);
        QCOMPARE(m_view->brightness(), -2);
        m_view->setBrightness(9);
        QCOMPARE(m_view->brightness(), FrequencyMapView::highestBrightness);
        m_view->setColorScheme(4);
        QCOMPARE(m_view->colorScheme(), 4);
        m_view->setColorScheme(-1);
        QCOMPARE(m_view->colorScheme(), 0);
        QCOMPARE(spectrum.count(), 4);
        m_view->setViewMode(FrequencyMapView::KeyRowsView);
        QVERIFY(!m_view->viewButton()->isChecked());
        QVERIFY(m_view->brightnessSlider()->isHidden());
    }

    // A drag of the key line in the labels sets the key as one step, and a double click in the
    // labels flattens the frames and the key.
    void the_labels_set_the_key() {
        auto map = flatMap();
        map.averageFrequency = 200;
        FrequencyMapRevisionBuffer buffer(map);
        m_view->setBuffer(&buffer);
        m_view->setZoomLevel(4);
        const int x = m_view->labelWidth() / 2;
        const auto at = [this, x](double frequency) {
            return QPoint(x, int(std::lround(m_view->yOf(frequency))));
        };
        QSignalSpy edited(m_view.get(), &FrequencyMapView::edited);
        drag(at(200), at(300), Qt::LeftButton);
        QCOMPARE(edited.count(), 1);
        QVERIFY2(near(buffer.key(), 300, 300 * 0.02), qPrintable(QString::number(buffer.key())));
        QVERIFY(buffer.undo());
        QCOMPARE(buffer.key(), 200.0);

        QTest::mouseDClick(m_view->viewport(), Qt::LeftButton, {}, at(250));
        QCOMPARE(edited.count(), 2);
        QVERIFY(near(buffer.map().frames[50].frequency, 250, 250 * 0.02));
        QCOMPARE(buffer.key(), buffer.map().frames[50].frequency);
        m_view->setBuffer(m_buffer.get());
    }

    // A left drag with the pen draws the frames under the pointer as one step, and selects
    // nothing.
    void a_left_drag_draws_one_step() {
        QSignalSpy edited(m_view.get(), &FrequencyMapView::edited);
        drag(pointOf(10, 400), pointOf(20, 400), Qt::LeftButton);
        QCOMPARE(edited.count(), 1);
        QCOMPARE(m_view->selection(), std::nullopt);
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

    // A left drag with the select tool selects the frames between the press and the pointer, and
    // edits nothing. The pen is the default tool.
    void a_drag_with_the_select_tool_selects_frames() {
        QCOMPARE(m_view->tool(), FrequencyMapView::PenTool);
        m_view->setTool(FrequencyMapView::SelectTool);
        QSignalSpy changed(m_view.get(), &FrequencyMapView::selectionChanged);
        QSignalSpy edited(m_view.get(), &FrequencyMapView::edited);
        drag(pointOf(40, 200), pointOf(30, 300), Qt::LeftButton);
        QVERIFY(changed.count() > 0);
        QCOMPARE(edited.count(), 0);
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

    // The title bar and the time ruler lie above the viewport across its width, the ruler right
    // above the viewport. The labeled marks of the ruler are 1, 2 or 5 times a power of ten
    // milliseconds apart, the least interval that keeps them apart.
    void the_time_ruler_marks_milliseconds() {
        const auto ruler = m_view->timeRuler();
        const auto bar = m_view->titleBar();
        QVERIFY(ruler->isVisible());
        QVERIFY(bar->isVisible());
        QVERIFY(ruler->height() > 0);
        QCOMPARE(ruler->geometry().bottom() + 1, m_view->viewport()->geometry().top());
        QCOMPARE(bar->geometry().bottom() + 1, ruler->geometry().top());
        QCOMPARE(ruler->width(), m_view->viewport()->width());
        QCOMPARE(bar->width(), m_view->viewport()->width());
        m_view->setTitle(QStringLiteral("a.wav"));
        QVERIFY(!ruler->grab().isNull());
        QVERIFY(!bar->grab().isNull());
        QCOMPARE(FrequencyMapView::rulerInterval(1, 50), 50.0);
        QCOMPARE(FrequencyMapView::rulerInterval(4, 50), 20.0);
        QCOMPARE(FrequencyMapView::rulerInterval(0.3, 50), 200.0);
        QCOMPARE(FrequencyMapView::rulerInterval(0.2, 50), 500.0);
        QCOMPARE(FrequencyMapView::rulerInterval(100, 50), 0.5);
    }

    // The controls of the view lie right of the labels, and stay there with another font.
    void the_controls_are_right_of_the_labels() {
        QVERIFY(m_view->viewButton()->geometry().left() >= m_view->labelWidth());
        auto font = m_view->font();
        font.setPointSizeF(font.pointSizeF() * 3);
        m_view->setFont(font);
        QVERIFY(m_view->viewButton()->geometry().left() >= m_view->labelWidth());
        QVERIFY(m_view->brightnessButton()->geometry().left() >
                m_view->viewButton()->geometry().left());
    }

    // The pen shows a cursor of its own over the plot, which stays visible over the rows, and the
    // select tool shows the arrow.
    void the_pen_has_a_cursor_of_its_own() {
        // The move is sent directly, because QTest::mouseMove moves the pointer of the system on
        // Windows, whose move may not reach the viewport.
        const auto point = pointOf(50, 200);
        moveTo(point, Qt::NoButton);
        QCOMPARE(m_view->viewport()->cursor().shape(), Qt::BitmapCursor);
        m_view->setTool(FrequencyMapView::SelectTool);
        // The move is sent elsewhere, since the hover is updated only when the hit changes.
        moveTo(QPoint(m_view->labelWidth() / 2, point.y()), Qt::NoButton);
        moveTo(point, Qt::NoButton);
        QCOMPARE(m_view->viewport()->cursor().shape(), Qt::ArrowCursor);
    }

    // The corrections stay on the view until another buffer is set, and a frame scrolled to lies
    // at the left edge of the plot.
    void corrections_and_scrolling() {
        std::vector<hello::kit::FrequencyMapCheck::Correction> corrections(100);
        corrections[10] = {hello::kit::FrequencyMapCheck::OutOfRange, 300};
        m_view->setCorrections(corrections);
        QCOMPARE(m_view->corrections().size(), size_t(100));
        QVERIFY(!m_view->grab().isNull());
        m_view->setZoomLevel(6);
        m_view->scrollToFrame(50);
        QVERIFY2(near(m_view->xOf(500), m_view->labelWidth(), 1),
                 qPrintable(QString::number(m_view->xOf(500))));
        m_view->setBuffer(m_buffer.get());
        QVERIFY(m_view->corrections().empty());
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
