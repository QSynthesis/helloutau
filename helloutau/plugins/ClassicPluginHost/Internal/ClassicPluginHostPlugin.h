#ifndef HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINHOSTPLUGIN_H
#define HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINHOSTPLUGIN_H

#include <string>

#include <stdcorelib/pluginsystem/iplugin.h>

#include <helloutau/Widgets/ActionContributionRegistry.h>
#include <helloutau/Widgets/SettingPageRegistry.h>

namespace hello::daw {

    /// The plugin that runs UTAU plugins on the selection of a project window from the submenu
    /// Plugins of the Tools menu. See docs/ClassicPluginHost.md.
    class ClassicPluginHostPlugin : public stdc::pluginsystem::IPlugin {
    public:
        ClassicPluginHostPlugin();
        ~ClassicPluginHostPlugin();

        bool initialize(std::string *errorMessage) override;
        void aboutToShutdown() override;

    private:
        ActionContributionRegistry::AddFactory m_registration;
        SettingPageRegistry::AddFactory m_settingPage;
    };

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINHOSTPLUGIN_H
