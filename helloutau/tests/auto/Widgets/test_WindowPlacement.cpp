#include <memory>

#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <helloutau/Widgets/WindowPlacement.h>

using namespace hello::daw;

namespace {

    constexpr QSize windowSize(200, 150);

    QRect available() {
        return QGuiApplication::primaryScreen()->availableGeometry();
    }

    // A shown window at the offset from the available area
    std::unique_ptr<QWidget> shownWindow(const QPoint &offset) {
        auto window = std::make_unique<QWidget>();
        window->resize(windowSize);
        window->move(available().topLeft() + offset);
        window->show();
        return window;
    }

    std::unique_ptr<QWidget> newWindow() {
        auto window = std::make_unique<QWidget>();
        window->resize(windowSize);
        return window;
    }

}

class test_WindowPlacement : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void a_window_without_others_is_centered() {
        const auto window = newWindow();
        WindowPlacement::place(window.get(), {}, available().center());
        QCOMPARE(window->pos(), available().center() - QPoint(100, 75));
    }

    // The window under the pointer is the anchor even if it is not the last window.
    void a_window_is_cascaded_from_the_window_under_the_pointer() {
        const auto first = shownWindow({10, 10});
        const auto second = shownWindow({300, 200});
        QVERIFY(QTest::qWaitForWindowExposed(first.get()));
        QVERIFY(QTest::qWaitForWindowExposed(second.get()));
        QCOMPARE(QApplication::topLevelAt(first->frameGeometry().center()), first.get());
        const auto window = newWindow();
        WindowPlacement::place(window.get(), {first.get(), second.get()},
                               first->frameGeometry().center());
        const QPoint step(WindowPlacement::cascadeStep, WindowPlacement::cascadeStep);
        QCOMPARE(window->pos(), first->frameGeometry().topLeft() + step);
    }

    // Without a window under the pointer, the last window on the screen of the pointer is the
    // anchor.
    void the_last_window_on_the_screen_is_the_anchor() {
        const auto first = shownWindow({10, 10});
        const auto second = shownWindow({300, 200});
        QVERIFY(QTest::qWaitForWindowExposed(second.get()));
        const auto window = newWindow();
        WindowPlacement::place(window.get(), {first.get(), second.get()},
                               available().bottomRight() - QPoint(5, 5));
        const QPoint step(WindowPlacement::cascadeStep, WindowPlacement::cascadeStep);
        QCOMPARE(window->pos(), second->frameGeometry().topLeft() + step);
    }

    // A window cascaded beyond the available area is kept within it.
    void a_cascaded_window_stays_within_the_screen() {
        const auto corner = available().bottomRight() - available().topLeft() -
                            QPoint(windowSize.width(), windowSize.height()) + QPoint(1, 1);
        const auto last = shownWindow(corner);
        QVERIFY(QTest::qWaitForWindowExposed(last.get()));
        const auto window = newWindow();
        WindowPlacement::place(window.get(), {last.get()}, last->frameGeometry().center());
        QCOMPARE(window->pos(), last->frameGeometry().topLeft());
    }

    // A hidden window, a destroyed window and the window itself are no anchors.
    void only_other_visible_windows_are_anchors() {
        const auto hidden = shownWindow({10, 10});
        hidden->hide();
        QPointer<QWidget> destroyed = new QWidget();
        delete destroyed.data();
        const auto window = newWindow();
        WindowPlacement::place(window.get(), {hidden.get(), destroyed, window.get()},
                               hidden->frameGeometry().center());
        QCOMPARE(window->pos(), available().center() - QPoint(100, 75));
    }
};

QTEST_MAIN(test_WindowPlacement)

#include "test_WindowPlacement.moc"
