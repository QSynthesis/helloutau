#include <QtGui/QGuiApplication>
#include <QtGui/QWheelEvent>
#include <QtTest/QTest>

#include <helloutau/Widgets/PlatformWheel.h>

using namespace hello::daw;

namespace {

    QWheelEvent wheel(QPoint delta, Qt::KeyboardModifiers modifiers) {
        return QWheelEvent(QPointF(), QPointF(), QPoint(), delta, Qt::NoButton, modifiers,
                           Qt::NoScrollPhase, false);
    }

}

class test_PlatformWheel : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The plugins windows and xcb transpose the delta while Alt is held, and the transposition
    // is undone. Elsewhere, and without Alt, the delta is unchanged.
    void the_transposition_of_alt_is_undone() {
        const auto platform = QGuiApplication::platformName();
        const bool transposes =
            platform == QStringLiteral("windows") || platform == QStringLiteral("xcb");
        QCOMPARE(PlatformWheel::angleDelta(wheel(QPoint(0, 120), Qt::AltModifier)),
                 transposes ? QPoint(120, 0) : QPoint(0, 120));
        QCOMPARE(PlatformWheel::angleDelta(wheel(QPoint(0, 120), Qt::ControlModifier)),
                 QPoint(0, 120));
        QCOMPARE(PlatformWheel::angleDelta(wheel(QPoint(120, 0), Qt::NoModifier)), QPoint(120, 0));
    }
};

QTEST_MAIN(test_PlatformWheel)

#include "test_PlatformWheel.moc"
