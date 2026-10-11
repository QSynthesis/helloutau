#ifndef HELLOKIT_SYNTH_SYNTHDIAGNOSERREGISTRY_H
#define HELLOKIT_SYNTH_SYNTHDIAGNOSERREGISTRY_H

#include <stdcorelib/adt/linked_map.h>
#include <stdcorelib/support/dynamicregistry.h>

#include <hellokit/Synth/SynthDiagnoser.h>

namespace hello::kit {

    /// The diagnosers of render failures registered in a host, in the order of registration. See
    /// the registration interfaces in docs/Plugins.md.
    ///
    /// SynthDiagnosers creates and holds the registry and creates one instance of each
    /// diagnoser. The name of an entry is the ID of its diagnoser, and a diagnoser whose ID
    /// differs from the name is rejected. A plugin creates its \c Add or \c AddFactory objects in
    /// initialize() and destroys them in aboutToShutdown(), before its library is unloaded. The
    /// registry is used only on the application thread.
    using SynthDiagnoserRegistry =
        stdc::DynamicRegistry<SynthDiagnoser, stdc::dynamic_registry_traits<SynthDiagnoser>,
                              stdc::linked_map>;

}

#endif // HELLOKIT_SYNTH_SYNTHDIAGNOSERREGISTRY_H
