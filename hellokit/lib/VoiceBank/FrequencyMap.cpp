#include "FrequencyMap.h"

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

}
