#ifndef HELLOUTAU_INTERCHANGE_INTERCHANGESERVICE_H
#define HELLOUTAU_INTERCHANGE_INTERCHANGESERVICE_H

#include <memory>

#include <QtCore/QtGlobal>

#include <Interchange/InterchangePluginGlobal.h>
#include <Interchange/InterchangeStepRegistry.h>

namespace hello::kit {
    class InterchangeDrivers;
}

namespace hello::daw {

    /// The interface object of the Interchange plugin, through which other plugins register
    /// import drivers, export drivers and custom step pages. See docs/Interchange.md.
    ///
    /// The Interchange plugin creates the service in initialize() and destroys it in
    /// aboutToShutdown(). A plugin that provides a format depends on the Interchange plugin and
    /// obtains the service from instance() in its initialize(). A test creates a service of its
    /// own. The service is used only on the application thread.
    class INTERCHANGEPLUGIN_EXPORT InterchangeService {
    public:
        /// Constructs a service, which becomes instance() if no other service exists.
        InterchangeService();
        ~InterchangeService();

        /// Returns the service of the running Interchange plugin, or null if no service exists.
        static InterchangeService *instance();

        /// Returns the import and export drivers, in whose registries the drivers are
        /// registered.
        kit::InterchangeDrivers &drivers() const;

        /// Returns the registry of the custom step pages.
        InterchangeStepRegistry &stepPages() const;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;

        Q_DISABLE_COPY_MOVE(InterchangeService)
    };

}

#endif // HELLOUTAU_INTERCHANGE_INTERCHANGESERVICE_H
