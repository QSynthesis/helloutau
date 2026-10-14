#include "FrequencyMap.h"

#include <algorithm>
#include <cmath>

namespace hello::kit {

    namespace {

        double noteOf(double frequency) {
            return 69 + 12 * std::log2(frequency / 440);
        }

        double frequencyOf(double note) {
            return 440 * std::exp2((note - 69) / 12);
        }

    }

    // The average is taken on the pitch scale, since pitch is perceived logarithmically, so that
    // the result is the note that the frames center on, and a frame an octave too high weighs as
    // much as a frame an octave too low. 55 Hz is the note A1, below the range of singing voices,
    // and frames at or below it are taken as unvoiced, whichever value a format writes for those.
    // The notes and their mean are taken in integral cents, truncated, a resolution finer than the
    // smallest difference of pitch that a listener perceives. Without voiced frames the result is
    // C4, the note at which FrequencyMapGenerator renders.
    double FrequencyMap::pitchAverage() const {
        qint64 sum = 0;
        qint64 count = 0;
        for (const auto &frame : frames) {
            if (frame.frequency > 55) {
                sum += qint64(noteOf(frame.frequency) * 100);
                ++count;
            }
        }
        if (count == 0) {
            return frequencyOf(60);
        }
        const qint64 cents = sum / count;
        return frequencyOf(double(cents) / 100);
    }

    // The frames are located by time rather than by index, since the frames of some formats,
    // such as dio, record their own times.
    FrequencyMap FrequencyMap::resampled(double hop, int sampleRate, std::size_t count) const {
        FrequencyMap result;
        result.hop = hop;
        result.averageFrequency = averageFrequency;
        const bool amplitudes =
            !frames.empty() && std::all_of(frames.begin(), frames.end(),
                                           [](const Frame &frame) { return frame.amplitude; });
        result.frames.reserve(count);
        // The index of the first frame of this map after the time of the new frame
        std::size_t next = 0;
        for (std::size_t i = 0; i < count; ++i) {
            Frame frame;
            frame.time = double(i) * hop * 1000 / sampleRate;
            while (next < frames.size() && frames[next].time <= frame.time) {
                ++next;
            }
            if (next == 0 || next == frames.size()) {
                if (!frames.empty()) {
                    const auto &end = next == 0 ? frames.front() : frames.back();
                    frame.frequency = end.frequency;
                    frame.amplitude = amplitudes ? end.amplitude : std::nullopt;
                }
            } else {
                const auto &before = frames[next - 1];
                const auto &after = frames[next];
                const double ratio = (frame.time - before.time) / (after.time - before.time);
                if (before.frequency > 0 && after.frequency > 0) {
                    frame.frequency =
                        before.frequency + (after.frequency - before.frequency) * ratio;
                } else {
                    frame.frequency = ratio <= 0.5 ? before.frequency : after.frequency;
                }
                if (amplitudes) {
                    frame.amplitude =
                        *before.amplitude + (*after.amplitude - *before.amplitude) * ratio;
                }
            }
            result.frames.push_back(frame);
        }
        return result;
    }

}
