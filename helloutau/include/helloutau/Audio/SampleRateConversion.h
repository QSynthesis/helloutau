#ifndef HELLOUTAU_AUDIO_SAMPLERATECONVERSION_H
#define HELLOUTAU_AUDIO_SAMPLERATECONVERSION_H

#include <vector>

#include <helloutau/Audio/HelloUtauAudioGlobal.h>

namespace hello::daw {

    /// Sample rate conversion with r8brain-free-src.
    class HELLOUTAU_AUDIO_EXPORT SampleRateConversion {
    public:
        /// Converts \a samples, interleaved by \a channels, from \a sourceRate to \a targetRate,
        /// each channel separately. The result has the same duration, rounded to whole frames.
        static std::vector<float> converted(const std::vector<float> &samples, int channels,
                                            int sourceRate, int targetRate);
    };

}

#endif // HELLOUTAU_AUDIO_SAMPLERATECONVERSION_H
