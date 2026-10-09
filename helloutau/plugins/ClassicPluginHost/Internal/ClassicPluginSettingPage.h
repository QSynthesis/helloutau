#ifndef HELLOUTAU_CLASSICPLUGINHOST_INTERNAL_CLASSICPLUGINSETTINGPAGE_H
#define HELLOUTAU_CLASSICPLUGINHOST_INTERNAL_CLASSICPLUGINSETTINGPAGE_H

#include <helloutau/Widgets/SettingPage.h>

namespace hello::daw {

    class AppSettings;

    /// The setting page Classic Plugins: the plugin folders in which the classic plugins are
    /// discovered, in the order of discovery, each with a button that opens the folder. The
    /// folders follow from the UTAU folder of the System Settings page and are not edited here.
    class ClassicPluginSettingPage : public SettingPage {
        Q_OBJECT
    public:
        static constexpr char pageId[] = "classicpluginhost.ClassicPlugins";

        explicit ClassicPluginSettingPage(AppSettings &settings, QObject *parent = nullptr);

    protected:
        QWidget *createWidget() override;

    private:
        AppSettings &m_settings;
    };

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_INTERNAL_CLASSICPLUGINSETTINGPAGE_H
