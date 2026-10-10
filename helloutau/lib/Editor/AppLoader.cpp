#include "AppLoader.h"

#include <cassert>
#include <filesystem>
#include <vector>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QPointer>
#include <QtCore/QtDebug>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>

#include <stdcorelib/pimpl.h>
#include <stdcorelib/pluginsystem/pluginsystem.h>
#include <stdcorelib/system.h>

#include <hellokit/Support/SettingsFile.h>

#include "AppSettings.h"
#include "Editor.h"
#include "Restarter.h"
#include "Translations.h"

namespace hello::daw {

    namespace {

        AppLoader *currentAppLoader = nullptr;

    }

    class AppLoader::Impl {
    public:
        stdc::pluginsystem::PluginSystem system{AppLoader::pluginIid,
                                                stdc::pluginsystem::PluginSystem::Bundle};
        QStringList pluginPaths;
        QStringList files;
        // The options of the command line without the files, which a restart passes again
        QStringList options;
        QString settingsDirectory;
        std::unique_ptr<AppSettings> settings;
        stdc::pluginsystem::PluginSettings pluginSettings;
        // Declared after the settings of the plugins, so that it is destroyed first and writes
        // the pending changes
        std::unique_ptr<kit::SettingsFile> pluginFile;
        bool loaded = false;
        QPointer<Editor> editor;

        // Settings rejected by the library are reported and ignored. Every plugin then follows
        // its metadata until the next change replaces the settings.
        void readPluginSettings() {
            const auto fileName = settingsDirectory + QStringLiteral("/plugins.json");
            pluginFile = std::make_unique<kit::SettingsFile>(
                fileName, [this] { return pluginSettings.toJson(); });
            std::string error;
            auto read = stdc::pluginsystem::PluginSettings::fromJson(
                stdc::json::Value(kit::SettingsFile::read(fileName)), &error);
            if (!read) {
                qWarning().noquote() << "The settings of the plugins in" << fileName
                                     << "are ignored:" << QString::fromStdString(error);
                return;
            }
            pluginSettings = std::move(*read);
        }
    };

    AppLoader::AppLoader(const QStringList &arguments) : _impl(std::make_unique<Impl>()) {
        stdc_impl_t;
        assert(!currentAppLoader);
        currentAppLoader = this;

        impl.pluginPaths.push_back(builtinPluginPath());
        impl.settingsDirectory = AppSettings::defaultDirectory();
        for (int i = 1; i < arguments.size(); ++i) {
            // A trailing option without its value is ignored.
            if (arguments[i] == QLatin1String(pluginPathOption)) {
                if (++i < arguments.size()) {
                    impl.pluginPaths.push_back(arguments[i]);
                    impl.options += {arguments[i - 1], arguments[i]};
                }
                continue;
            }
            if (arguments[i] == QLatin1String(settingsOption)) {
                if (++i < arguments.size()) {
                    impl.settingsDirectory = arguments[i];
                    impl.options += {arguments[i - 1], arguments[i]};
                }
                continue;
            }
            impl.files.push_back(arguments[i]);
        }
        impl.settings = std::make_unique<AppSettings>(impl.settingsDirectory +
                                                      QStringLiteral("/settings.json"));
        impl.readPluginSettings();
        Translations::install(impl.settings->language());
    }

    AppLoader::~AppLoader() {
        shutdown();
        currentAppLoader = nullptr;
    }

    AppLoader *AppLoader::instance() {
        return currentAppLoader;
    }

    QString AppLoader::builtinPluginPath() {
        return QDir::cleanPath(QCoreApplication::applicationDirPath() + QLatin1Char('/') +
                               QStringLiteral(HELLOUTAU_BUILTIN_PLUGINS_PATH));
    }

    void AppLoader::addQtPluginPaths() {
        // The directory of the program from the system, known before the application object
        const auto directory =
            QString::fromStdU16String(stdc::system::application_directory().u16string());
        if (directory.isEmpty()) {
            return;
        }
        // addLibraryPath() prepends, so the paths are added from the last to the first. Qt keeps
        // the paths added before the application object, ahead of those it computes then.
        const auto paths = qtPluginPaths(directory);
        for (auto it = paths.crbegin(); it != paths.crend(); ++it) {
            QCoreApplication::addLibraryPath(*it);
        }
    }

