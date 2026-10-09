#include <QtGui/QWheelEvent>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QScrollBar>

#include <helloutau/Widgets/PianoKeyboard.h>
#include <helloutau/Widgets/SceneView.h>
#include <helloutau/Widgets/TimelineRuler.h>

using namespace hello::daw;

namespace {

    // Records what a gesture receives, in order.
    QStringList g_log;

    class RecordingGesture : public SceneGesture {
    public:
        void move(QPointF position, Qt::KeyboardModifiers) override {
            g_log.push_back(QStringLiteral("move %1").arg(position.x()));
        }
        void release(QPointF position, Qt::KeyboardModifiers) override {
            g_log.push_back(QStringLiteral("release %1").arg(position.x()));
        }
        void cancel() override {
            g_log.push_back(QStringLiteral("cancel"));
        }
    };

    // Reports a hit on a rectangle of the view and starts a recording gesture on a press.
    class RectLayer : public SceneLayer {
    public:
        RectLayer(QRectF area, quint64 node) : m_area(area), m_node(node) {
        }

        void paint(QPainter &, const QRect &) override {
        }

        std::optional<SceneHit> hitTest(QPointF position) const override {
            if (!m_area.contains(position)) {
                return std::nullopt;
            }
            SceneHit hit;
            hit.node = m_node;
            hit.cursor = Qt::SizeHorCursor;
            return hit;
        }

        std::unique_ptr<SceneGesture> press(const SceneHit &, QPointF position, Qt::MouseButton,
                                            Qt::KeyboardModifiers) override {
            g_log.push_back(QStringLiteral("press %1").arg(position.x()));
            return std::make_unique<RecordingGesture>();
        }

        bool doubleClick(const SceneHit &, QPointF position) override {
            if (!respondsToDoubleClick) {
                return false;
            }
            g_log.push_back(QStringLiteral("double %1").arg(position.x()));
            return true;
        }

        bool respondsToDoubleClick = false;

    private:
        QRectF m_area;
        quint64 m_node;
    };

