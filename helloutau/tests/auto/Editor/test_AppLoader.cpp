#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QHash>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QStandardPaths>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <helloutau/Editor/AppLoader.h>
#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/ProjectWindow.h>

using namespace hello::daw;

namespace {

    /// Copies the plugin library \a library into the directory \a name of \a root with a
    /// plugin.json of the ID \a id and the further fields \a fields, the directory layout.
    void addPlugin(const QString &root, const QString &name, const QString &library,
                   const QString &id, const QString &fields = {}) {
        const QDir directory(root + QLatin1Char('/') + name);
        QVERIFY(directory.mkpath(QStringLiteral(".")));
        const QFileInfo info(library);
        QVERIFY(QFile::copy(library, directory.filePath(info.fileName())));

        QFile metadata(directory.filePath(QStringLiteral("plugin.json")));
        QVERIFY(metadata.open(QIODevice::WriteOnly));
        metadata.write(
            QStringLiteral(R"({"name":"%1","id":"%2","displayName":"%3","version":"1.0"%4})")
                .arg(info.completeBaseName(), id, name, fields)
                .toUtf8());
    }

    QStringList events() {
        return qApp->property("appLoaderEvents").toStringList();
    }

    // The command line of the program on \a files, with the settings in a directory of the test
    // rather than those of the user
    QStringList arguments(const QStringList &files = {}) {
        static QTemporaryDir directory;
        return QStringList({QStringLiteral("helloutau"), QLatin1String(AppLoader::settingsOption),
                            directory.path()}) +
               files;
    }

    int projectWindowCount() {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        int count = 0;
        for (const auto widget : QApplication::topLevelWidgets()) {
            count += qobject_cast<ProjectWindow *>(widget) ? 1 : 0;
        }
        return count;
    }

}

