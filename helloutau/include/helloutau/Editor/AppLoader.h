#ifndef HELLOUTAU_EDITOR_APPLOADER_H
#define HELLOUTAU_EDITOR_APPLOADER_H

#include <memory>
#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonValue>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    class AppSettings;

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
        /// in place of the directory of the user.
        static constexpr char settingsOption[] = "--settings";

        /// Information about a plugin found by the loader, as displayed on the Plugins page of
        /// the settings.
        struct PluginInfo {
            /// State of the plugin in this run.
            enum State {
                /// Loaded and running.
                Running,
                /// Disabled by its metadata or by the settings.
                Disabled,
                /// Reading, resolution, loading or initialization failed. \c error holds the
                /// reason.
                Failed,
                /// Neither running nor failed, for example after shutdown().
                NotLoaded,
            };

            /// A dependency of the plugin.
            struct Dependency {
                QString id;
                /// Whether the plugin runs without the dependency.
                bool optional = false;
            };

            QString id;
            QString displayName;
            QString description;
            QString version;
            /// Path of the library of the plugin.
            QString filePath;
            QList<Dependency> dependencies;
            State state = NotLoaded;
            QString error;
            /// Whether the metadata enables the plugin, without the settings of the user.
            bool enabledByDefault = false;
            /// Whether the plugin was enabled in this run, according to the settings of the user
            /// at the start of load().
            bool enabled = false;
        };

        /// Creates a loader for the command-line \a arguments, whose first element is the
        /// program. Each \c --plugin-path adds the following directory to the plugin paths.
        /// \c --settings specifies the settings directory. The remaining arguments are files.
        /// Installs the translations of the language in the settings (Translations::install()).
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

        /// Writes the pending changes of both files immediately. Otherwise the changes are
        /// written once the event loop runs, or at the destruction of the loader.
        void syncSettings();

        /// \name Settings of the plugins
        ///
        /// \c plugins.json uses the format that \c PluginSettings of stdcorelib.plugin reads.
        /// \c enabledPlugins and \c disabledPlugins record the IDs of the plugins that the user
        /// enabled or disabled, which load() applies. \c userData records the values of each
        /// plugin under its ID. A key is the path of a value in the groups of a plugin, as for
        /// AppSettings::value().
        /// @{

        /// Returns the value of the plugin \a id at \a key, or an undefined value if absent.
        QJsonValue pluginValue(const QString &id, const QString &key) const;

        /// Replaces the value of the plugin \a id at \a key, and writes the file once the event
        /// loop runs, or at the destruction of the loader. An undefined or null \a value removes
        /// the entry.
        void setPluginValue(const QString &id, const QString &key, const QJsonValue &value);

        /// Returns whether the user enabled (true) or disabled (false) the plugin \a id, or
        /// \c std::nullopt if the metadata of the plugin applies.
        std::optional<bool> pluginEnabled(const QString &id) const;

        /// Replaces the choice of the user for the plugin \a id, and writes the file as
        /// setPluginValue() does. \c std::nullopt removes the choice so that the metadata
        /// applies. The running plugins are unaffected. The choice takes effect at the next start.
        void setPluginEnabled(const QString &id, std::optional<bool> enabled);
        /// @}

        /// Loads the plugins once, with the plugins enabled or disabled by the settings. Errors
        /// of plugins other than the core plugin are logged.
        ///
        /// \return true if the core plugin runs. Otherwise false, with the reason in \a error.
        bool load(QString *error);

        /// Returns every plugin found in the plugin paths, in the order of discovery.
        QList<PluginInfo> plugins() const;

        /// Returns the failed plugins other than the core plugin, each as its ID and the reason.
        QStringList errors() const;

        /// Loads the plugins, shows the reason if the core plugin does not run, runs the event
        /// loop, and shuts the plugins down. If the application quit to restart, starts it again
        /// with the same options and without the files (Restarter::startAgain()).
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
