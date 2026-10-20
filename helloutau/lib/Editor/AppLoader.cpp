#include "AppLoader.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QLibraryInfo>
#include <QtCore/QPointer>
#include <QtCore/QtDebug>
#include <QtGui/QFileOpenEvent>
#include <QtGui/QIcon>
#include <QtGui/QPixmap>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>

#include <stdcorelib/console.h>
#include <stdcorelib/pimpl.h>
#include <stdcorelib/pluginsystem/pluginsystem.h>
#include <stdcorelib/system.h>

#include <hellokit/NativeSystem/NativeMessageBox.h>
#include <hellokit/NativeSystem/NativeProcess.h>
#include <hellokit/Support/ExitLauncher.h>
#include <hellokit/Support/SettingsFile.h>
#include <hellokit/Support/SingleProcessPeer.h>
#include <hellokit/Support/TemporaryStorage.h>
#include <hellokit/Support/TranslationLoader.h>

#include "helloutau/Config.h"

#include "AppSettings.h"
#include "Editor.h"

namespace hello::daw {

    namespace {

        AppLoader *currentAppLoader = nullptr;

        // The title that Qt gives the settings item of the application menu of macOS, which
        // holds the action of the settings. qtbase translates the title as 首选项 (Preferences),
        // whereas the untranslated title appears as Settings on macOS 13 and later. The
        // translation of this application, which is installed after that of Qt and is therefore
        // looked up first, gives the item the name of the action.
        [[maybe_unused]] constexpr const char *preferencesMenuTitle =
            QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "Preferences...");

        // Returns whether the program in programDirectory is that of a macOS bundle, in
        // Contents/MacOS. The other directories of the bundle are Contents/Frameworks for the
        // libraries and the plugins of Qt, and Contents/Resources for the data, the counterparts
        // of lib and share beside the bin directory elsewhere.
        bool isBundle(const QString &programDirectory) {
#ifdef Q_OS_MACOS
            // The path is cleaned first. The file name of Contents/MacOS/.. is .., not Contents.
            return QDir::cleanPath(QDir(programDirectory).absolutePath())
                .endsWith(QLatin1String("/Contents/MacOS"));
#else
            Q_UNUSED(programDirectory);
            return false;
#endif
        }

        // Passes the files that the system asks the application to open to a function. The
        // Finder on macOS opens a file by a QFileOpenEvent to the application rather than on the
        // command line, also when the file launches the application.
        class FileOpenFilter : public QObject {
        public:
            explicit FileOpenFilter(std::function<void(const QString &)> open)
                : m_open(std::move(open)) {
            }

        protected:
            bool eventFilter(QObject *watched, QEvent *event) override {
                if (event->type() == QEvent::FileOpen) {
                    // A URL that is no local file has no file name, and is not opened.
                    if (const auto file = static_cast<QFileOpenEvent *>(event)->file();
                        !file.isEmpty()) {
                        m_open(file);
                        return true;
                    }
                }
                return QObject::eventFilter(watched, event);
            }

        private:
            std::function<void(const QString &)> m_open;
        };

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

#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
        // Returns the icon of the application from the icon theme that the installation holds on
        // Linux, one pixmap for each drawing in the directories of the theme. QPixmap renders an
        // SVG file at the size of its view box, the master at 256 px.
        QIcon applicationIcon(const QString &programDirectory) {
            const QDir theme(sharePath(programDirectory) + QStringLiteral("/icons/hicolor"));
            const auto file = QStringLiteral("/apps/" HELLOUTAU_APPLICATION_ID ".svg");
            QIcon icon;
            for (const auto &directory : theme.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                const QPixmap pixmap(theme.filePath(directory + file));
                if (!pixmap.isNull()) {
                    icon.addPixmap(pixmap);
                }
            }
            return icon;
        }
#endif

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
        bool allowsRoot = false;
        bool showsHelp = false;
        QPointer<Editor> editor;
        // Declared last, so that they are destroyed first and call no member of a destroyed
        // loader
        std::optional<FileOpenFilter> fileOpenFilter;
        std::optional<kit::SingleProcessPeer> peer;

