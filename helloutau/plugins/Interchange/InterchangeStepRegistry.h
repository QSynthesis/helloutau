#ifndef HELLOUTAU_INTERCHANGE_INTERCHANGESTEPREGISTRY_H
#define HELLOUTAU_INTERCHANGE_INTERCHANGESTEPREGISTRY_H

#include <stdcorelib/support/dynamicregistry.h>

#include <Interchange/InterchangeStepPage.h>

namespace hello::daw {

    /// The custom step pages, each registered under the ID that an import driver returns from
    /// \c customStepId(). See the custom selection steps in docs/Interchange.md.
    ///
    /// InterchangeService creates and holds the registry. Unlike the other registries of the
    /// application, an entry is instantiated for each import: the import wizard creates a new
    /// page from the entry of the step ID of its driver and owns the page. A plugin registers a
    /// page with an \c AddFactory object, creates the object in initialize() and destroys it in
    /// aboutToShutdown(), before its library is unloaded. The registry is used only on the
    /// application thread.
    using InterchangeStepRegistry = stdc::DynamicRegistry<InterchangeStepPage>;

}

#endif // HELLOUTAU_INTERCHANGE_INTERCHANGESTEPREGISTRY_H
