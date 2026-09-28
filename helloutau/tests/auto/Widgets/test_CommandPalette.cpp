#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsDropShadowEffect>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>

#include <helloutau/Widgets/CommandPalette.h>

using namespace hello::daw;

class test_CommandPalette : public QObject {
    Q_OBJECT

private:
    static QList<CommandEntry> commands() {
        return {
            {QStringLiteral("save"),
             QStringLiteral("File: Save"),
             {},
             QKeySequence(QStringLiteral("Ctrl+S"))                                },
            {QStringLiteral("saveAs"), QStringLiteral("File: Save As..."),   {}, {}},
            {QStringLiteral("undo"),
             QStringLiteral("Edit: Undo"),
             {},
             QKeySequence(QStringLiteral("Ctrl+Z"))                                },
            {QStringLiteral("prefs"),  QStringLiteral("Tools: Settings..."), {}, {}},
        };
    }

    static QLineEdit *inputOf(CommandPalette &palette) {
        return palette.findChild<QLineEdit *>();
    }

private Q_SLOTS:
    void typing_filters_and_ranks_the_commands() {
        QMainWindow window;
        CommandPalette palette(&window);
        palette.setCommands(commands());
        palette.popup();

        QCOMPARE(palette.shownIds().size(), 4);
        palette.setQuery(QStringLiteral("sa"));
        QCOMPARE(palette.shownIds(),
                 (QStringList{QStringLiteral("save"), QStringLiteral("saveAs")}));
        QCOMPARE(palette.currentId(), QStringLiteral("save"));

        palette.setQuery(QStringLiteral("nothing like it"));
        QVERIFY(palette.shownIds().isEmpty());
        QVERIFY(palette.currentId().isEmpty());
    }

    // The shadow comes from the style sheet, and there is none without it.
    void the_style_sheet_gives_the_shadow() {
        QMainWindow window;
        CommandPalette palette(&window);
        QVERIFY(!palette.graphicsEffect());

        window.setStyleSheet(QStringLiteral(
            "hello--daw--CommandPalette { qproperty-shadow: qshadow(#40000000, 16px, 0 4px); }"));
        palette.ensurePolished();
        const auto effect = qobject_cast<QGraphicsDropShadowEffect *>(palette.graphicsEffect());
        QVERIFY(effect);
        QCOMPARE(effect->blurRadius(), 16.0);
        QCOMPARE(effect->offset(), QPointF(0, 4));
        QCOMPARE(effect->color(), QColor(0, 0, 0, 0x40));

        palette.setShadow({});
        QVERIFY(!palette.graphicsEffect());
    }

    void the_keys_choose_and_enter_activates() {
        QMainWindow window;
        window.show();
        CommandPalette palette(&window);
        palette.setCommands(commands());
        QSignalSpy spy(&palette, &CommandPalette::commandActivated);

        palette.popup();
        QVERIFY(palette.isVisible());
        palette.setQuery(QStringLiteral("sa"));
        QTest::keyClick(inputOf(palette), Qt::Key_Down);
        QCOMPARE(palette.currentId(), QStringLiteral("saveAs"));

        QTest::keyClick(inputOf(palette), Qt::Key_Return);
        QVERIFY(!palette.isVisible());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), QStringLiteral("saveAs"));
    }

    void escape_closes_without_choosing() {
        QMainWindow window;
        window.show();
        CommandPalette palette(&window);
        palette.setCommands(commands());
        QSignalSpy spy(&palette, &CommandPalette::commandActivated);

        palette.popup();
        palette.setQuery(QStringLiteral("undo"));
        QTest::keyClick(inputOf(palette), Qt::Key_Escape);
        QVERIFY(!palette.isVisible());
        QCOMPARE(spy.count(), 0);

        // Opening again starts from an empty query.
        palette.popup();
        QVERIFY(palette.query().isEmpty());
        QCOMPARE(palette.shownIds().size(), 4);
    }

    // While nothing is typed, the recently used commands come first, the latest first. Typing
    // ranks by the match alone.
    void recently_used_commands_come_first() {
        QMainWindow window;
        CommandPalette palette(&window);
        palette.setCommands(commands());
        palette.setRecentIds(
            {QStringLiteral("prefs"), QStringLiteral("undo"), QStringLiteral("gone")});
        palette.popup();

        QCOMPARE(palette.shownIds(),
                 (QStringList{QStringLiteral("prefs"), QStringLiteral("undo"),
                              QStringLiteral("save"), QStringLiteral("saveAs")}));
        palette.setQuery(QStringLiteral("sa"));
        QCOMPARE(palette.shownIds().first(), QStringLiteral("save"));
    }

    // Half the width of the window within 450 and 750 pixels, centered under the menu bar
    void the_palette_follows_the_size_of_its_window() {
        QMainWindow window;
        window.setCentralWidget(new QWidget());
        window.resize(1200, 800);
        window.show();
        CommandPalette palette(&window);
        palette.setCommands(commands());
        palette.popup();
        QCOMPARE(palette.width(), 600);
        QCOMPARE(palette.x(), 300);

        window.resize(2000, 800);
        QCOMPARE(palette.width(), 750);
        window.resize(600, 800);
        QCOMPARE(palette.width(), 450);
    }

    void a_click_outside_closes_it() {
        QMainWindow window;
        const auto content = new QWidget();
        window.setCentralWidget(content);
        window.resize(1200, 800);
        window.show();
        CommandPalette palette(&window);
        palette.setCommands(commands());
        QSignalSpy spy(&palette, &CommandPalette::commandActivated);

        palette.popup();
        QTest::mouseClick(content, Qt::LeftButton, {}, QPoint(5, content->height() - 5));
        QVERIFY(!palette.isVisible());
        QCOMPARE(spy.count(), 0);
    }

    void enter_with_nothing_shown_does_nothing() {
        QMainWindow window;
        window.show();
        CommandPalette palette(&window);
        palette.setCommands(commands());
        QSignalSpy spy(&palette, &CommandPalette::commandActivated);

        palette.popup();
        palette.setQuery(QStringLiteral("zzz"));
        QTest::keyClick(inputOf(palette), Qt::Key_Return);
        QVERIFY(palette.isVisible());
        QCOMPARE(spy.count(), 0);
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_CommandPalette test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_CommandPalette.moc"
