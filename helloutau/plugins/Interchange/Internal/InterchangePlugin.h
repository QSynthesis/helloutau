#ifndef HELLOUTAU_INTERCHANGE_INTERNAL_INTERCHANGEPLUGIN_H
#define HELLOUTAU_INTERCHANGE_INTERNAL_INTERCHANGEPLUGIN_H

#include <optional>
#include <string>

#include <stdcorelib/pluginsystem/iplugin.h>

#include <hellokit/Interchange/BuiltinInterchangeDrivers.h>

#include <helloutau/Widgets/ActionContributionRegistry.h>

#include <Interchange/InterchangeService.h>
#include <Interchange/InterchangeStepRegistry.h>

namespace hello::daw {

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
        std::optional<InterchangeService> m_service;
        std::optional<kit::BuiltinInterchangeDrivers> m_drivers;
        InterchangeStepRegistry::AddFactory m_midiEncoding;
        ActionContributionRegistry::AddFactory m_actions;
    };

}

#endif // HELLOUTAU_INTERCHANGE_INTERNAL_INTERCHANGEPLUGIN_H
