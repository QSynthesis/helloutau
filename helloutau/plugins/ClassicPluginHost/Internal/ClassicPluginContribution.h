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

    /// The submenu Plugins of the Tools menu of the project windows, as in UTAU: the plugins of
    /// UTAU that were found, then Refresh and the folders of the plugins.
    ///
    /// The plugins are found the first time the menu opens, again on Refresh, and again when the
    /// UTAU folder of the settings has changed. They are found in the \c plugins folder of UTAU
    /// and in the folder of HelloUtau for them, userDirectory().
    class ClassicPluginContribution : public ActionContribution {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::ClassicPluginContribution)
    public:
        ClassicPluginContribution();
        ~ClassicPluginContribution();

        const QAK::ActionExtension *extension() const override;
        void addActions(ProjectWindow *window, QAK::WidgetActionContext *context) override;

        /// The folder of the plugins of UTAU that the user installs for HelloUtau, apart from
        /// those of UTAU and from the native plugins.
        static std::filesystem::path userDirectory();

    private:
        void fill(QMenu *menu, ProjectWindow *window);
        void refresh(ProjectWindow *window);

        QList<ClassicPlugin> m_plugins;

        // The UTAU folder of the settings when the plugins were last found, absent before
        std::optional<std::filesystem::path> m_utauDirectory;
    };

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINCONTRIBUTION_H
