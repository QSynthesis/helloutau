#ifndef HELLOKIT_INTERCHANGE_INTERCHANGEWRITERREGISTRY_H
#define HELLOKIT_INTERCHANGE_INTERCHANGEWRITERREGISTRY_H

#include <stdcorelib/adt/linked_map.h>
#include <stdcorelib/support/dynamicregistry.h>

#include <hellokit/Interchange/InterchangeWriter.h>

namespace hello::kit {

    /// The export drivers registered in a host, in the order of registration, as
    /// InterchangeReaderRegistry for the import drivers. A driver that supports both directions
    /// is registered in both registries under the same ID. See the registration interfaces in
    /// docs/Plugins.md.
    using InterchangeWriterRegistry =
        stdc::DynamicRegistry<InterchangeWriter, stdc::dynamic_registry_traits<InterchangeWriter>,
                              stdc::linked_map>;

}

#endif // HELLOKIT_INTERCHANGE_INTERCHANGEWRITERREGISTRY_H
