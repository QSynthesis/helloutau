#include "AppLoader.h"

#include <cassert>
#include <filesystem>
#include <optional>
#include <vector>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QLibraryInfo>
#include <QtCore/QPointer>
#include <QtCore/QtDebug>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>

#include <stdcorelib/pimpl.h>
#include <stdcorelib/pluginsystem/pluginsystem.h>
#include <stdcorelib/system.h>

#include <hellokit/Support/ExitLauncher.h>
#include <hellokit/Support/SettingsFile.h>
#include <hellokit/Support/TemporaryStorage.h>
#include <hellokit/Support/TranslationLoader.h>

#include "AppSettings.h"
#include "Editor.h"

namespace hello::daw {

    namespace {

        AppLoader *currentAppLoader = nullptr;

        // Returns whether the program in programDirectory is that of a macOS bundle, in
        // Contents/MacOS. The other directories of the bundle are Contents/Frameworks for the
        // libraries and the plugins of Qt, and Contents/Resources for the data, the counterparts
        // of lib and share beside the bin directory elsewhere.
        bool isBundle(const QString &programDirectory) {
#ifdef Q_OS_MACOS
            const QDir directory(programDirectory);
            return directory.dirName() == QLatin1String("MacOS") &&
                   QFileInfo(directory.absoluteFilePath(QStringLiteral(".."))).fileName() ==
                       QLatin1String("Contents");
#else
            Q_UNUSED(programDirectory);
            return false;
#endif
        }

        // Returns path relative to the parent of the directory of the program, the installation
        // or Contents of a bundle
        QString besideProgram(const QString &programDirectory, const QString &path) {
            return QDir::cleanPath(programDirectory + QStringLiteral("/../") + path);
        }

        // Returns the shared data directory, share or Contents/Resources of a bundle, as qmsetup
        // lays it out
        QString sharePath(const QString &programDirectory) {
            return besideProgram(programDirectory, isBundle(programDirectory)
                                                       ? QStringLiteral("Resources")
                                                       : QStringLiteral("share"));
        }

    }

    class AppLoader::Impl {
    public:
        // Declared first, so that they are destroyed last, after the plugins and their windows
        std::optional<kit::TemporaryStorage> temporaryStorage;
        std::optional<kit::ExitLauncher> exitLauncher;
        std::optional<kit::TranslationLoader> translations;
        stdc::pluginsystem::PluginSystem system{AppLoader::pluginIid,
                                                stdc::pluginsystem::PluginSystem::Bundle};
        QStringList pluginPaths;
        QStringList files;
        // The options of the command line without the files, which a restart passes again
        QStringList options;
        QString settingsDirectory;
        // Mutable, because settings() hands out the settings for changes from a const loader
        mutable std::optional<AppSettings> settings;
        stdc::pluginsystem::PluginSettings pluginSettings;
        // Declared after the settings of the plugins, so that it is destroyed first and writes
        // the pending changes
        std::optional<kit::SettingsFile> pluginFile;
        bool loaded = false;
        QPointer<Editor> editor;

