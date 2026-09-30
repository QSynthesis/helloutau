#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include <helloutau/Widgets/FindBar.h>

using namespace hello::daw;

class test_FindBar : public QObject {
    Q_OBJECT

private:
    // A window with a widget that the bar covers, below another widget
    struct Window {
        QWidget window;
        QWidget *top;
        QWidget *area;
        FindBar *bar;

        Window() {
            top = new QWidget();
            top->setFixedHeight(40);
            area = new QWidget();
            area->setFocusPolicy(Qt::StrongFocus);
            auto layout = new QVBoxLayout(&window);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(0);
            layout->addWidget(top);
            layout->addWidget(area);
            bar = new FindBar(&window);
            bar->setAnchor(area);
            window.resize(900, 500);
            window.show();
        }
    };

private Q_SLOTS:
    void the_bar_covers_the_top_right_corner_of_its_anchor() {
        Window w;
        QVERIFY(QTest::qWaitForWindowExposed(&w.window));
        QVERIFY(w.bar->isHidden());
        w.bar->showFind();
        QVERIFY(w.bar->isVisible());
        QCOMPARE(w.bar->y(), w.area->y());
        QVERIFY(w.bar->geometry().right() < w.area->geometry().right());
        QVERIFY(w.bar->geometry().right() > w.area->geometry().right() - 40);

        // The bar follows the anchor.
        w.window.resize(1000, 500);
        QTRY_VERIFY(w.bar->geometry().right() > w.area->geometry().right() - 40);
    }

    void return_requests_the_next_match_and_shift_the_previous() {
        Window w;
        w.bar->showFind();
        QSignalSpy next(w.bar, &FindBar::findNextRequested);
        QSignalSpy previous(w.bar, &FindBar::findPreviousRequested);
        QTest::keyClick(w.bar->findField(), Qt::Key_Return);
        QTest::keyClick(w.bar->findField(), Qt::Key_Return, Qt::ShiftModifier);
        QCOMPARE(next.size(), 1);
        QCOMPARE(previous.size(), 1);
    }

    void return_in_the_replace_field_requests_a_replacement() {
        Window w;
        w.bar->showReplace();
        QVERIFY(w.bar->isReplaceShown());
        QSignalSpy one(w.bar, &FindBar::replaceRequested);
        QSignalSpy all(w.bar, &FindBar::replaceAllRequested);
        QTest::keyClick(w.bar->replaceField(), Qt::Key_Return);
        QTest::keyClick(w.bar->replaceField(), Qt::Key_Return,
                        Qt::ControlModifier | Qt::AltModifier);
        QCOMPARE(one.size(), 1);
        QCOMPARE(all.size(), 1);

        // Nothing is replaced in a scope that cannot be replaced.
        w.bar->setReplaceEnabled(false);
        QTest::keyClick(w.bar->findField(), Qt::Key_Return, Qt::ControlModifier | Qt::AltModifier);
        QCOMPARE(all.size(), 1);
        QVERIFY(!w.bar->findChild<QToolButton *>(QStringLiteral("replaceAll"))->isEnabled());
    }

    // The keys of the options are shortcuts of the bar, which require the focus in the bar.
    void the_options_change_the_query() {
        Window w;
        w.window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&w.window));
        w.bar->showFind();
        QTRY_VERIFY(w.bar->findField()->hasFocus());
        QSignalSpy changed(w.bar, &FindBar::queryChanged);
        QTest::keyClicks(w.bar->findField(), QStringLiteral("a"));
        w.bar->findChild<QToolButton *>(QStringLiteral("matchCase"))->click();
        QTest::keyClick(w.bar->findField(), Qt::Key_W, Qt::AltModifier);
        QTest::keyClick(w.bar->findField(), Qt::Key_R, Qt::AltModifier);
        QCOMPARE(changed.size(), 4);
        QVERIFY(w.bar->isCaseSensitive());
        QVERIFY(w.bar->isWholeWord());
        QVERIFY(w.bar->isRegularExpression());
    }

    void escape_hides_the_bar_and_returns_the_focus() {
        Window w;
        QVERIFY(QTest::qWaitForWindowExposed(&w.window));
        w.window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&w.window));
        w.bar->showFind();
        QTRY_VERIFY(w.bar->findField()->hasFocus());
        QSignalSpy closed(w.bar, &FindBar::closed);
        QTest::keyClick(w.bar->findField(), Qt::Key_Escape);
        QVERIFY(w.bar->isHidden());
        QCOMPARE(closed.size(), 1);
        QTRY_VERIFY(w.area->hasFocus());
    }

    void the_result_is_counted() {
        Window w;
        w.bar->showFind();
        const auto previous = w.bar->findChild<QToolButton *>(QStringLiteral("previous"));
        w.bar->setResult(0, 0);
        QCOMPARE(w.bar->resultText(), QString());
        w.bar->setText(QStringLiteral("a"));
        w.bar->setResult(0, 0);
        QCOMPARE(w.bar->resultText(), QStringLiteral("No results"));
        QVERIFY(!previous->isEnabled());
        w.bar->setResult(2, 5);
        QCOMPARE(w.bar->resultText(), QStringLiteral("2 of 5"));
        QVERIFY(previous->isEnabled());
        w.bar->setResult(0, 5);
        QCOMPARE(w.bar->resultText(), QStringLiteral("? of 5"));
        w.bar->setError(QStringLiteral("missing )"));
        QVERIFY(!previous->isEnabled());
        QCOMPARE(w.bar->findField()->toolTip(), QStringLiteral("missing )"));
    }

    void the_scopes_are_offered_if_there_are_several() {
        Window w;
        w.bar->showFind();
        QSignalSpy changed(w.bar, &FindBar::queryChanged);
        w.bar->setScopes({QStringLiteral("Aliases"), QStringLiteral("File Names")});
        QCOMPARE(changed.size(), 0);
        QCOMPARE(w.bar->scope(), 0);
        w.bar->setScope(1);
        QCOMPARE(w.bar->scope(), 1);
        QCOMPARE(changed.size(), 1);
    }
};

QTEST_MAIN(test_FindBar)

#include "test_FindBar.moc"
