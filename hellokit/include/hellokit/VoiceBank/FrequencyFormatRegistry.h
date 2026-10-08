#ifndef HELLOKIT_VOICEBANK_FREQUENCYFORMATREGISTRY_H
#define HELLOKIT_VOICEBANK_FREQUENCYFORMATREGISTRY_H

#include <stdcorelib/adt/linked_map.h>
#include <stdcorelib/support/dynamicregistry.h>

#include <hellokit/VoiceBank/FrequencyFormat.h>

namespace hello::kit {

    /// The frequency table formats registered in a host, in the order of registration. See the
    /// registration interfaces in docs/Plugins.md.
    ///
    /// FrequencyFormats creates and holds the registry and creates one instance of each format.
    /// The name of an entry is the ID of its format, and a format whose ID differs from the name
    /// is rejected. Built-in formats and plugin formats are registered alike, with an \c Add or
    /// \c AddFactory object that a plugin creates in initialize() and destroys in
    /// aboutToShutdown(), before its library is unloaded. The registry is used only on the
    /// application thread.
    using FrequencyFormatRegistry =
        stdc::DynamicRegistry<FrequencyFormat, stdc::dynamic_registry_traits<FrequencyFormat>,
                              stdc::linked_map>;

}

#endif // HELLOKIT_VOICEBANK_FREQUENCYFORMATREGISTRY_H