class test_AppLoader : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void init() {
        qApp->setProperty("appLoaderEvents", QStringList());
    }

    // --plugin-path adds the directory after it, --settings names the directory of the
    // settings, and the other arguments are files.
    void the_arguments_are_plugin_paths_and_files() {
        QCOMPARE(AppLoader::instance(), nullptr);
        QTemporaryDir directory;
        {
            const AppLoader loader({QStringLiteral("helloutau"), QStringLiteral("a.ust"),
                                    QStringLiteral("--plugin-path"), QStringLiteral("extra"),
                                    QStringLiteral("--settings"), directory.path(),
                                    QStringLiteral("b.ust"), QStringLiteral("--plugin-path")});
            QCOMPARE(AppLoader::instance(), &loader);
            QCOMPARE(loader.pluginPaths(),
                     QStringList({AppLoader::builtinPluginPath(), QStringLiteral("extra")}));
            QCOMPARE(loader.files(),
                     QStringList({QStringLiteral("a.ust"), QStringLiteral("b.ust")}));
            QCOMPARE(loader.settingsDirectory(), directory.path());
            QCOMPARE(loader.settings().fileName(),
                     directory.filePath(QStringLiteral("settings.json")));
        }
        // Without --settings, those of the user
        {
            const AppLoader loader({QStringLiteral("helloutau")});
            QCOMPARE(loader.settingsDirectory(), AppSettings::defaultDirectory());
        }
        QCOMPARE(AppLoader::instance(), nullptr);

        // The layout outside a macOS bundle, where the tests run
        QCOMPARE(AppLoader::builtinPluginPath(),
                 QDir::cleanPath(QCoreApplication::applicationDirPath() +
                                 QStringLiteral("/../lib/plugins/helloutau")));
    }

    // The core plugin is initialized, told when every plugin is, and shut down once.
    void the_core_plugin_runs() {
        QTemporaryDir root;
        addPlugin(root.path(), QStringLiteral("Core"), QStringLiteral(TEST_APPLOADER_CORE),
                  QLatin1String(AppLoader::corePluginId));

        AppLoader loader(arguments({QStringLiteral("a.ust")}));
        loader.setPluginPaths({root.path()});
        QCOMPARE(loader.pluginPaths(), QStringList({root.path()}));
        QString error;
        QVERIFY2(loader.load(&error), qPrintable(error));
        QCOMPARE(events(), QStringList({QStringLiteral("initialize a.ust"),
                                        QStringLiteral("pluginsInitialized")}));

        // Loading again loads nothing.
        QVERIFY(loader.load(&error));
        QCOMPARE(events().size(), 2);

        loader.shutdown();
        loader.shutdown();
        QCOMPARE(events(), QStringList({QStringLiteral("initialize a.ust"),
                                        QStringLiteral("pluginsInitialized"),
                                        QStringLiteral("aboutToShutdown")}));
    }

    // Another plugin that fails leaves the application running, its reason among the errors.
    void another_plugin_may_fail() {
        QTemporaryDir root;
        addPlugin(root.path(), QStringLiteral("Core"), QStringLiteral(TEST_APPLOADER_CORE),
                  QLatin1String(AppLoader::corePluginId));
        addPlugin(root.path(), QStringLiteral("Other"),
                  QStringLiteral(TEST_APPLOADER_FAILING_CORE),
                  QStringLiteral("org.helloutau.other"));
        AppLoader loader(arguments());
        loader.setPluginPaths({root.path()});
        QString error;
        QTest::ignoreMessage(QtWarningMsg, "Plugin org.helloutau.other: intentional failure");
        QVERIFY2(loader.load(&error), qPrintable(error));
        QCOMPARE(loader.errors(),
                 QStringList({QStringLiteral("org.helloutau.other: intentional failure")}));
    }

    // Every plugin found is listed with what became of it, running, disabled or failed.
    void the_plugins_are_listed() {
        QTemporaryDir root;
        addPlugin(root.path(), QStringLiteral("Core"), QStringLiteral(TEST_APPLOADER_CORE),
                  QLatin1String(AppLoader::corePluginId));
        addPlugin(root.path(), QStringLiteral("Off"), QStringLiteral(TEST_APPLOADER_CORE),
                  QStringLiteral("org.test.off"), QStringLiteral(R"(,"enabledByDefault":false)"));
        addPlugin(
            root.path(), QStringLiteral("Failing"), QStringLiteral(TEST_APPLOADER_FAILING_CORE),
            QStringLiteral("org.test.failing"),
            QStringLiteral(
                R"(,"dependencies":[{"id":"org.helloutau.core","version":"1.0","type":"required"},)"
                R"({"id":"org.test.off","version":"1.0","type":"optional"}])"));
        AppLoader loader(arguments());
        loader.setPluginPaths({root.path()});
        QString error;
        QTest::ignoreMessage(QtWarningMsg, "Plugin org.test.failing: intentional failure");
        QVERIFY2(loader.load(&error), qPrintable(error));

        QHash<QString, AppLoader::PluginInfo> plugins;
        for (const auto &info : loader.plugins()) {
            plugins.insert(info.id, info);
        }
        QCOMPARE(plugins.size(), 3);

        const auto core = plugins.value(QLatin1String(AppLoader::corePluginId));
        QCOMPARE(core.state, AppLoader::PluginInfo::Running);
        QCOMPARE(core.displayName, QStringLiteral("Core"));
        QCOMPARE(core.version, QStringLiteral("1.0"));
        QCOMPARE(QFileInfo(core.filePath).absoluteDir(),
                 QDir(QDir(root.path()).filePath(QStringLiteral("Core"))));
        QVERIFY(core.dependencies.isEmpty());
        QVERIFY(core.error.isEmpty());
        QVERIFY(core.enabledByDefault);
        QVERIFY(core.enabled);

        const auto off = plugins.value(QStringLiteral("org.test.off"));
        QCOMPARE(off.state, AppLoader::PluginInfo::Disabled);
        QVERIFY(!off.enabledByDefault);
        QVERIFY(!off.enabled);

        const auto failing = plugins.value(QStringLiteral("org.test.failing"));
        QCOMPARE(failing.state, AppLoader::PluginInfo::Failed);
        QCOMPARE(failing.error, QStringLiteral("intentional failure"));
        QCOMPARE(failing.dependencies.size(), 2);
        QCOMPARE(failing.dependencies[0].id, QLatin1String(AppLoader::corePluginId));
        QVERIFY(!failing.dependencies[0].optional);
        QCOMPARE(failing.dependencies[1].id, QStringLiteral("org.test.off"));
        QVERIFY(failing.dependencies[1].optional);
        // A disabled plugin is no error.
        QCOMPARE(loader.errors(),
                 QStringList({QStringLiteral("org.test.failing: intentional failure")}));

        loader.shutdown();
        for (const auto &info : loader.plugins()) {
            QVERIFY(info.state != AppLoader::PluginInfo::Running);
        }
    }

    // The choice of the user to enable or disable a plugin goes into plugins.json, and leaves
    // the plugins that run as they are until the next start.
    void the_user_enables_and_disables_plugins() {
        QTemporaryDir directory;
        QTemporaryDir root;
        addPlugin(root.path(), QStringLiteral("Core"), QStringLiteral(TEST_APPLOADER_CORE),
                  QLatin1String(AppLoader::corePluginId));
        addPlugin(root.path(), QStringLiteral("Off"), QStringLiteral(TEST_APPLOADER_CORE),
                  QStringLiteral("org.test.off"), QStringLiteral(R"(,"enabledByDefault":false)"));
        const QStringList command = {QStringLiteral("helloutau"), QStringLiteral("--settings"),
                                     directory.path()};
        const auto file = directory.filePath(QStringLiteral("plugins.json"));
        const auto written = [&] {
            QFile in(file);
            [&] { QVERIFY(in.open(QIODevice::ReadOnly)); }();
            return QJsonDocument::fromJson(in.readAll()).object();
        };
        QString error;
        {
            AppLoader loader(command);
            loader.setPluginPaths({root.path()});
            QVERIFY2(loader.load(&error), qPrintable(error));
            QCOMPARE(loader.pluginEnabled(QStringLiteral("org.test.off")), std::nullopt);
            loader.setPluginEnabled(QStringLiteral("org.test.off"), true);
            loader.setPluginEnabled(QStringLiteral("org.test.other"), false);
            QCOMPARE(loader.pluginEnabled(QStringLiteral("org.test.off")), true);
            QCOMPARE(loader.pluginEnabled(QStringLiteral("org.test.other")), false);
            for (const auto &info : loader.plugins()) {
                if (info.id == QStringLiteral("org.test.off")) {
                    QCOMPARE(info.state, AppLoader::PluginInfo::Disabled);
                }
            }
            loader.syncSettings();
            QCOMPARE(written().value(QStringLiteral("enabledPlugins")),
                     QJsonValue(QJsonArray({QStringLiteral("org.test.off")})));
            QCOMPARE(written().value(QStringLiteral("disabledPlugins")),
                     QJsonValue(QJsonArray({QStringLiteral("org.test.other")})));
        }
        {
            AppLoader loader(command);
            loader.setPluginPaths({root.path()});
            QVERIFY2(loader.load(&error), qPrintable(error));
            bool found = false;
            for (const auto &info : loader.plugins()) {
                if (info.id == QStringLiteral("org.test.off")) {
                    found = true;
                    QCOMPARE(info.state, AppLoader::PluginInfo::Running);
                    QVERIFY(!info.enabledByDefault);
                    QVERIFY(info.enabled);
                }
            }
            QVERIFY(found);
            loader.setPluginEnabled(QStringLiteral("org.test.off"), std::nullopt);
            QCOMPARE(loader.pluginEnabled(QStringLiteral("org.test.off")), std::nullopt);
        }
        QCOMPARE(written().value(QStringLiteral("enabledPlugins")), QJsonValue(QJsonArray()));
    }

    // Without the core plugin, running or not, the loader gives the reason.
    void the_core_plugin_is_required() {
        QString error;
        {
            // A plugin of another ID is no core plugin.
            QTemporaryDir root;
            addPlugin(root.path(), QStringLiteral("Other"), QStringLiteral(TEST_APPLOADER_CORE),
                      QStringLiteral("org.helloutau.other"));
            AppLoader loader(arguments());
            loader.setPluginPaths({root.path()});
            QVERIFY(!loader.load(&error));
            QCOMPARE(error, QStringLiteral("The core plugin was not found."));
            QCOMPARE(events().first(), QStringLiteral("initialize "));
        }
        {
            QTemporaryDir root;
            addPlugin(root.path(), QStringLiteral("Core"),
                      QStringLiteral(TEST_APPLOADER_FAILING_CORE),
                      QLatin1String(AppLoader::corePluginId));
            AppLoader loader(arguments());
            loader.setPluginPaths({root.path()});
            QVERIFY(!loader.load(&error));
            QCOMPARE(error, QStringLiteral("intentional failure"));
        }
        {
            QTemporaryDir root;
            addPlugin(root.path(), QStringLiteral("Core"), QStringLiteral(TEST_APPLOADER_CORE),
                      QLatin1String(AppLoader::corePluginId),
                      QStringLiteral(R"(,"enabledByDefault":false)"));
            AppLoader loader(arguments());
            loader.setPluginPaths({root.path()});
            QVERIFY(!loader.load(&error));
            QCOMPARE(error, QStringLiteral("The core plugin is disabled."));
        }
    }

    // The plugins that plugins.json enables or disables override their metadata.
    void the_settings_enable_and_disable_plugins() {
        QTemporaryDir directory;
        const auto settingsOf = [&](const QByteArray &json) {
            QFile out(directory.filePath(QStringLiteral("plugins.json")));
            [&] { QVERIFY(out.open(QIODevice::WriteOnly)); }();
            out.write(json);
            return directory.path();
        };
        QString error;
        {
            QTemporaryDir root;
            addPlugin(root.path(), QStringLiteral("Core"), QStringLiteral(TEST_APPLOADER_CORE),
                      QLatin1String(AppLoader::corePluginId));
            AppLoader loader({QStringLiteral("helloutau"), QStringLiteral("--settings"),
                              settingsOf(R"({"disabledPlugins": ["org.helloutau.core"]})")});
            loader.setPluginPaths({root.path()});
            QVERIFY(!loader.load(&error));
            QCOMPARE(error, QStringLiteral("The core plugin is disabled."));
        }
        {
            QTemporaryDir root;
            addPlugin(root.path(), QStringLiteral("Core"), QStringLiteral(TEST_APPLOADER_CORE),
                      QLatin1String(AppLoader::corePluginId),
                      QStringLiteral(R"(,"enabledByDefault":false)"));
            AppLoader loader({QStringLiteral("helloutau"), QStringLiteral("--settings"),
                              settingsOf(R"({"enabledPlugins": ["org.helloutau.core"]})")});
            loader.setPluginPaths({root.path()});
            QVERIFY2(loader.load(&error), qPrintable(error));
        }
    }

    // A plugin keeps its values under its ID in the userData of plugins.json, beside the
    // plugins that the user enabled or disabled, which stay. The application's settings are
    // another file.
    void the_plugins_keep_their_values() {
        QTemporaryDir directory;
        const auto file = directory.filePath(QStringLiteral("plugins.json"));
        {
            QFile out(file);
            QVERIFY(out.open(QIODevice::WriteOnly));
            out.write(R"({"disabledPlugins": ["org.test.off"]})");
        }
        const QStringList command = {QStringLiteral("helloutau"), QStringLiteral("--settings"),
                                     directory.path()};
        {
            AppLoader loader(command);
            QVERIFY(loader.pluginValue(QStringLiteral("org.test.p"), QStringLiteral("a/b"))
                        .isUndefined());
            loader.setPluginValue(QStringLiteral("org.test.p"), QStringLiteral("a/b"), 7);
            loader.setPluginValue(QStringLiteral("org.test.q"), QStringLiteral("c"),
                                  QJsonArray({QStringLiteral("x")}));
        }
        {
            AppLoader loader(command);
            QCOMPARE(loader.pluginValue(QStringLiteral("org.test.p"), QStringLiteral("a/b")),
                     QJsonValue(7));
            QCOMPARE(loader.pluginValue(QStringLiteral("org.test.q"), QStringLiteral("c")),
                     QJsonValue(QJsonArray({QStringLiteral("x")})));
            loader.setPluginValue(QStringLiteral("org.test.q"), QStringLiteral("c"), QJsonValue());
        }

        QFile in(file);
        QVERIFY(in.open(QIODevice::ReadOnly));
        const auto root = QJsonDocument::fromJson(in.readAll()).object();
        QCOMPARE(root.value(QStringLiteral("disabledPlugins")),
                 QJsonValue(QJsonArray({QStringLiteral("org.test.off")})));
        const auto userData = root.value(QStringLiteral("userData")).toObject();
        QCOMPARE(userData.value(QStringLiteral("org.test.p")),
                 QJsonValue(QJsonObject({
                     {QStringLiteral("a"), QJsonObject({{QStringLiteral("b"), 7}})}
        })));
        QVERIFY(!userData.contains(QStringLiteral("org.test.q")));
        QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("settings.json"))));
    }

    // The two files are written once the event loop runs, or at once by syncSettings().
    void the_settings_are_written_later_or_on_sync() {
        QTemporaryDir directory;
        const auto plugins = directory.filePath(QStringLiteral("plugins.json"));
        const auto settings = directory.filePath(QStringLiteral("settings.json"));
        AppLoader loader(
            {QStringLiteral("helloutau"), QStringLiteral("--settings"), directory.path()});
        loader.setPluginValue(QStringLiteral("org.test.p"), QStringLiteral("a"), 1);
        loader.settings().setResampler(QStringLiteral("r.exe"));
        QVERIFY(!QFile::exists(plugins));
        QVERIFY(!QFile::exists(settings));
        loader.syncSettings();
        QVERIFY(QFile::exists(plugins));
        QVERIFY(QFile::exists(settings));

        QVERIFY(QFile::remove(plugins));
        loader.setPluginValue(QStringLiteral("org.test.p"), QStringLiteral("a"), 2);
        QTRY_VERIFY(QFile::exists(plugins));
    }

    // The core plugin that comes with the application, found beside the program, opens a new
    // project, and its windows close when it shuts down.
    void the_builtin_core_plugin_opens_a_window() {
        AppLoader loader(arguments());
        QString error;
        QVERIFY2(loader.load(&error), qPrintable(error));
        // Every plugin that comes with the application loads.
        QCOMPARE(loader.errors(), QStringList());
        QCOMPARE(projectWindowCount(), 1);

        loader.shutdown();
        QCOMPARE(projectWindowCount(), 0);
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display. The directory of the settings of the user is one for tests, should
    // a loader reach it.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    test_AppLoader test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_AppLoader.moc"
