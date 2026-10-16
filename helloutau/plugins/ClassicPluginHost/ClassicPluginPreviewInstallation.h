#ifndef HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINPREVIEWINSTALLATION_H
#define HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINPREVIEWINSTALLATION_H

#include <filesystem>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>

#include <hellokit/Support/Diagnostic.h>

#include <ClassicPluginHost/ClassicPluginHostPluginGlobal.h>

namespace hello::daw {

    /// The preview program of a classic plugin: hello-plugin-play, deployed in the folder of the
    /// plugin under the name playinplugin.exe, which some plugins start for a preview. See the
    /// section on deployment in docs/PluginPlay.md. The file names have no .exe on systems other
    /// than Windows.
    ///
    /// An installation keeps the playinplugin.exe of the plugin as playinplugin-original.exe,
    /// which hello-plugin-play runs if HelloUtau does not run the plugin. It is recorded at
    /// <tt>hostData/ClassicPluginHost/playinplugin</tt> of the plugin.json of the plugin
    /// (docs/note.md). A plugin with only a plugin.txt receives a plugin.json transcribed from
    /// it, in an encoding that the user confirms, because a plugin.json replaces plugin.txt. An
    /// installation either succeeds or leaves the folder as it was.
    class CLASSICPLUGINHOST_EXPORT ClassicPluginPreviewInstallation {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::ClassicPluginPreviewInstallation)
    public:
        enum State {
            Absent,    ///< no installation is recorded
            Installed, ///< the program in the folder is \a program
            Outdated,  ///< the program in the folder differs from \a program
        };

        /// The file name under which a plugin starts the preview program
        static const QString programName;

        /// The file name under which the program of the plugin is kept
        static const QString originalName;

        /// The file name of hello-plugin-play beside the application
        static const QString bundledName;

        /// Returns the hello-plugin-play beside the application, or an empty path if it is
        /// absent.
        static std::filesystem::path bundledProgram();

        /// Returns the state of the installation in \a folder , compared with \a program .
        static State stateOf(const std::filesystem::path &folder,
                             const std::filesystem::path &program);

        /// Returns whether the installation of \a program in \a folder is recommended, which the
        /// button Install All installs. Only a plugin that comes with a playinplugin.exe starts
        /// the program for a preview, so the recommendations are limited to these plugins:
        /// - the installation, if the folder has a playinplugin.exe of the plugin and no
        ///   installation;
        /// - the update, if the installation is Outdated and kept the program of the plugin.
        static bool isRecommended(const std::filesystem::path &folder,
                                  const std::filesystem::path &program);

        /// Returns whether an installation in \a folder transcribes plugin.txt into a new
        /// plugin.json, because the folder has none.
        static bool transcribes(const std::filesystem::path &folder);

        /// Installs \a program in \a folder . \a charset is the encoding of plugin.txt if
        /// transcribes() holds.
        ///
        /// \return whether the installation succeeded. Otherwise \a diagnostics receives the
        ///         reason, and the folder is unchanged.
        static bool install(const std::filesystem::path &folder,
                            const std::filesystem::path &program, const QString &charset,
                            kit::DiagnosticList &diagnostics);

        /// Replaces the program of the installation in \a folder with \a program .
        ///
        /// \return whether the replacement succeeded. Otherwise \a diagnostics receives the reason.
        static bool update(const std::filesystem::path &folder,
                           const std::filesystem::path &program, kit::DiagnosticList &diagnostics);

        /// Removes the installation in \a folder and restores the program of the plugin. Only the
        /// record is removed from plugin.json. A plugin.json that the installation transcribed is
        /// kept, and reads as the plugin.txt.
        ///
        /// \return whether the removal succeeded. Otherwise \a diagnostics receives the reason.
        static bool remove(const std::filesystem::path &folder, kit::DiagnosticList &diagnostics);
    };

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINPREVIEWINSTALLATION_H
