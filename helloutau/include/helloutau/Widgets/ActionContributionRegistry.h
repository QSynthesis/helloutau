#ifndef HELLOUTAU_WIDGETS_ACTIONCONTRIBUTIONREGISTRY_H
#define HELLOUTAU_WIDGETS_ACTIONCONTRIBUTIONREGISTRY_H

#include <stdcorelib/adt/linked_map.h>
#include <stdcorelib/support/dynamicregistry.h>

#include <helloutau/Widgets/ActionContribution.h>

namespace hello::daw {

    /// The action contributions of a host, such as an editor, in the order of registration. See
    /// the registration interfaces in docs/Plugins.md.
    ///
    /// The host creates and holds the registry, creates one instance of each contribution, and
    /// applies the instance to its windows. A plugin registers a contribution with an
    /// \c AddFactory object under the ID of the plugin, creates the object in initialize() and
    /// destroys it in aboutToShutdown(), before its library is unloaded. The registry is used
    /// only on the application thread.
    using ActionContributionRegistry =
        stdc::DynamicRegistry<ActionContribution, stdc::dynamic_registry_traits<ActionContribution>,
                              stdc::linked_map>;

}

#endif // HELLOUTAU_WIDGETS_ACTIONCONTRIBUTIONREGISTRY_H
