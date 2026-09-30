#ifndef HELLOUTAU_INTERCHANGE_INTERCHANGEPLUGIN_H
#define HELLOUTAU_INTERCHANGE_INTERCHANGEPLUGIN_H

#include <memory>
#include <string>

#include <stdcorelib/pluginsystem/iplugin.h>

namespace hello::kit {
    class BuiltinInterchangeDrivers;
    class InterchangeRegistry;
}

namespace hello::daw {

    class ActionRegistration;

    /// The format conversion plugin. Registers the MIDI drivers and adds the commands Import and
    /// Export > Other Formats to the project windows. The commands use every driver registered
    /// in the process. See docs/ImportExport.md.
    class InterchangePlugin : public stdc::pluginsystem::IPlugin {
    public:
        InterchangePlugin();
        ~InterchangePlugin();

        bool initialize(std::string *errorMessage) override;
        void aboutToShutdown() override;

    private:
        std::unique_ptr<kit::BuiltinInterchangeDrivers> m_drivers;
        std::unique_ptr<kit::InterchangeRegistry> m_registry;
        std::unique_ptr<ActionRegistration> m_actions;
    };

}

#endif // HELLOUTAU_INTERCHANGE_INTERCHANGEPLUGIN_H
