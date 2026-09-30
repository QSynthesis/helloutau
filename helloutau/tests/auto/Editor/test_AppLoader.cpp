#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
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
                            directory.filePath(QStringLiteral("settings.json"))}) +
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

    // --plugin-path adds the directory after it, --settings names the settings file, and the
    // other arguments are files.
    void the_arguments_are_plugin_paths_and_files() {
        QCOMPARE(AppLoader::instance(), nullptr);
        QTemporaryDir directory;
        const auto settings = directory.filePath(QStringLiteral("settings.json"));
        {
            const AppLoader loader({QStringLiteral("helloutau"), QStringLiteral("a.ust"),
                                    QStringLiteral("--plugin-path"), QStringLiteral("extra"),
                                    QStringLiteral("--settings"), settings, QStringLiteral("b.ust"),
                                    QStringLiteral("--plugin-path")});
            QCOMPARE(AppLoader::instance(), &loader);
            QCOMPARE(loader.pluginPaths(),
                     QStringList({AppLoader::builtinPluginPath(), QStringLiteral("extra")}));
            QCOMPARE(loader.files(),
                     QStringList({QStringLiteral("a.ust"), QStringLiteral("b.ust")}));
            QCOMPARE(loader.settings().fileName(), settings);
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

    // The plugins that the settings enable or disable override their metadata.
    void the_settings_enable_and_disable_plugins() {
        QTemporaryDir directory;
        const auto settingsOf = [&](const QByteArray &json) {
            const auto file = directory.filePath(QStringLiteral("settings.json"));
            QFile out(file);
            [&] { QVERIFY(out.open(QIODevice::WriteOnly)); }();
            out.write(json);
            return file;
        };
        QString error;
        {
            QTemporaryDir root;
            addPlugin(root.path(), QStringLiteral("Core"), QStringLiteral(TEST_APPLOADER_CORE),
                      QLatin1String(AppLoader::corePluginId));
            AppLoader loader(
                {QStringLiteral("helloutau"), QStringLiteral("--settings"),
                 settingsOf(R"({"plugins": {"disabledPlugins": ["org.helloutau.core"]}})")});
            loader.setPluginPaths({root.path()});
            QVERIFY(!loader.load(&error));
            QCOMPARE(error, QStringLiteral("The core plugin is disabled."));
        }
        {
            QTemporaryDir root;
            addPlugin(root.path(), QStringLiteral("Core"), QStringLiteral(TEST_APPLOADER_CORE),
                      QLatin1String(AppLoader::corePluginId),
                      QStringLiteral(R"(,"enabledByDefault":false)"));
            AppLoader loader(
                {QStringLiteral("helloutau"), QStringLiteral("--settings"),
                 settingsOf(R"({"plugins": {"enabledPlugins": ["org.helloutau.core"]}})")});
            loader.setPluginPaths({root.path()});
            QVERIFY2(loader.load(&error), qPrintable(error));
        }
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
    // Runs without a display.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_AppLoader test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_AppLoader.moc"
