#ifndef HELLOUTAU_INTERCHANGE_INTERCHANGEPLUGIN_H
#define HELLOUTAU_INTERCHANGE_INTERCHANGEPLUGIN_H

#include <memory>
#include <string>

#include <stdcorelib/pluginsystem/iplugin.h>

#include <helloutau/Widgets/ActionContributionRegistry.h>

#include <Interchange/InterchangeStepRegistry.h>

namespace hello::kit {
    class BuiltinInterchangeDrivers;
}

namespace hello::daw {

    class InterchangeService;

    /// The format conversion plugin. Creates the InterchangeService, registers the MIDI drivers
    /// and the MIDI encoding page in it, and adds the commands Import and Export > Other Formats
    /// to the project windows. The commands use every driver registered in the service. See
    /// docs/ImportExport.md.
    class InterchangePlugin : public stdc::pluginsystem::IPlugin {
    public:
        InterchangePlugin();
        ~InterchangePlugin();

        bool initialize(std::string *errorMessage) override;
        void aboutToShutdown() override;

    private:
        std::unique_ptr<InterchangeService> m_service;
        std::unique_ptr<kit::BuiltinInterchangeDrivers> m_drivers;
        InterchangeStepRegistry::AddFactory m_midiEncoding;
        ActionContributionRegistry::AddFactory m_actions;
    };

}

#endif // HELLOUTAU_INTERCHANGE_INTERCHANGEPLUGIN_H
