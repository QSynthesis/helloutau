#ifndef HELLOKIT_VOICEBANK_BUILTINFREQUENCYFORMATS_P_H
#define HELLOKIT_VOICEBANK_BUILTINFREQUENCYFORMATS_P_H

#include <memory>
#include <vector>

#include "FrequencyFormat.h"

namespace hello::kit {

    /// Creates the formats that FrequencyFormatRegistry::addBuiltinFormats() adds, in the order
    /// in which they are matched against a resampler: frq, dio, mrq.
    std::vector<std::unique_ptr<FrequencyFormat>> builtinFrequencyFormats();

}

#endif // HELLOKIT_VOICEBANK_BUILTINFREQUENCYFORMATS_P_H
