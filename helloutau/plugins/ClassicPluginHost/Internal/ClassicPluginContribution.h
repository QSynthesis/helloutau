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

    /// The submenu Plugins of the Tools menu of the project windows, as in UTAU: the discovered
    /// UTAU plugins, followed by Refresh and the commands that open the plugin folders.
    ///
    /// The plugins are discovered when the menu first opens, on Refresh, and after the UTAU
    /// folder in the settings has changed. The discovery covers the \c plugins folder of UTAU
    /// and the HelloUtau folder for UTAU plugins, userDirectory().
    class ClassicPluginContribution : public ActionContribution {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::ClassicPluginContribution)
    public:
        ClassicPluginContribution();
        ~ClassicPluginContribution();

        const QAK::ActionExtension *extension() const override;
        void addActions(ProjectWindow *window, QAK::WidgetActionContext *context) override;

        /// Returns the folder of the UTAU plugins that the user installs for HelloUtau, separate
        /// from the \c plugins folder of UTAU and from the native plugins.
        static std::filesystem::path userDirectory();

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
