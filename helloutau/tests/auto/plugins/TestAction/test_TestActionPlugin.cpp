#include <QtCore/QStandardPaths>
#include <QtCore/QTemporaryDir>
#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>

#include <QAKCore/actionregistry.h>
#include <QAKWidgets/widgetactioncontext.h>

#include <helloutau/Widgets/SettingPage.h>

#include <helloutau/Editor/AppLoader.h>
#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/ProjectWindow.h>

using namespace hello::daw;

namespace {

    const QString HelloId = QStringLiteral("helloutau.test.hello");

    ProjectWindow *projectWindow() {
        for (const auto widget : QApplication::topLevelWidgets()) {
            if (const auto window = qobject_cast<ProjectWindow *>(widget)) {
                return window;
            }
        }
        return nullptr;
    }

    QStringList toolsMenu(const ProjectWindow *window) {
        QStringList texts;
        for (const auto action : window->menuBar()->actions()) {
            if (action->text() == QStringLiteral("&Tools")) {
                for (const auto item : action->menu()->actions()) {
                    texts.push_back(item->text());
                }
            }
        }
        return texts;
    }

}

class test_TestActionPlugin : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // If loaded with the core plugin, the plugin adds its command to the window that the core
    // plugin opens, which makes the command available to the command palette. The plugin
    // removes the command at shutdown.
    void the_plugin_adds_a_command_to_the_project_window() {
        // The settings in a directory of the test instead of the settings directory of the user
        QTemporaryDir settings;
        AppLoader loader({QStringLiteral("helloutau"), QLatin1String(AppLoader::settingsOption),
                          settings.path()});
        loader.setPluginPaths(
            {AppLoader::builtinPluginPath(), QStringLiteral(TEST_ACTION_PLUGINS_DIR)});
        QString error;
        QVERIFY2(loader.load(&error), qPrintable(error));

        const auto window = projectWindow();
        QVERIFY(window);
        // The core plugin adds its pages to the settings, the Plugins page with the loader, see
        // test_CoreSettingPages.
        QVERIFY(window->editor()->settingCatalog()->page(QStringLiteral("core.Keymap")));
        QVERIFY(window->editor()->settingCatalog()->page(QStringLiteral("core.Plugins")));
        const auto action = window->actionContext()->action(HelloId);
        QVERIFY(action);
        QVERIFY(action->isEnabled());
        QVERIFY(toolsMenu(window).contains(QStringLiteral("Hello")));

        action->trigger();
        QCOMPARE(qApp->property("testActionTriggered").toInt(), 1);

        loader.shutdown();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!projectWindow());
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display. The settings directory of the user is a test directory in case a
    // loader accesses it.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    test_TestActionPlugin test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_TestActionPlugin.moc"