        // Settings rejected by the library are reported and ignored. Every plugin then follows
        // its metadata until the next change replaces the settings.
        void readPluginSettings() {
            const auto fileName = settingsDirectory + QStringLiteral("/plugins.json");
            pluginFile.emplace(fileName, [this] { return pluginSettings.toJson(); });
            kit::DiagnosticList diagnostics;
            auto object = kit::SettingsFile::read(fileName, diagnostics);
            // TODO: Show the diagnostics in a list of the errors of the application, which does
            // not exist yet.
            for (const auto &diagnostic : std::as_const(diagnostics)) {
                qWarning().noquote() << diagnostic.message;
            }
            if (!object) {
                return;
            }
            std::string error;
            auto read = stdc::pluginsystem::PluginSettings::fromJson(
                stdc::json::Value(std::move(*object)), &error);
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

        // Every temporary file of the application in one directory, which the uninstaller
        // removes. See docs/Distribution.md.
        impl.temporaryStorage.emplace(std::filesystem::path(
            QDir::cleanPath(QDir::tempPath() + QLatin1Char('/') +
                            QCoreApplication::organizationName() + QLatin1Char('/') +
                            QCoreApplication::applicationName())
                .toStdU16String()));

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
        // A restart starts the program again with the options and without the files.
        impl.exitLauncher.emplace(impl.options);

        // A settings directory given by the option also holds the voice banks and the plugins of
        // the user, so that it is self-contained.
        const auto settingsFile = impl.settingsDirectory + QStringLiteral("/settings.json");
        if (impl.settingsDirectory == AppSettings::defaultDirectory()) {
            impl.settings.emplace(settingsFile, AppSettings::defaultUserDirectory());
        } else {
            impl.settings.emplace(settingsFile);
        }
        impl.readPluginSettings();
        // The plugins of the user after those of the application, which the user directory of
        // the settings determines
        impl.pluginPaths.insert(1, extensionPath() + QStringLiteral("/plugins"));

        impl.translations.emplace(
            AppearanceSettings::localeOf(impl.settings->appearance().language()));
        // The translation of Qt, so that the standard buttons and dialogs match the rest, and
        // that of the libraries
        impl.translations->load(QStringLiteral("qtbase"),
                                qtTranslationPaths(QCoreApplication::applicationDirPath()));
        impl.translations->load(QStringLiteral("helloutau"), translationPaths());
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
        // TODO: Build and test the macOS bundle. Its plugins of Qt are in
        // Contents/Frameworks/Qt/plugins rather than in Contents/PlugIns, which Qt searches
        // itself, because Contents/Plugins holds the plugins of the application in the layout of
        // qmsetup, and both names denote one directory on a case-insensitive file system.

        // The directory of the program from the system, known before the application object
        const auto directory =
            QString::fromStdU16String(stdc::system::application_directory().u16string());
        if (directory.isEmpty()) {
            return;
        }
        // addLibraryPath() prepends, so the paths are added from the last to the first. Qt places
        // the paths added before the application object ahead of the paths computed at its
        // creation.
        const auto paths = qtPluginPaths(directory);
        for (auto it = paths.crbegin(); it != paths.crend(); ++it) {
            QCoreApplication::addLibraryPath(*it);
        }
    }

    QStringList AppLoader::qtPluginPaths(const QString &programDirectory) {
        // lib/Qt/plugins is laid out as lib/helloutau/plugins.
        const auto path =
            isBundle(programDirectory)
                ? besideProgram(programDirectory, QStringLiteral("Frameworks/Qt/plugins"))
                : QDir::cleanPath(programDirectory + QLatin1Char('/') +
                                  QStringLiteral(HELLOUTAU_BUILTIN_PLUGINS_PATH) +
                                  QStringLiteral("/../../Qt/plugins"));
        return QFileInfo(path).isDir() ? QStringList{path} : QStringList();
    }

    QString AppLoader::extensionPath() const {
        stdc_impl_t;
        return QString::fromStdU16String(impl.settings->userDirectory().u16string()) +
               QStringLiteral("/Extensions");
    }

    QStringList AppLoader::translationPaths() const {
        return {extensionPath() + QStringLiteral("/translations"),
                applicationTranslationPath(QCoreApplication::applicationDirPath())};
    }

    QString AppLoader::applicationTranslationPath(const QString &programDirectory) {
        // The data directory of qmsetup, share/helloutau, or Contents/Resources of a bundle
        const auto share = sharePath(programDirectory);
        return isBundle(programDirectory) ? share + QStringLiteral("/translations")
                                          : share + QStringLiteral("/helloutau/translations");
    }

    QStringList AppLoader::qtTranslationPaths(const QString &programDirectory) {
        // Those of the installation, see helloutau/CMakeLists.txt, and those of Qt itself
        QStringList paths{sharePath(programDirectory) + QStringLiteral("/Qt/translations")};
        const auto system = QDir::cleanPath(QLibraryInfo::path(QLibraryInfo::TranslationsPath));
        if (!paths.contains(system)) {
            paths.append(system);
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

    void AppLoader::updatePluginSettings(
        const std::function<void(stdc::pluginsystem::PluginSettings &settings)> &change) {
        stdc_impl_t;
        change(impl.pluginSettings);
        impl.pluginFile->syncLater();
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
        // The restart and the installer of an update, which waits for this process to end
        impl.exitLauncher->launchAll();
        return code;
    }

    void AppLoader::shutdown() {
        stdc_impl_t;
        impl.system.shutdownPlugins();
    }

}
