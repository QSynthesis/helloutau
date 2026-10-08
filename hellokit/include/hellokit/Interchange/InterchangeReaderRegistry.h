#ifndef HELLOKIT_INTERCHANGE_INTERCHANGEREADERREGISTRY_H
#define HELLOKIT_INTERCHANGE_INTERCHANGEREADERREGISTRY_H

#include <stdcorelib/adt/linked_map.h>
#include <stdcorelib/support/dynamicregistry.h>

#include <hellokit/Interchange/InterchangeReader.h>

namespace hello::kit {

    /// The import drivers registered in a host, in the order of registration. See the
    /// registration interfaces in docs/Plugins.md.
    ///
    /// InterchangeDrivers creates and holds the registry and creates one instance of each
    /// driver. The name of an entry is the ID of its driver, and a driver whose ID differs from
    /// the name is rejected. Built-in drivers and plugin drivers are registered alike, with an
    /// \c Add or \c AddFactory object that a plugin creates in initialize() and destroys in
    /// aboutToShutdown(), before its library is unloaded. The registry is used only on the
    /// application thread.
    using InterchangeReaderRegistry =
        stdc::DynamicRegistry<InterchangeReader, stdc::dynamic_registry_traits<InterchangeReader>,
                              stdc::linked_map>;

}

#endif // HELLOKIT_INTERCHANGE_INTERCHANGEREADERREGISTRY_H
