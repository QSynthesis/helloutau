#include <QtCore/QDir>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <helloutau/Theme/ThemeManager.h>

using namespace hello::daw;

namespace {

    // The description files beside the test: good holds two themes, dark extending light, over a
    // common part, in two files; bad holds a cycle, a missing variable and file, and no JSON.
    QString themes(const char *set) {
        return QDir(QString::fromUtf8(TEST_RESOURCE_DIRECTORY))
            .absoluteFilePath(QStringLiteral("themes/") + QString::fromLatin1(set));
    }

}

class test_ThemeManager : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void themes_are_found_in_the_description_files() {
        ThemeManager manager;
        manager.addSearchPath(themes("good"));
        QCOMPARE(manager.themes(), (QStringList{QStringLiteral("dark"), QStringLiteral("light")}));
    }

    // _common first; within a theme by priority; each file relative to its folder; variables of
    // the whole chain, the derived theme overriding its base
    void a_style_sheet_is_assembled_along_the_chain() {
        ThemeManager manager;
        manager.addSearchPath(themes("good"));

        manager.setCurrentTheme(QStringLiteral("light"));
        const auto folder = QDir(themes("good")).absoluteFilePath(QStringLiteral("base/light"));
        QCOMPARE(manager.styleSheet({QStringLiteral("MainWindow")}),
                 QStringLiteral("A { margin: 4px; }\n\n"
                                "W { color: #000000; image: url(%1/icon.png); }\n\n"
                                "R { background: #FFFFFF; }\n\n"
                                "R { color: #000000; }")
                     .arg(folder));

        manager.setCurrentTheme(QStringLiteral("dark"));
        QCOMPARE(manager.styleSheet({QStringLiteral("MainWindow")}),
                 QStringLiteral("A { margin: 4px; }\n\n"
                                "W { color: #EEEEEE; image: url(%1/icon.png); }\n\n"
                                "R { background: #101010; }\n\n"
                                "R { color: #EEEEEE; }\n\n"
                                "R { border: 2px; }")
                     .arg(folder));
        QCOMPARE(manager.styleSheet({QStringLiteral("Palette")}),
                 QStringLiteral("P { qproperty-shadow: qshadow(#40000000, 8px); }"));

        // The scale applies to lengths, with the ratio of each sheet.
        manager.setScale(2);
        const auto scaled = manager.styleSheet({QStringLiteral("MainWindow")});
        QVERIFY(scaled.contains(QStringLiteral("A { margin: 8px; }")));
        QVERIFY(scaled.contains(QStringLiteral("R { border: 4px; }")));
    }

    // An installed widget follows the theme, once per series of changes, until it is destroyed.
    void installed_widgets_follow_the_theme() {
        ThemeManager manager;
        manager.addSearchPath(themes("good"));
        QSignalSpy spy(&manager, &ThemeManager::currentThemeChanged);

        auto widget = std::make_unique<QWidget>();
        manager.install(widget.get(), {QStringLiteral("Palette")});
        QVERIFY(widget->styleSheet().isEmpty());

        manager.setCurrentTheme(QStringLiteral("dark"));
        QCOMPARE(spy.count(), 1);
        QVERIFY(widget->styleSheet().isEmpty());
        QTRY_VERIFY(widget->styleSheet().contains(QStringLiteral("qshadow")));

        // A destroyed widget is forgotten before the next refresh.
        manager.setCurrentTheme(QStringLiteral("light"));
        widget.reset();
        QTest::qWait(10);
    }

    void problems_are_reported() {
        ThemeManager manager;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("absent\\.qss")));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("not a JSON object")));
        manager.addSearchPath(themes("bad"));

        manager.setCurrentTheme(QStringLiteral("a"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("cycle")));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("missing")));
        // An unknown variable is left as written.
        QCOMPARE(manager.styleSheet({QStringLiteral("x")}),
                 QStringLiteral("X { color: ${missing}; }"));
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_ThemeManager test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_ThemeManager.moc"