    QStringList AppLoader::qtPluginPaths(const QString &programDirectory) {
        // lib/plugins/Qt is the sibling of lib/plugins/helloutau.
        const QString candidates[] = {
            QDir::cleanPath(programDirectory + QStringLiteral("/plugins")),
            QDir::cleanPath(programDirectory + QLatin1Char('/') +
                            QStringLiteral(HELLOUTAU_BUILTIN_PLUGINS_PATH) +
                            QStringLiteral("/../Qt")),
        };
        QStringList paths;
        for (const auto &path : candidates) {
            if (QFileInfo(path).isDir()) {
                paths.push_back(path);
            }
        }
        return paths;
    }

    QStringList AppLoader::pluginPaths() const {
        stdc_impl_t;
        return impl.pluginPaths;
    }

    void AppLoader::setPluginPaths(const QStringList &paths) {
        stdc_impl_t;
        impl.pluginPaths = paths;
    }

    QStringList AppLoader::files() const {
        stdc_impl_t;
        return impl.files;
    }

    void AppLoader::syncSettings() {
        stdc_impl_t;
        impl.settings->sync();
        impl.pluginFile->sync();
    }

    QString AppLoader::settingsDirectory() const {
        stdc_impl_t;
        return impl.settingsDirectory;
    }

    const stdc::pluginsystem::PluginSettings &AppLoader::pluginSettings() const {
        stdc_impl_t;
        return impl.pluginSettings;
    }

    void AppLoader::changePluginSettings(
        const std::function<void(stdc::pluginsystem::PluginSettings &settings)> &change) {
        stdc_impl_t;
        change(impl.pluginSettings);
        impl.pluginFile->changed();
    }

    AppSettings &AppLoader::settings() const {
        stdc_impl_t;
        return *impl.settings;
    }

    Editor *AppLoader::editor() const {
        stdc_impl_t;
        return impl.editor;
    }

    void AppLoader::setEditor(Editor *editor) {
        stdc_impl_t;
        impl.editor = editor;
    }

    bool AppLoader::load(QString *error) {
        stdc_impl_t;
        if (!impl.loaded) {
            impl.loaded = true;
            std::vector<std::filesystem::path> paths;
            for (const auto &path : std::as_const(impl.pluginPaths)) {
                paths.emplace_back(path.toStdU16String());
            }
            impl.system.setPluginPaths(paths);
            // The plugins that the user enabled or disabled override their metadata.
            impl.system.setPluginSettings(stdc::pluginsystem::PluginSystem::Local,
                                          impl.pluginSettings);
            impl.system.loadPlugins();
        }

        for (const auto &message : errors()) {
            qWarning().noquote() << "Plugin" << message;
        }
        stdc::pluginsystem::PluginSpec *core = nullptr;
        for (const auto spec : impl.system.plugins()) {
            if (spec->id() == corePluginId) {
                core = spec;
                break;
            }
        }

        if (!core) {
            *error = tr("The core plugin was not found.");
            return false;
        }
        if (core->hasError()) {
            *error = QString::fromStdString(core->errorMessage());
            return false;
        }
        if (core->state() != stdc::pluginsystem::PluginSpec::Running) {
            *error = tr("The core plugin is disabled.");
            return false;
        }
        return true;
    }

    const stdc::pluginsystem::PluginSystem &AppLoader::pluginSystem() const {
        stdc_impl_t;
        return impl.system;
    }

    QStringList AppLoader::errors() const {
        stdc_impl_t;
        QStringList result;
        bool coreSeen = false;
        for (const auto spec : impl.system.plugins()) {
            // load() reports the first core plugin. Another plugin with the same ID is an error.
            if (spec->id() == corePluginId && !coreSeen) {
                coreSeen = true;
                continue;
            }
            if (spec->hasError()) {
                result.push_back(QString::fromStdString(spec->id() + ": " + spec->errorMessage()));
            }
        }
        return result;
    }

    int AppLoader::run() {
        stdc_impl_t;
        QString error;
        if (!load(&error)) {
            QMessageBox::critical(nullptr, QApplication::applicationName(),
                                  tr("The application cannot start: %1").arg(error));
            return 1;
        }
        const int code = QApplication::exec();
        shutdown();
        Restarter::startAgain(impl.options);
        return code;
    }

    void AppLoader::shutdown() {
        stdc_impl_t;
        impl.system.shutdownPlugins();
    }

}
