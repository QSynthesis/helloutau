#ifndef HELLOUTAU_EDITOR_APPLOADER_H
#define HELLOUTAU_EDITOR_APPLOADER_H

#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// Starts the application: loads the native plugins, of which the core plugin creates the
    /// editor and opens its windows, and runs the event loop. The program does nothing else. See
    /// docs/Plugins.md.
    ///
    /// One loader exists at a time, which the plugins reach through instance().
    class HELLOUTAU_EDITOR_EXPORT AppLoader {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::AppLoader)
    public:
        /// The interface ID that every native plugin is built with. The CMake variable
        /// HELLOUTAU_PLUGIN_IID of the plugins of this repository holds the same.
        static constexpr char pluginIid[] = "org.OpenVPI.HelloUtau.Plugin";

        /// The ID of the core plugin, without which the application does not start.
        static constexpr char corePluginId[] = "org.helloutau.core";

        /// The option that adds a directory to search for plugins, followed by the directory.
        static constexpr char pluginPathOption[] = "--plugin-path";

        /// A loader for the command line \a arguments, the first of which is the program. Each
        /// \c --plugin-path adds the directory after it to the plugin paths, and the other
        /// arguments are files.
        explicit AppLoader(const QStringList &arguments);

        /// Shuts the plugins down if they are still loaded.
        ~AppLoader();

        /// The loader that exists, or \c nullptr.
        static AppLoader *instance();

        /// The directory of the plugins that come with the application, where the build puts
        /// them relative to the program: <tt>lib/plugins/helloutau</tt> beside the directory of
        /// the program, or <tt>Contents/Plugins</tt> of a macOS bundle.
        static QString builtinPluginPath();

        /// The directories searched for plugins: builtinPluginPath() and then those of the
        /// command line.
        QStringList pluginPaths() const;

        /// Replaces the directories searched, before load(). Tests use it to search only their
        /// own plugins.
        void setPluginPaths(const QStringList &paths);

        /// The files named on the command line, which the core plugin opens.
        QStringList files() const;

        /// Loads the plugins once. The errors of plugins other than the core plugin are logged.
        ///
        /// \return whether the core plugin runs, or else false with the reason in \a error
        bool load(QString *error);

        /// The plugins other than the core plugin that failed, each as its ID and the reason.
        QStringList errors() const;

        /// Loads the plugins, shows the reason if the core plugin does not run, runs the event
        /// loop, and shuts the plugins down.
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
