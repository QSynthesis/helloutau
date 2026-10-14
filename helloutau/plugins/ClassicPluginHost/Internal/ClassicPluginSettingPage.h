#ifndef HELLOUTAU_CLASSICPLUGINHOST_INTERNAL_CLASSICPLUGINSETTINGPAGE_H
#define HELLOUTAU_CLASSICPLUGINHOST_INTERNAL_CLASSICPLUGINSETTINGPAGE_H

#include <helloutau/Widgets/SettingPage.h>

namespace hello::daw {

    class AppSettings;

    /// The setting page Classic Plugins:
    ///
    /// - The plugin folders in which the classic plugins are discovered, in the order of
    ///   discovery, each with a button that opens the folder. The folders follow from the UTAU
    ///   folder of the UTAU page and are not edited here.
    /// - The preview program of each discovered plugin, with buttons that install, update or
    ///   remove it (ClassicPluginPreviewInstallation, docs/PluginPlay.md).
    ///
    /// Both lists follow an applied change of the UTAU folder while the dialog is open.
    class ClassicPluginSettingPage : public SettingPage {
        Q_OBJECT
    public:
        static constexpr char pageId[] = "classicpluginhost.ClassicPlugins";

        explicit ClassicPluginSettingPage(AppSettings &settings, QObject *parent = nullptr);

        void settingsApplied() override;

    protected:
        QWidget *createWidget() override;

    private:
        AppSettings &m_settings;
        // The rows of the folders and of the preview programs, owned by the widget
        QPointer<QWidget> m_folders;
        QPointer<QWidget> m_previews;

        void fillFolders();
        void fillPreviews();
    };

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_INTERNAL_CLASSICPLUGINSETTINGPAGE_H
