#ifndef HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINCONTRIBUTION_H
#define HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINCONTRIBUTION_H

#include <filesystem>
#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QList>

#include <helloutau/Editor/ActionContribution.h>

#include <ClassicPluginHost/ClassicPlugin.h>

class QMenu;

namespace hello::daw {

    class AppSettings;

    /// The submenu Classic Plugins of the Tools menu of the project windows, as the plugin menu
    /// of UTAU: the discovered UTAU plugins, followed by Refresh and the commands that open the
    /// plugin folders. The command Classic Plugins at Pointer (key N, as in UTAU) shows the same
    /// menu at the mouse pointer. That command is in no menu.
    ///
    /// The plugins are discovered when the menu first opens, on Refresh, and after the UTAU
    /// folder in the settings has changed. The discovery covers the HelloUtau folder for UTAU
    /// plugins, userDirectory(), and then the \c plugins folder of UTAU.
    class ClassicPluginContribution : public ActionContribution {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::ClassicPluginContribution)
    public:
        ClassicPluginContribution();
        ~ClassicPluginContribution();

        const QAK::ActionExtension *extension(Editor::WindowKind kind) const override;
        void addActions(ProjectWindow *window, QAK::WidgetActionContext *context) override;

        /// Returns the folder of the UTAU plugins that the user installs for HelloUtau, separate
        /// from the \c plugins folder of UTAU and from the native plugins.
        static std::filesystem::path userDirectory();

        /// Returns the plugin folders in the order of discovery: userDirectory(), and then the
        /// \c plugins folder of the UTAU folder of \a settings if that is set.
        static QList<std::filesystem::path> pluginFolders(const AppSettings &settings);

        /// Opens \a folder in the file manager, and creates the folder if necessary.
        static void openFolder(const std::filesystem::path &folder);

    private:
        void fill(QMenu *menu, ProjectWindow *window);
        void refresh(ProjectWindow *window);

        QList<ClassicPlugin> m_plugins;

        // The UTAU folder in the settings at the last discovery, or \c std::nullopt before the
        // first discovery
        std::optional<std::filesystem::path> m_utauDirectory;
    };

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINCONTRIBUTION_H
