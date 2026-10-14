#include "FrequencyMapConverter.h"

#include <algorithm>
#include <cmath>

#include <hellokit/Support/WaveAudio.h>

#include "FrequencyFormats.h"

namespace hello::kit {

    namespace {

        // Returns whether map is to be resampled to the hop of target at sampleRate
        bool differsInHop(const FrequencyMap &map, const FrequencyFormat &target, int sampleRate) {
            const double hop = target.hopOf(sampleRate);
            return std::abs(map.hop - hop) > hop * 1e-9;
        }

        // Returns the root mean square of the samples of the first channel of audio from the
        // sample first to the sample last, exclusive, on the scale of 16-bit samples, or 0 if no
        // sample of the audio is in the range
        double amplitudeOf(const WaveAudio &audio, qint64 first, qint64 last) {
            first = std::max<qint64>(first, 0);
            last = std::min<qint64>(last, audio.frameCount());
            if (first >= last) {
                return 0;
            }
            double sum = 0;
            for (qint64 i = first; i < last; ++i) {
                const double sample = double(audio.samples[size_t(i * audio.channels)]) * 32768;
                sum += sample * sample;
            }
            return std::sqrt(sum / double(last - first));
        }

    }

    FrequencyMap FrequencyMapConverter::converted(const FrequencyMap &map,
                                                  const FrequencyFormat &target,
                                                  const WaveAudio &audio) {
        auto result = map;
        if (differsInHop(map, target, audio.sampleRate)) {
            const double hop = target.hopOf(audio.sampleRate);
            const auto count = size_t(std::floor(double(audio.frameCount()) / hop)) + 1;
            result = map.resampled(hop, audio.sampleRate, count);
        }
        if (std::any_of(result.frames.begin(), result.frames.end(),
                        [](const FrequencyMap::Frame &frame) { return !frame.amplitude; })) {
            for (size_t i = 0; i < result.frames.size(); ++i) {
                result.frames[i].amplitude =
                    amplitudeOf(audio, std::llround(double(i) * result.hop),
                                std::llround(double(i + 1) * result.hop));
            }
        }
        return result;
    }

    FrequencyMapConverter::Outcome
        FrequencyMapConverter::convert(const FrequencyFormat &source, const FrequencyFormat &target,
                                       const std::filesystem::path &directory,
                                       const QList<std::filesystem::path> &wavs,
                                       DiagnosticList &diagnostics) {
        Outcome outcome;
        const auto exist = source.mapsExist(directory, wavs);
        QList<FrequencyFormat::MapWrite> writes;
        int resampled = 0;
        for (qsizetype i = 0; i < wavs.size(); ++i) {
            if (!exist.value(i)) {
                continue;
            }
            const auto path = directory / wavs.at(i);
            const auto audio = WaveAudio::read(path, diagnostics);
            if (!audio) {
                continue;
            }
            const auto map = source.read(path, audio->sampleRate, diagnostics);
            if (!map) {
                continue;
            }
            if (differsInHop(*map, target, audio->sampleRate)) {
                ++resampled;
            }
            FrequencyFormat::MapWrite write;
            write.wav = wavs.at(i);
            write.map = converted(*map, target, *audio);
            write.sampleRate = audio->sampleRate;
            write.editedByHand = false;
            writes.push_back(std::move(write));
        }
        if (!writes.isEmpty() &&
            FrequencyFormats::writeMaps(target, directory, writes, true, diagnostics)) {
            outcome.converted = int(writes.size());
            outcome.resampled = resampled;
        }
        return outcome;
    }

}
