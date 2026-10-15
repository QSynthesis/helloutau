#include <cmath>
#include <memory>

#include <QtCore/QStringList>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <helloutau/Widgets/SceneGestures.h>
#include <helloutau/Widgets/SceneView.h>

using namespace hello::daw;

namespace {

    // A gesture that records the events it receives.
    class RecordingGesture : public SceneGesture {
    public:
        explicit RecordingGesture(QStringList &log, bool autoScroll = false)
            : m_log(log), m_autoScroll(autoScroll) {
        }

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override {
            m_log.push_back(QStringLiteral("move %1 %2").arg(position.x()).arg(int(modifiers)));
        }

        void release(QPointF position, Qt::KeyboardModifiers modifiers) override {
            m_log.push_back(QStringLiteral("release %1 %2").arg(position.x()).arg(int(modifiers)));
        }

        void cancel() override {
            m_log.push_back(QStringLiteral("cancel"));
        }

        bool hasAutoScroll() const override {
            return m_autoScroll;
        }

    private:
        QStringList &m_log;
        bool m_autoScroll;
    };

    constexpr int ctrl = int(Qt::ControlModifier);
    constexpr int shift = int(Qt::ShiftModifier);

}

class test_SceneGestures : public QObject {
    Q_OBJECT

private:
    QStringList m_log;

    PressGesture::DragFunction drag(bool autoScroll = false) {
        return [this, autoScroll](Qt::KeyboardModifiers modifiers) {
            m_log.push_back(QStringLiteral("drag %1").arg(int(modifiers)));
            return std::make_unique<RecordingGesture>(m_log, autoScroll);
        };
    }

    PressGesture::ClickFunction click() {
        return [this](Qt::KeyboardModifiers modifiers) {
            m_log.push_back(QStringLiteral("click %1").arg(int(modifiers)));
        };
    }

    // A distance below the start distance of a drag
    static double near() {
        return QApplication::startDragDistance() - 1;
    }

    static double far() {
        return QApplication::startDragDistance() + 1;
    }

private Q_SLOTS:
    void init() {
        m_log.clear();
    }

    // A release within the start distance is a click, with the modifiers held at the release.
    void a_release_near_the_press_is_a_click() {
        PressGesture press(QPointF(0, 0), drag(), click());
        press.move(QPointF(near(), 0), Qt::ControlModifier);
        press.release(QPointF(near(), 0), Qt::ShiftModifier);
        QCOMPARE(m_log, QStringList{QStringLiteral("click %1").arg(shift)});
    }

    // The first move past the start distance starts the drag with the modifiers held then, and
    // the gesture of the drag receives that move, the later moves and the release.
    void a_move_past_the_start_distance_starts_the_drag() {
        PressGesture press(QPointF(0, 0), drag(true), click());
        QVERIFY(!press.hasAutoScroll());
        press.move(QPointF(far(), 0), Qt::ControlModifier);
        press.move(QPointF(40, 0), Qt::ShiftModifier);
        press.release(QPointF(50, 0), Qt::NoModifier);
        QCOMPARE(m_log, (QStringList{QStringLiteral("drag %1").arg(ctrl),
                                     QStringLiteral("move %1 %2").arg(far()).arg(ctrl),
                                     QStringLiteral("move 40 %1").arg(shift),
                                     QStringLiteral("release 50 0")}));
        QVERIFY(press.hasAutoScroll());
    }

    void a_drag_without_a_gesture_does_nothing() {
        PressGesture press(
            QPointF(0, 0),
            [this](Qt::KeyboardModifiers) {
                m_log.push_back(QStringLiteral("drag"));
                return std::unique_ptr<SceneGesture>();
            },
            click());
        press.move(QPointF(far(), 0), Qt::NoModifier);
        press.release(QPointF(far(), 0), Qt::NoModifier);
        press.cancel();
        QCOMPARE(m_log, QStringList{QStringLiteral("drag")});
    }

    // Without a click function, the first move starts the drag, and a release before any move
    // ends a drag that does not move.
    void without_a_click_any_move_or_release_is_a_drag() {
        {
            PressGesture press(QPointF(0, 0), drag(), {});
            press.move(QPointF(1, 0), Qt::NoModifier);
        }
        QCOMPARE(m_log, (QStringList{QStringLiteral("drag 0"), QStringLiteral("move 1 0")}));

        m_log.clear();
        PressGesture press(QPointF(0, 0), drag(), {});
        press.release(QPointF(0, 0), Qt::ControlModifier);
        QCOMPARE(m_log, (QStringList{QStringLiteral("drag %1").arg(ctrl),
                                     QStringLiteral("release 0 %1").arg(ctrl)}));
    }

    void a_cancel_reaches_the_gesture_of_the_drag() {
        PressGesture press(QPointF(0, 0), drag(), click());
        press.cancel();
        QVERIFY(m_log.isEmpty());
        press.move(QPointF(far(), 0), Qt::NoModifier);
        press.cancel();
        QCOMPARE(m_log.last(), QStringLiteral("cancel"));
    }

    // A zoom drag zooms both axes about its origin, and only the axis of the first move while
    // locked.
    void a_zoom_drag_keeps_its_origin() {
        SceneView view;
        view.resize(800, 600);
        view.setTickRange(0, 100000);
        view.setTimeScaleRange(0.001, 100);
        view.setKeyScaleRange(1, 100);
        view.setTimeAxis({0.1, 1000});
        view.setKeyAxis({20, 100});
        const QPointF origin(200, 100);
        const double tick = view.timeAxis().toTick(origin.x());
        const double key = view.keyAxis().toKey(origin.y());

        bool locked = false;
        SceneZoomGesture zoom(&view, origin, [&locked](Qt::KeyboardModifiers) { return locked; });
        zoom.move(origin + QPointF(10, 5), Qt::NoModifier);
        QVERIFY(qAbs(view.timeAxis().pixelsPerTick - 0.1 * std::pow(1.01, 10)) < 1e-9);
        QVERIFY(qAbs(view.keyAxis().pixelsPerKey - 20 * std::pow(1.01, 5)) < 1e-9);
        QVERIFY(qAbs(view.timeAxis().toTick(origin.x()) - tick) < 1e-6);
        QVERIFY(qAbs(view.keyAxis().toKey(origin.y()) - key) < 1e-6);

        // Locked, the first move past the start distance chooses the horizontal axis.
        locked = true;
        const double keyScale = view.keyAxis().pixelsPerKey;
        zoom.move(origin + QPointF(10 + far(), 6), Qt::NoModifier);
        zoom.release(origin + QPointF(30 + far(), 30), Qt::NoModifier);
        QCOMPARE(view.keyAxis().pixelsPerKey, keyScale);
        QVERIFY(qAbs(view.timeAxis().pixelsPerTick - 0.1 * std::pow(1.01, 30 + far())) < 1e-9);
    }
};

QTEST_MAIN(test_SceneGestures)

#include "test_SceneGestures.moc"