    void doubleClick(SceneView &view, QPointF at) {
        QMouseEvent event(QEvent::MouseButtonDblClick, at, view.viewport()->mapToGlobal(at),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(view.viewport(), &event);
    }

    void wheel(SceneView &view, int notches, Qt::KeyboardModifiers modifiers, QPointF at,
               int horizontalNotches = 0) {
        QWheelEvent event(at, view.viewport()->mapToGlobal(at), QPoint(),
                          QPoint(horizontalNotches * 120, notches * 120),
                          Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
        QApplication::sendEvent(view.viewport(), &event);
    }

}

class test_SceneView : public QObject {
    Q_OBJECT

private:
    // A shown view, so that its viewport has a size
    static std::unique_ptr<SceneView> shownView() {
        auto view = std::make_unique<SceneView>();
        view->resize(600, 400);
        view->show();
        view->setTickRange(0, 480 * 4 * 16);
        view->setKeyRange(0, 127);
        return view;
    }

private Q_SLOTS:
    void init() {
        g_log.clear();
    }

    void the_axes_stay_within_the_scene() {
        const auto view = shownView();
        const double width = view->viewport()->width();

        view->setTimeAxis({0.125, 1e9});
        QCOMPARE(view->timeAxis().left, 480 * 4 * 16 - width / 0.125);
        view->setTimeAxis({0.125, -100});
        QCOMPARE(view->timeAxis().left, 0.0);

        view->setKeyAxis({24, 1000});
        QCOMPARE(view->keyAxis().top, 128.0);
    }

    void the_scroll_bars_follow_the_axes() {
        const auto view = shownView();
        view->setTimeAxis({0.125, 960});
        QCOMPARE(view->horizontalScrollBar()->value(), 120);
        view->setKeyAxis({24, 100});
        QCOMPARE(view->verticalScrollBar()->value(), (128 - 100) * 24);

        view->horizontalScrollBar()->setValue(240);
        QCOMPARE(view->timeAxis().left, 1920.0);
    }

    void zooming_keeps_the_position_under_the_pointer() {
        const auto view = shownView();
        view->setTimeAxis({0.125, 960});
        const double tick = view->timeAxis().toTick(100);
        view->zoomTime(2, 100);
        QCOMPARE(view->timeAxis().pixelsPerTick, 0.25);
        QCOMPARE(view->timeAxis().toTick(100), tick);

        view->setKeyAxis({24, 90});
        const double key = view->keyAxis().toKey(50);
        view->zoomKeys(1.5, 50);
        QCOMPARE(view->keyAxis().pixelsPerKey, 36.0);
        QCOMPARE(view->keyAxis().toKey(50), key);

        view->setTimeScaleRange(0.1, 0.3);
        view->zoomTime(100, 0);
        QCOMPARE(view->timeAxis().pixelsPerTick, 0.3);
    }

    void the_wheel_scrolls_and_zooms_by_its_modifiers() {
        const auto view = shownView();
        view->setTimeAxis({0.125, 960});
        view->setKeyAxis({24, 90});

        wheel(*view, 1, Qt::NoModifier, {10, 10});
        QCOMPARE(view->keyAxis().top, 93.0);

        wheel(*view, -1, Qt::ShiftModifier, {10, 10});
        QVERIFY(view->timeAxis().left > 960);

        wheel(*view, 1, Qt::ControlModifier, {10, 10});
        QCOMPARE(view->timeAxis().pixelsPerTick, 0.125 * 1.25);

        wheel(*view, 1, Qt::ControlModifier | Qt::ShiftModifier, {10, 10});
        QCOMPARE(view->keyAxis().pixelsPerKey, 24 * 1.25);
    }

    void the_wheel_uses_configured_modifiers() {
        const auto view = shownView();
        view->setTimeAxis({0.125, 960});
        view->setKeyAxis({24, 90});
        view->setWheelActions([](Qt::KeyboardModifiers modifiers) {
            if (modifiers == (Qt::AltModifier | Qt::MetaModifier)) {
                return SceneView::KeyZoom;
            }
            if (modifiers == Qt::MetaModifier) {
                return SceneView::TimeZoom;
            }
            if (modifiers == Qt::AltModifier) {
                return SceneView::HorizontalScroll;
            }
            return SceneView::VerticalScroll;
        });

        wheel(*view, -1, Qt::AltModifier, {10, 10});
        QVERIFY(view->timeAxis().left > 960);
        wheel(*view, 1, Qt::MetaModifier, {10, 10});
        QCOMPARE(view->timeAxis().pixelsPerTick, 0.125 * 1.25);
        wheel(*view, 1, Qt::AltModifier | Qt::MetaModifier, {10, 10});
        QCOMPARE(view->keyAxis().pixelsPerKey, 24 * 1.25);
    }

    void alt_does_not_scroll_unless_configured() {
        const auto view = shownView();
        view->setTimeAxis({0.125, 960});
        view->setKeyAxis({24, 90});

        wheel(*view, 1, Qt::AltModifier, {10, 10}, 1);
        QCOMPARE(view->timeAxis().left, 960.0);
        QCOMPARE(view->keyAxis().top, 90.0);
    }

    // The layer added last is drawn on top and hit first.
    void hits_come_from_the_top_layer() {
        const auto view = shownView();
        view->addLayer(std::make_unique<RectLayer>(QRectF(0, 0, 100, 100), 1));
        view->addLayer(std::make_unique<RectLayer>(QRectF(50, 50, 100, 100), 2));

        QCOMPARE(view->hitAt({75, 75})->node, quint64(2));
        QCOMPARE(view->hitAt({75, 75})->layer, 1);
        QCOMPARE(view->hitAt({25, 25})->node, quint64(1));
        QVERIFY(!view->hitAt({300, 300}));
    }

    void hovering_shows_the_cursor_of_the_hit() {
        const auto view = shownView();
        view->addLayer(std::make_unique<RectLayer>(QRectF(0, 0, 100, 100), 1));

        QTest::mouseMove(view->viewport(), QPoint(20, 20));
        QVERIFY(view->hoveredHit());
        QCOMPARE(view->viewport()->cursor().shape(), Qt::SizeHorCursor);

        QTest::mouseMove(view->viewport(), QPoint(300, 300));
        QVERIFY(!view->hoveredHit());
        QCOMPARE(view->viewport()->cursor().shape(), Qt::ArrowCursor);
    }

    void a_gesture_runs_from_press_to_release() {
        const auto view = shownView();
        view->addLayer(std::make_unique<RectLayer>(QRectF(0, 0, 100, 100), 1));

        QTest::mousePress(view->viewport(), Qt::LeftButton, {}, QPoint(20, 20));
        QVERIFY(view->hasGesture());
        QTest::mouseMove(view->viewport(), QPoint(150, 20));
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, {}, QPoint(160, 20));
        QVERIFY(!view->hasGesture());
        QCOMPARE(g_log, (QStringList{QStringLiteral("press 20"), QStringLiteral("move 150"),
                                     QStringLiteral("release 160")}));

        // A press on nothing starts nothing.
        g_log.clear();
        QTest::mousePress(view->viewport(), Qt::LeftButton, {}, QPoint(300, 300));
        QVERIFY(!view->hasGesture());
        QVERIFY(g_log.isEmpty());
    }

    // A layer that responds to a double click receives it instead of a press; otherwise the
    // second click is a press as the first was.
    void a_double_click_goes_to_the_layer_hit() {
        const auto view = shownView();
        const auto layer = static_cast<RectLayer *>(
            view->addLayer(std::make_unique<RectLayer>(QRectF(0, 0, 100, 100), 1)));

        doubleClick(*view, QPointF(20, 20));
        QCOMPARE(g_log, QStringList{QStringLiteral("press 20")});
        QVERIFY(view->hasGesture());
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, {}, QPoint(20, 20));

        g_log.clear();
        layer->respondsToDoubleClick = true;
        doubleClick(*view, QPointF(30, 20));
        QCOMPARE(g_log, QStringList{QStringLiteral("double 30")});
        QVERIFY(!view->hasGesture());
    }

