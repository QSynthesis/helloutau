#ifndef HELLOUTAU_INTERCHANGE_INTERCHANGEPLUGIN_H
#define HELLOUTAU_INTERCHANGE_INTERCHANGEPLUGIN_H

#include <memory>
#include <string>

#include <stdcorelib/pluginsystem/iplugin.h>

namespace hello::kit {
    class BuiltinInterchangeDrivers;
}

namespace hello::daw {

    /// The plugin of format conversion: registers the drivers of MIDI, and later the import and
    /// export in the menus, their dialogs and the custom steps of the drivers. See
    /// docs/Interchange.md.
    class InterchangePlugin : public stdc::pluginsystem::IPlugin {
    public:
        InterchangePlugin();
        ~InterchangePlugin();

        bool initialize(std::string *errorMessage) override;
        void aboutToShutdown() override;

    private:
        std::unique_ptr<kit::BuiltinInterchangeDrivers> m_drivers;
    };

}

#endif // HELLOUTAU_INTERCHANGE_INTERCHANGEPLUGIN_H