        // Passes the files of a QFileOpenEvent, or the arguments of another process of the same
        // settings directory, to the plugins, which open them as remote commands. The plugin
        // system keeps a command until every plugin is initialized, so that the windows open
        // with every registration of the plugins in place.
        void passToPlugins(const QStringList &arguments, const QString &workingDirectory) {
            std::vector<std::string> converted;
            converted.reserve(arguments.size());
            for (const auto &argument : arguments) {
                converted.push_back(argument.toStdString());
            }
            system.remoteCommand(converted,
                                 std::filesystem::path(workingDirectory.toStdU16String()));
        }

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
            if (arguments[i] == QLatin1String(allowRootOption)) {
                impl.allowsRoot = true;
                impl.options += arguments[i];
                continue;
            }
            if (arguments[i] == QLatin1String(helpOption)) {
                impl.showsHelp = true;
                continue;
            }
            impl.files.push_back(arguments[i]);
        }
        // A restart starts the program again with the options and without the files.
        impl.exitLauncher.emplace(impl.options);
        if (const auto application = QCoreApplication::instance()) {
            impl.fileOpenFilter.emplace(
                [&impl](const QString &file) { impl.passToPlugins({file}, QString()); });
            application->installEventFilter(&*impl.fileOpenFilter);

            // One process per settings directory. A settings directory of the option is a
            // separate instance, as a portable copy or a test is.
            auto directory = QDir::cleanPath(QFileInfo(impl.settingsDirectory).absoluteFilePath());
#ifdef Q_OS_WINDOWS
            directory = directory.toLower();
#endif
            impl.peer.emplace(QCoreApplication::organizationName() + QLatin1Char('/') +
                              QCoreApplication::applicationName() + QLatin1Char('\n') + directory);
            QObject::connect(&*impl.peer, &kit::SingleProcessPeer::activateRequested, &*impl.peer,
                             [&impl](const QStringList &arguments, const QString &directory) {
                                 impl.passToPlugins(arguments, directory);
                             });
        }

        // A settings directory given by the option also holds the voice banks and the plugins of
        // the user, so that it is self-contained.
        const auto settingsFile = impl.settingsDirectory + QStringLiteral("/settings.json");
        if (impl.settingsDirectory == AppSettings::defaultDirectory()) {
            impl.settings.emplace(settingsFile, AppSettings::defaultUserDirectory());
        } else {
            impl.settings.emplace(settingsFile);
        }
        impl.readPluginSettings();
        // The plugins of the user follow those of the application, in Extensions/plugins of the
        // user directory of the settings
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

    bool AppLoader::forwardToRunningProcess() {
        stdc_impl_t;
        if (impl.showsHelp || !impl.peer || impl.peer->isPrimary()) {
            return false;
        }
        return impl.peer->sendToPrimary(impl.files, QDir::currentPath());
    }

    bool AppLoader::showStartMessages() const {
        stdc_impl_t;
        const auto title = QCoreApplication::applicationName();
        // Shows a message box of the system, or writes to the stream if none can be shown, in
        // which case the result is NoButton
        const auto show =
            [&title](
                const QString &text, kit::NativeMessageBox::Icon icon, FILE *stream,
                int buttons = kit::NativeMessageBox::Ok,
                kit::NativeMessageBox::StandardButton defaultButton =
                    kit::NativeMessageBox::NoButton,
                const std::map<kit::NativeMessageBox::StandardButton, std::string> &texts = {}) {
                const auto chosen = kit::NativeMessageBox::show(
                    title.toStdString(), text.toStdString(), icon, buttons, defaultButton, texts);
                if (chosen == kit::NativeMessageBox::NoButton) {
                    stdc::console::u8fprintf(stream, "%s\n", text.toStdString().c_str());
                }
                return chosen;
            };
        if (impl.showsHelp) {
            // Each part is translated alone, so that the description of --allow-root, which
            // differs between the systems, does not repeat the other parts in the translations.
            const auto directory = tr("<directory>");
            const auto option = [](const QString &name, const QString &description) {
                return QStringLiteral("  %1\n      %2").arg(name, description);
            };
#ifdef Q_OS_WINDOWS
            const auto allowRoot =
                tr("Starts without the warning about the privileges of the administrator.");
#else
            const auto allowRoot = tr("Starts without the warning about the privileges of root.");
#endif
            const QStringList lines{
                tr("Usage: %1 [options] [files]")
                    .arg(QFileInfo(QCoreApplication::applicationFilePath()).fileName()),
                {},
                tr("Starts %1 and opens the files.").arg(title),
                {},
                tr("Options:"),
                option(QLatin1String(settingsOption) + QLatin1Char(' ') + directory,
                       tr("The directory of the settings, which also holds the voice banks and "
                          "the plugins of the user.")),
                option(QLatin1String(pluginPathOption) + QLatin1Char(' ') + directory,
                       tr("A further directory of native plugins.")),
                option(QLatin1String(allowRootOption), allowRoot),
                option(QLatin1String(helpOption), tr("Shows this text.")),
            };
            show(lines.join(QLatin1Char('\n')), kit::NativeMessageBox::Information, stdout);
            return false;
        }
        if (!impl.allowsRoot && kit::NativeProcess::isElevated()) {
#ifdef Q_OS_WINDOWS
            const auto text = tr(
                "You are running %1 as the administrator. The synth tools and the plugins that "
                "a project runs then have the privileges of the administrator as well, and "
                "File Explorer cannot drop files into the windows of %1.\n\n"
                "Start %1 without using Run as administrator. The option --allow-root suppresses "
                "this warning.");
#else
            const auto text =
                tr("You are running %1 as the user root. The synth tools and the plugins that a "
                   "project runs then have the privileges of root as well, and the files that %1 "
                   "writes into the home directory belong to root.\n\n"
                   "Start %1 as a normal user. The option --allow-root suppresses this "
                   "warning.");
#endif
            // Quit at first, so that pressing Enter does not start with the privileges
            const auto chosen =
                show(text.arg(title), kit::NativeMessageBox::Warning, stderr,
                     kit::NativeMessageBox::Continue | kit::NativeMessageBox::Quit,
                     kit::NativeMessageBox::Quit,
                     {
                         {kit::NativeMessageBox::Continue, tr("Continue").toStdString()},
                         {kit::NativeMessageBox::Quit,     tr("Quit").toStdString()    },
            });
            return chosen != kit::NativeMessageBox::Quit;
        }
        return true;
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

    QString AppLoader::applicationDataPath(const QString &programDirectory) {
        const auto share = sharePath(programDirectory);
        return isBundle(programDirectory) ? share : share + QStringLiteral("/helloutau");
    }

    QString AppLoader::applicationTranslationPath(const QString &programDirectory) {
        return applicationDataPath(programDirectory) + QStringLiteral("/translations");
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
#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
        // The icon that X11 shows if the desktop has no entry of the application. Windows and
        // macOS take the icon from the program and from the bundle.
        QApplication::setWindowIcon(applicationIcon(QCoreApplication::applicationDirPath()));
#endif
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
