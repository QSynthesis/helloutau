#include "SampleRateConversion.h"

#include <cmath>

#include <r8brain-free-src/CDSPResampler.h>

namespace hello::daw {

    namespace {

        // The block in which the resampler takes its input, in samples
        constexpr int resamplerBlock = 4096;

    }

    std::vector<float> SampleRateConversion::converted(const std::vector<float> &samples,
                                                       int channels, int sourceRate,
                                                       int targetRate) {
        Q_ASSERT(sourceRate > 0 && targetRate > 0);
        if (sourceRate == targetRate || channels < 1 || samples.empty()) {
            return samples;
        }
        const qsizetype frames = qsizetype(samples.size()) / channels;
        const auto outFrames = qsizetype(std::llround(double(frames) * targetRate / sourceRate));
        std::vector<float> result(size_t(outFrames * channels));

        std::vector<float> input(static_cast<size_t>(frames));
        std::vector<float> output(static_cast<size_t>(outFrames));
        r8b::CDSPResampler24 resampler(sourceRate, targetRate, resamplerBlock);
        for (int channel = 0; channel < channels; ++channel) {
            for (qsizetype i = 0; i < frames; ++i) {
                input[size_t(i)] = samples[size_t(i * channels + channel)];
            }
            resampler.oneshot(input.data(), int(frames), output.data(), int(outFrames));
            for (qsizetype i = 0; i < outFrames; ++i) {
                result[size_t(i * channels + channel)] = output[size_t(i)];
            }
        }
        return result;
    }

}
