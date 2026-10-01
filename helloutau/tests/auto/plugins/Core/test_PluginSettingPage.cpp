#include <optional>

#include <QtCore/QStandardPaths>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTreeWidget>

#include <helloutau/Editor/AppLoader.h>

#include <Core/PluginSettingPage.h>

using namespace hello::daw;

class test_PluginSettingPage : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The page lists the plugins that the loader found, the core plugin checked and locked. A
    // plugin unchecked and applied is recorded as disabled for the next start, with the notice
    // to restart. Checked again, the record is removed, since its metadata enables it.
    void the_plugins_are_enabled_for_the_next_start() {
        QTemporaryDir settings;
        AppLoader loader({QStringLiteral("helloutau"), QLatin1String(AppLoader::settingsOption),
                          settings.path()});
        loader.setPluginPaths({AppLoader::builtinPluginPath()});
        QString error;
        QVERIFY2(loader.load(&error), qPrintable(error));
        const auto plugins = loader.plugins();

        PluginSettingPage page(loader);
        QCOMPARE(page.id(), QStringLiteral("core.Plugins"));
        const auto tree = page.widget()->findChild<QTreeWidget *>(QStringLiteral("plugins"));
        const auto restart = page.widget()->findChild<QLabel *>(QStringLiteral("restart"));
        QVERIFY(tree && restart);
        QCOMPARE(tree->topLevelItemCount(), plugins.size());

        int core = -1;
        int other = -1;
        for (int row = 0; row < plugins.size(); ++row) {
            if (plugins[row].id == QLatin1String(AppLoader::corePluginId)) {
                core = row;
            } else if (other < 0 && plugins[row].state == AppLoader::PluginInfo::Running &&
                       plugins[row].enabledByDefault) {
                other = row;
            }
        }
        QVERIFY(core >= 0 && other >= 0);
        const auto coreItem = tree->topLevelItem(core);
        QCOMPARE(coreItem->checkState(0), Qt::Checked);
        QVERIFY(!(coreItem->flags() & Qt::ItemIsUserCheckable));
        QVERIFY(restart->isHidden());
        QVERIFY(!page.isModified());

        const auto item = tree->topLevelItem(other);
        const auto id = plugins[other].id;
        item->setCheckState(0, Qt::Unchecked);
        QVERIFY(page.isModified());
        QVERIFY(!restart->isHidden());
        QVERIFY(page.apply(&error));
        QVERIFY(!page.isModified());
        QCOMPARE(loader.pluginEnabled(id), std::optional<bool>(false));
        // The plugin still runs until the next start.
        QVERIFY(!restart->isHidden());

        item->setCheckState(0, Qt::Checked);
        QVERIFY(page.apply(&error));
        QCOMPARE(loader.pluginEnabled(id), std::optional<bool>());
        QVERIFY(restart->isHidden());

        loader.shutdown();
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display. The settings directory of the user is a test directory in case a
    // loader accesses it.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    test_PluginSettingPage test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_PluginSettingPage.moc"
