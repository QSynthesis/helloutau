#include "FrequencyMapCheck.h"

#include <algorithm>
#include <cmath>

namespace hello::kit {

    namespace {

        using Correction = FrequencyMapCheck::Correction;
        using Settings = FrequencyMapCheck::Settings;

        // The frequency up to which a frame is unvoiced
        constexpr double unvoicedFrequency = 55;

        // The multiples of the frequency of a frame outside the range, of which the one nearest
        // to the target frequency is proposed. 1 is absent, so that the frame always moves by an
        // octave or a fifth.
        constexpr double factors[] = {4.0, 3.0, 2.0, 1.5, 2.0 / 3.0, 0.5, 1.0 / 3.0, 0.25};

        // A frame of the check, indexed by int, because the runs reach one frame before and
        // after themselves
        struct Frame {
            bool unvoiced = false;
            double frequency = 0;
            Correction correction;
        };

        struct IntRun {
            int first = 0;
            int last = 0;
        };

        // Returns the amplitude of frame, 0 if the format records none.
        double amplitudeOf(const FrequencyMap::Frame &frame) {
            return frame.amplitude.value_or(0);
        }

        // The expressions below keep one order of operations each, because another order
        // changes the last bit of a proposed frequency.

        double frequencyOf(double note) {
            return std::pow(2.0, (note - 69.0) / 12.0) * 440.0;
        }

        double noteOf(double frequency) {
            return std::log(frequency / 440.0) * 12.0 / std::log(2.0) + 69.0;
        }

        // Returns the note of the frame around a run: its proposed frequency if it is outside
        // the range and has one, else its frequency if it is voiced, else the note of the
        // settings.
        double neighborNote(const Settings &settings, const Frame &frame) {
            double frequency;
            if (frame.correction.reason != FrequencyMapCheck::UnvoicedFrame &&
                frame.correction.frequency > 0) {
                frequency = frame.correction.frequency;
            } else if (frame.frequency > unvoicedFrequency) {
                frequency = frame.frequency;
            } else {
                frequency = frequencyOf(settings.note);
            }
            return noteOf(frequency);
        }

        // Returns the maximal runs in [first, last] of the frames with reason.
        std::vector<IntRun> runsOf(const std::vector<Frame> &frames, int first, int last,
                                   FrequencyMapCheck::Reason reason) {
            std::vector<IntRun> runs;
            int start = 0;
            bool previous = false;
            for (int i = first; i <= last; ++i) {
                const bool current = frames[size_t(i)].correction.reason == reason;
                if (current && !previous) {
                    start = i;
                } else if (!current && previous) {
                    runs.push_back({start, i - 1});
                }
                previous = current;
            }
            if (previous) {
                runs.push_back({start, last});
            }
            return runs;
        }

        // Returns the frequency of frame i of run on the line from the note left to the note
        // right, which lie one frame before and one frame after the run.
        double interpolated(const IntRun &run, int i, double left, double right) {
            const double step = (right - left) / double(run.last - run.first + 2);
            return frequencyOf(double(i - run.first + 1) * step + left);
        }

        // Proposes for each frame of run, which is outside the range, the multiple of its
        // frequency nearest to the interpolated frequency. A run at an end of the segment takes
        // the note of the settings at that end.
        void correctOutOfRange(const Settings &settings, std::vector<Frame> &frames, int first,
                               int last, const IntRun &run) {
            const double left = run.first > first
                                    ? neighborNote(settings, frames[size_t(run.first - 1)])
                                    : double(settings.note);
            const double right = run.last < last
                                     ? neighborNote(settings, frames[size_t(run.last + 1)])
                                     : double(settings.note);
            for (int i = run.first; i <= run.last; ++i) {
                const double target = interpolated(run, i, left, right);
                auto &frame = frames[size_t(i)];
                double best = 99999999.0;
                double chosen = 0;
                for (const double factor : factors) {
                    const double candidate = factor * frame.frequency;
                    const double distance = std::fabs(target - candidate);
                    if (best > distance) {
                        best = distance;
                        chosen = candidate;
                    }
                }
                frame.correction.frequency = chosen;
            }
        }

        void checkRange(const Settings &settings, std::vector<Frame> &frames, int first, int last) {
            const double margin = settings.allowsPitchChange ? 10 : 5;
            const double upper =
                std::pow(2.0, (double(settings.note) + margin - 69.0) / 12.0) * 440.0;
            const double lower =
                std::pow(2.0, (double(settings.note) - margin - 69.0) / 12.0) * 440.0;
            for (int i = first; i <= last; ++i) {
                auto &frame = frames[size_t(i)];
                if (!frame.unvoiced && (lower > frame.frequency || frame.frequency > upper)) {
                    frame.correction = {FrequencyMapCheck::OutOfRange, -1};
                }
            }
            for (const auto &run : runsOf(frames, first, last, FrequencyMapCheck::OutOfRange)) {
                correctOutOfRange(settings, frames, first, last, run);
            }
        }

        // Drops the corrections from the first voiced frame in the direction of step up to the
        // first frame without a correction.
        void clearFromEnd(std::vector<Frame> &frames, int first, int last, int step) {
            int i = step > 0 ? first : last;
            while (i >= first && i <= last && frames[size_t(i)].unvoiced) {
                i += step;
            }
            for (; i >= first && i <= last; i += step) {
                auto &frame = frames[size_t(i)];
                if (frame.correction.reason == FrequencyMapCheck::NoCorrection) {
                    return;
                }
                frame.correction = {};
            }
        }

