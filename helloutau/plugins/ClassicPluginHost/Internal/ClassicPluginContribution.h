#ifndef HELLOUTAU_CLASSICPLUGINHOST_INTERNAL_CLASSICPLUGINCONTRIBUTION_H
#define HELLOUTAU_CLASSICPLUGINHOST_INTERNAL_CLASSICPLUGINCONTRIBUTION_H

#include <array>
#include <filesystem>
#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QList>

#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Widgets/ActionContribution.h>

#include <ClassicPluginHost/ClassicPlugin.h>

class QMenu;

namespace hello::daw {

    class ProjectWindow;

    /// The submenu Classic Plugins of the Tools menu of the project windows, as the plugin menu
    /// of UTAU: the discovered UTAU plugins, followed by Refresh and the commands that open the
    /// plugin folders. The command Classic Plugins at Pointer (key N, as in UTAU) shows the same
    /// menu at the mouse pointer. That command is in no menu.
    ///
    /// The plugins are discovered when the menu first opens, on Refresh, after the UTAU folder in
    /// the settings has changed, and after the modification time of a plugin folder has changed,
    /// which a plugin added to or removed from the folder changes, such as by an installation
    /// archive. The discovery covers the folders of pluginFolders().
    class ClassicPluginContribution : public ActionContribution {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::ClassicPluginContribution)
    public:
        ClassicPluginContribution();
        ~ClassicPluginContribution();

        const QAK::ActionExtension *extension(const QString &windowKind) const override;
        void addActions(QWidget *widget, QAK::WidgetActionContext *context) override;

        /// Returns the folder of the UTAU plugins that the user installs for HelloUtau, the
        /// \c plugins directory of the classic extensions directory of \a settings. It is
        /// separate from the \c plugins folder of UTAU and from the native plugins.
        static std::filesystem::path userDirectory(const AppSettings &settings);

        /// Returns the plugin folders in the order of discovery: userDirectory(), the \c plugins
        /// directory of UtauSettings::dataDirectory() if that is not empty, and then the
        /// \c plugins folder of the UTAU folder of \a settings if that is set.
        static QList<std::filesystem::path> pluginFolders(const AppSettings &settings);

        /// Opens \a folder in the file manager. The folder is created if necessary only if it is
        /// userDirectory() of \a settings, because the other folders belong to UTAU. A missing
        /// folder of UTAU is reported in a message box over \a parent.
        static void openFolder(QWidget *parent, const AppSettings &settings,
                               const std::filesystem::path &folder);

    private:
        // The modification time of the plugins directory of each root of
        // AppSettings::classicRoot(), indexed by the root
        using FolderTimes = std::array<std::optional<std::filesystem::file_time_type>,
                                       AppSettings::ClassicRootCount>;

        // Returns the modification time of each plugin folder, std::nullopt for a folder that
        // does not apply or cannot be read.
        static FolderTimes modificationTimesOf(const AppSettings &settings);

        void fill(QMenu *menu, ProjectWindow *window);
        void refresh(ProjectWindow *window);

        QList<ClassicPlugin> m_plugins;
        // The folders and plugins that the last discovery could not read
        kit::DiagnosticList m_diagnostics;

        // The UTAU folder in the settings at the last discovery, or \c std::nullopt before the
        // first discovery
        std::optional<std::filesystem::path> m_utauDirectory;
        // The modification times of the plugin folders at the last discovery
        FolderTimes m_folderTimes;
    };

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_INTERNAL_CLASSICPLUGINCONTRIBUTION_H