    void escape_cancels_the_gesture() {
        const auto view = shownView();
        view->addLayer(std::make_unique<RectLayer>(QRectF(0, 0, 100, 100), 1));

        QTest::mousePress(view->viewport(), Qt::LeftButton, {}, QPoint(20, 20));
        QTest::keyClick(view.get(), Qt::Key_Escape);
        QVERIFY(!view->hasGesture());
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, {}, QPoint(30, 20));
        QCOMPARE(g_log, (QStringList{QStringLiteral("press 20"), QStringLiteral("cancel")}));
    }

    void keys_are_named_as_utau_names_them() {
        QCOMPARE(PianoKeyboard::keyName(60), QStringLiteral("C4"));
        QCOMPARE(PianoKeyboard::keyName(69), QStringLiteral("A4"));
        QCOMPARE(PianoKeyboard::keyName(61), QStringLiteral("C#4"));
        QCOMPARE(PianoKeyboard::keyName(0), QStringLiteral("C-1"));
        QVERIFY(PianoKeyboard::isBlackKey(61));
        QVERIFY(!PianoKeyboard::isBlackKey(64));
    }

    void bar_numbers_are_spaced_to_stay_legible() {
        QCOMPARE(TimelineRuler::labelInterval(100, 20), 1);
        QCOMPARE(TimelineRuler::labelInterval(10, 20), 4);
        QCOMPARE(TimelineRuler::labelInterval(3, 20), 16);
    }

    // The ruler and the keyboard draw from the axes of their view without failing.
    void the_ruler_and_the_keyboard_draw_along_the_view() {
        const auto view = shownView();
        TimelineRuler ruler(view.get());
        ruler.setMarks({
            {960, QStringLiteral("120")}
        });
        PianoKeyboard keyboard(view.get());
        ruler.resize(600, ruler.sizeHint().height());
        keyboard.resize(keyboard.sizeHint().width(), 400);
        view->zoomTime(4, 0);
        QVERIFY(!ruler.grab().isNull());
        QVERIFY(!keyboard.grab().isNull());
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_SceneView test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_SceneView.moc"
