#ifndef HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINHOSTPLUGIN_H
#define HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINHOSTPLUGIN_H

#include <string>

#include <stdcorelib/pluginsystem/iplugin.h>

namespace hello::daw {

    /// The plugin that runs the plugins of UTAU on the selection of a project window. See
    /// docs/ClassicPluginHost.md.
    class ClassicPluginHostPlugin : public stdc::pluginsystem::IPlugin {
    public:
        ClassicPluginHostPlugin();
        ~ClassicPluginHostPlugin();

        bool initialize(std::string *errorMessage) override;
        void aboutToShutdown() override;
    };

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINHOSTPLUGIN_H
