#ifndef HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINHOSTPLUGIN_H
#define HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINHOSTPLUGIN_H

#include <memory>
#include <string>

#include <stdcorelib/pluginsystem/iplugin.h>

namespace hello::daw {

    class ActionRegistration;
    class SettingPageRegistration;

    /// The plugin that runs UTAU plugins on the selection of a project window from the submenu
    /// Plugins of the Tools menu. See docs/ClassicPluginHost.md.
    class ClassicPluginHostPlugin : public stdc::pluginsystem::IPlugin {
    public:
        ClassicPluginHostPlugin();
        ~ClassicPluginHostPlugin();

        bool initialize(std::string *errorMessage) override;
        void aboutToShutdown() override;

    private:
        std::unique_ptr<ActionRegistration> m_registration;
        std::unique_ptr<SettingPageRegistration> m_settingPage;
    };

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINHOSTPLUGIN_H