        // Proposes interpolated frequencies for the unvoiced frames between the first and the
        // last voiced frame. A segment without a voiced frame takes the note of the settings
        // throughout.
        void fillUnvoiced(const Settings &settings, std::vector<Frame> &frames, int first,
                          int last) {
            int voicedFirst = first;
            while (voicedFirst <= last && frames[size_t(voicedFirst)].unvoiced) {
                ++voicedFirst;
            }
            if (voicedFirst > last) {
                const double frequency = frequencyOf(settings.note);
                for (int i = first; i <= last; ++i) {
                    frames[size_t(i)].correction = {FrequencyMapCheck::UnvoicedFrame, frequency};
                }
                return;
            }
            int voicedLast = last;
            while (voicedLast >= voicedFirst && frames[size_t(voicedLast)].unvoiced) {
                --voicedLast;
            }
            for (int i = voicedFirst; i <= voicedLast; ++i) {
                if (frames[size_t(i)].unvoiced) {
                    frames[size_t(i)].correction = {FrequencyMapCheck::UnvoicedFrame, -1};
                }
            }
            for (const auto &run : runsOf(frames, first, last, FrequencyMapCheck::UnvoicedFrame)) {
                if (run.first <= first || run.last >= last) {
                    continue;
                }
                const double left = neighborNote(settings, frames[size_t(run.first - 1)]);
                const double right = neighborNote(settings, frames[size_t(run.last + 1)]);
                for (int i = run.first; i <= run.last; ++i) {
                    frames[size_t(i)].correction.frequency = interpolated(run, i, left, right);
                }
            }
        }

    }

    double FrequencyMapCheck::segmentThreshold(const FrequencyMap &map) {
        const auto &frames = map.frames;
        std::vector<double> louder;
        for (std::size_t i = 0; i + 1 < frames.size(); i += 2) {
            louder.push_back(std::max(amplitudeOf(frames[i]), amplitudeOf(frames[i + 1])));
        }
        if (louder.empty()) {
            return 0;
        }
        std::sort(louder.begin(), louder.end());
        return std::clamp(louder[louder.size() * 25 / 100], 500.0, 2000.0);
    }

    std::vector<FrequencyMapCheck::Run> FrequencyMapCheck::segments(const FrequencyMap &map) {
        const auto &frames = map.frames;
        const std::size_t count = frames.size();
        const double threshold = segmentThreshold(map);
        std::vector<bool> marked(count, false);
        const std::size_t paired = count / 2 * 2;
        for (std::size_t i = 0; i < paired; i += 2) {
            const bool loud =
                threshold <= std::max(amplitudeOf(frames[i]), amplitudeOf(frames[i + 1]));
            marked[i] = loud;
            marked[i + 1] = loud;
        }
        if (paired < count) {
            marked[paired] = threshold <= amplitudeOf(frames[paired]);
        }
        // The marks spread over the voiced frames next to a marked frame, first forward and then
        // backward.
        bool previous = false;
        for (std::size_t i = 0; i < count; ++i) {
            if (frames[i].frequency > unvoicedFrequency && previous) {
                marked[i] = true;
            }
            previous = marked[i];
        }
        previous = false;
        for (std::size_t i = count; i-- > 0;) {
            if (frames[i].frequency > unvoicedFrequency && previous) {
                marked[i] = true;
            }
            previous = marked[i];
        }

        std::vector<Run> runs;
        for (std::size_t i = 0; i < count; ++i) {
            if (!marked[i]) {
                continue;
            }
            if (i > 0 && marked[i - 1]) {
                runs.back().last = i;
            } else {
                runs.push_back(Run{i, i});
            }
        }
        return runs;
    }

    std::vector<FrequencyMapCheck::Correction> FrequencyMapCheck::check(const FrequencyMap &map,
                                                                        const Settings &settings) {
        std::vector<Frame> frames(map.frames.size());
        for (std::size_t i = 0; i < frames.size(); ++i) {
            frames[i].frequency = map.frames[i].frequency;
            frames[i].unvoiced = frames[i].frequency <= unvoicedFrequency;
        }
        for (const auto &segment : segments(map)) {
            const int first = int(segment.first);
            const int last = int(segment.last);
            checkRange(settings, frames, first, last);
            if (settings.allowsChangeAtStart) {
                clearFromEnd(frames, first, last, 1);
            }
            if (settings.allowsChangeAtEnd) {
                clearFromEnd(frames, first, last, -1);
            }
            fillUnvoiced(settings, frames, first, last);
        }
        std::vector<Correction> corrections;
        corrections.reserve(frames.size());
        for (const auto &frame : frames) {
            corrections.push_back(frame.correction);
        }
        return corrections;
    }

    FrequencyMap FrequencyMapCheck::withAmplitudes(FrequencyMap map, const WaveAudio &audio) {
        const auto count = std::size_t(audio.frameCount());
        const auto channels = std::size_t(std::max(audio.channels, 1));
        for (std::size_t i = 0; i < map.frames.size(); ++i) {
            auto &frame = map.frames[i];
            if (frame.amplitude) {
                continue;
            }
            const auto first = std::min(count, std::size_t(std::floor(double(i) * map.hop)));
            const auto last = std::min(count, std::size_t(std::floor(double(i + 1) * map.hop)));
            if (first >= last) {
                frame.amplitude = 0.0;
                continue;
            }
            // The samples in 16-bit units are integers below 2^16, so that the sum of their
            // squares is exact in any order.
            double sum = 0;
            for (std::size_t j = first; j < last; ++j) {
                const double sample = std::round(audio.samples[j * channels] * 32768.0);
                sum += sample * sample;
            }
            frame.amplitude = std::sqrt(sum / double(last - first));
        }
        return map;
    }

}
