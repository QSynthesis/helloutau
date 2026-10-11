#ifndef HELLOUTAU_EDITOR_APPLOADER_H
#define HELLOUTAU_EDITOR_APPLOADER_H

#include <functional>
#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <stdcorelib/pluginsystem/pluginsettings.h>
#include <stdcorelib/pluginsystem/pluginsystem.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    class AppSettings;
    class Editor;

    /// Application loader. Loads the native plugins and runs the event loop. The core plugin
    /// creates the editor and opens its windows. The program performs no other work. See
    /// docs/Plugins.md.
    ///
    /// At most one loader exists at a time. Plugins access it through instance().
    class HELLOUTAU_EDITOR_EXPORT AppLoader {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::AppLoader)
    public:
        /// Interface ID of every native plugin. The CMake variable HELLOUTAU_PLUGIN_IID of the
        /// plugins of this repository holds the same value.
        static constexpr char pluginIid[] = "org.OpenVPI.HelloUtau.Plugin";

        /// ID of the core plugin, which the application requires in order to start.
        static constexpr char corePluginId[] = "org.helloutau.core";

        /// Command-line option that adds the following directory to the plugin paths.
        static constexpr char pluginPathOption[] = "--plugin-path";

        /// Command-line option that specifies the following directory as the settings directory
        /// in place of the directory of the user. The directory also serves as the user directory
        /// of the settings (AppSettings::userDirectory()).
        static constexpr char settingsOption[] = "--settings";

        /// Creates a loader for the command-line \a arguments, whose first element is the
        /// program. Each \c --plugin-path adds the following directory to the plugin paths.
        /// \c --settings specifies the settings directory. The remaining arguments are files.
        ///
        /// The constructor prepares the application in this order:
        /// 1. Creates the temporary storage of the application (kit::TemporaryStorage).
        /// 2. Reads the settings of the application and of the plugins.
        /// 3. Creates the translation loader (kit::TranslationLoader) in the language of the
        ///    settings and installs the translations of Qt and of the libraries. The plugins add
        ///    theirs when they initialize.
        explicit AppLoader(const QStringList &arguments);

        /// Shuts the plugins down if they are still loaded.
        ~AppLoader();

        /// Returns the existing loader, or \c nullptr if none exists.
        static AppLoader *instance();

        /// Returns the directory of the bundled plugins, at the location relative to the program
        /// where the build places them: <tt>lib/plugins/helloutau</tt> beside the directory of
        /// the program, or <tt>Contents/Plugins</tt> of a macOS bundle.
        static QString builtinPluginPath();

        /// Adds the directories of the Qt plugins of the installation to the library paths of
        /// Qt, ahead of the defaults. Called before the application object is created, because
        /// Qt loads the platform plugin as the object is created, so that no \c qt.conf is
        /// needed. See qtPluginPaths(). Qt searches the directory of the program as well, where
        /// a user may have put the plugin directories of Qt. On macOS Qt finds the plugins of
        /// the bundle itself.
        static void addQtPluginPaths();

        /// Returns the directories of the Qt plugins for the program in \a programDirectory that
        /// exist, the first to be searched first: \c plugins in the directory of the program,
        /// where a user may copy the plugins of Qt, and <tt>lib/plugins/Qt</tt> beside the
        /// plugins of the application, where the package puts them.
        static QStringList qtPluginPaths(const QString &programDirectory);

        /// Returns the plugin paths: builtinPluginPath() followed by the directories of the
        /// command line.
        QStringList pluginPaths() const;

        /// Replaces the plugin paths before load(). Tests use this function to search only
        /// their own plugins.
        void setPluginPaths(const QStringList &paths);

        /// Returns the files specified on the command line, which the core plugin opens.
        QStringList files() const;

        /// Returns the settings directory: AppSettings::defaultDirectory(), or the directory of
        /// \c --settings. The directory holds \c settings.json with the settings of the
        /// application, and \c plugins.json with the settings of the plugins.
        QString settingsDirectory() const;

        /// Returns the settings of the application in \c settings.json, which the core plugin
        /// passes to the editor.
        AppSettings &settings() const;

        /// Returns the editor that the core plugin created, or \c nullptr before the core plugin
        /// initializes and after it shuts down. A plugin, which depends on the core plugin,
        /// registers its actions and setting pages with this editor in initialize().
        Editor *editor() const;

        /// Records the editor of the core plugin, or \c nullptr as the editor is destroyed.
        void setEditor(Editor *editor);

        /// Writes the pending changes of both files immediately. Otherwise the changes are
        /// written once the event loop runs, or at the destruction of the loader.
        void syncSettings();

        /// \name Settings of the plugins
        ///
        /// \c plugins.json holds the \c PluginSettings of stdcorelib.plugin. Its enabled and
        /// disabled plugins record the choices of the user, which load() applies, and its
        /// \c userData records the values of each plugin under the ID of the plugin. A plugin
        /// addresses a value with kit::JsonInterop::valueAt() and a path that starts with its ID,
        /// such as <tt>org.helloutau.classicpluginhost/approved</tt>.
        /// @{

        /// Returns the settings of the plugins.
        const stdc::pluginsystem::PluginSettings &pluginSettings() const;

        /// Updates the settings of the plugins with \a change, and writes the file once the
        /// event loop runs, or at the destruction of the loader. The running plugins are
        /// unaffected, and a changed choice of the user takes effect at the next start.
        void updatePluginSettings(
            const std::function<void(stdc::pluginsystem::PluginSettings &settings)> &change);
        /// @}

        /// Loads the plugins once, with the plugins enabled or disabled by the settings. Errors
        /// of plugins other than the core plugin are logged.
        ///
        /// \return true if the core plugin runs. Otherwise false, with the reason in \a error.
        bool load(QString *error);

        /// Returns the plugin system, whose \c plugins() returns every plugin found in the
        /// plugin paths, in the order of discovery.
        const stdc::pluginsystem::PluginSystem &pluginSystem() const;

        /// Returns the failed plugins other than the core plugin, each as its ID and the reason.
        QStringList errors() const;

        /// Loads the plugins, shows the reason if the core plugin does not run, runs the event
        /// loop, and shuts the plugins down. If a restart is scheduled, starts a new process of
        /// the program with the same options and without the files
        /// (kit::RestartScheduler::relaunchIfScheduled()).
        ///
        /// \return the exit code of the event loop, or 1 if the core plugin does not run
        int run();

        /// Shuts the plugins down and unloads them, in the reverse order of loading. Repeated
        /// calls have no effect.
        void shutdown();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;

        Q_DISABLE_COPY_MOVE(AppLoader)
    };

}

#endif // HELLOUTAU_EDITOR_APPLOADER_H
