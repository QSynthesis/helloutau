#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtGui/QIcon>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>

#include <helloutau/Theme/ThemeManager.h>

using namespace hello::daw;

namespace {

    // The description files in the test resources. The set good defines two themes, dark
    // extending light, above a common part, in two files. The set bad contains a cycle, a missing
    // variable, a missing file and a file that is not JSON. The set icons assigns a button an
    // icon in a different color per theme.
    QString themes(const char *set) {
        return QDir(QString::fromUtf8(TEST_RESOURCE_DIRECTORY))
            .absoluteFilePath(QStringLiteral("themes/") + QString::fromLatin1(set));
    }

    // The color at the center of a 16-pixel icon
    QColor colorOf(const QIcon &icon) {
        return icon.pixmap(QSize(16, 16), 1.0).toImage().pixelColor(8, 8);
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

    // The sheets of _common come first, and the sheets within a theme are ordered by priority.
    // Each file is resolved against its own directory. The variables come from the whole chain,
    // and the derived theme overrides its base.
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

    // An installed widget receives the style sheet of the current theme, once per series of
    // changes, until it is destroyed.
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

        // A destroyed widget is removed before the next refresh.
        manager.setCurrentTheme(QStringLiteral("light"));
        widget.reset();
        QTest::qWait(10);
    }

    // A plain button whose icon is assigned by the style sheet reflects the current theme, and
    // the current icon files after a reload, without code specific to themes.
    void installed_icons_follow_the_theme() {
        QTemporaryDir dir;
        for (const auto name : {"icons.res.json", "square.svg"}) {
            QVERIFY(QFile::copy(QDir(themes("icons")).filePath(QString::fromLatin1(name)),
                                dir.filePath(QString::fromLatin1(name))));
        }
        ThemeManager manager;
        manager.addSearchPath(dir.path());
        manager.setCurrentTheme(QStringLiteral("light"));

        QToolButton button;
        manager.install(&button, {QStringLiteral("Button")});
        button.ensurePolished();
        QCOMPARE(colorOf(button.icon()), QColor(255, 0, 0));

        manager.setCurrentTheme(QStringLiteral("dark"));
        QTRY_COMPARE(colorOf(button.icon()), QColor(0, 0, 255));

        QFile file(dir.filePath(QStringLiteral("square.svg")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"16\" height=\"16\">"
                   "<rect width=\"16\" height=\"16\" fill=\"#00FF00\"/></svg>");
        file.close();
        manager.reload();
        QTRY_COMPARE(colorOf(button.icon()), QColor(0, 255, 0));
    }

    void problems_are_reported() {
        ThemeManager manager;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("absent\\.qss")));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("not a JSON object")));
        manager.addSearchPath(themes("bad"));

        manager.setCurrentTheme(QStringLiteral("a"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("cycle")));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("missing")));
        // An unknown variable is left unchanged.
        QCOMPARE(manager.styleSheet({QStringLiteral("x")}),
                 QStringLiteral("X { color: ${missing}; }"));
    }
};

int main(int argc, char *argv[]) {
    // The offscreen platform requires no display.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_ThemeManager test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_ThemeManager.moc"
