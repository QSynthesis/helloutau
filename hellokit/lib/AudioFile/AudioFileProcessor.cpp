#include "AudioFileProcessor.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include <QtCore/QFile>
#include <QtCore/QSaveFile>
#include <QtCore/QString>

#include "WaveAudio.h"

namespace hello::kit {

    namespace {

        QString textOf(const std::filesystem::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

        bool fail(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Error, message, std::nullopt});
            return false;
        }

        // Reads the audio of bytes, with the errors in diagnostics. The warning of a truncated
        // data chunk is left out, since the inspection reports it as a change.
        std::optional<WaveAudio> audioOf(QByteArrayView bytes, DiagnosticList &diagnostics) {
            DiagnosticList found;
            auto audio = WaveAudio::fromBytes(bytes, found);
            for (const auto &diagnostic : found) {
                if (diagnostic.severity == DiagnosticSeverity::Error) {
                    diagnostics.push_back(diagnostic);
                }
            }
            return audio;
        }

        // Returns the mean of each channel of audio
        std::vector<double> meansOf(const WaveAudio &audio) {
            std::vector<double> means(size_t(std::max(0, audio.channels)), 0.0);
            const auto frames = audio.frameCount();
            if (frames == 0) {
                return means;
            }
            for (qsizetype i = 0; i < frames; ++i) {
                for (int c = 0; c < audio.channels; ++c) {
                    means[size_t(c)] += audio.samples[size_t(i * audio.channels + c)];
                }
            }
            for (auto &mean : means) {
                mean /= double(frames);
            }
            return means;
        }

        double peakOf(const std::vector<float> &samples) {
            double peak = 0;
            for (const float sample : samples) {
                peak = std::max(peak, double(std::abs(sample)));
            }
            return peak;
        }

        AudioFileProcessor::Inspection inspectionOf(const WaveAudio &audio,
                                                    const WaveMetadata::Report &metadata) {
            AudioFileProcessor::Inspection inspection;
            inspection.sampleRate = audio.sampleRate;
            inspection.channels = audio.channels;
            inspection.bitsPerSample = audio.bitsPerSample;
            inspection.floatingPoint = audio.floatingPoint;
            inspection.extensible = audio.extensible;
            inspection.truncated = audio.truncated;
            inspection.metadata = metadata;
            for (const double mean : meansOf(audio)) {
                inspection.dcOffset = std::max(inspection.dcOffset, std::abs(mean));
            }
            inspection.peak = peakOf(audio.samples);
            return inspection;
        }

        // Returns the one channel of audio that mix derives
        std::vector<float> mixed(const WaveAudio &audio, AudioFileProcessor::ChannelMix mix) {
            const auto frames = audio.frameCount();
            std::vector<float> result(static_cast<size_t>(frames));
            for (qsizetype i = 0; i < frames; ++i) {
                const auto frame = audio.samples.begin() + i * audio.channels;
                switch (mix) {
                    case AudioFileProcessor::AverageChannels: {
                        double sum = 0;
                        for (int c = 0; c < audio.channels; ++c) {
                            sum += frame[c];
                        }
                        result[size_t(i)] = float(sum / audio.channels);
                        break;
                    }
                    case AudioFileProcessor::FirstChannel:
                        result[size_t(i)] = frame[0];
                        break;
                    case AudioFileProcessor::SecondChannel:
                        result[size_t(i)] = frame[1];
                        break;
                }
            }
            return result;
        }

    }

    AudioFileProcessor::Changes
        AudioFileProcessor::Inspection::changes(const Options &options) const {
        Changes result;
        if (options.sampleRate && *options.sampleRate != sampleRate) {
            result |= SampleRateChange;
        }
        if (options.mono && channels > 1) {
            result |= ChannelChange;
        }
        if (options.sixteenBits && (floatingPoint || bitsPerSample != 16)) {
            result |= BitDepthChange;
        }
        if (options.removingMetadata && !metadata.isEmpty()) {
            result |= MetadataChange;
        }
        if (options.plainHeader && extensible) {
            result |= HeaderChange;
        }
        if (truncated) {
            result |= DataSizeChange;
        }
        if (options.removingDcOffset && dcOffset >= dcOffsetThreshold) {
            result |= DcOffsetChange;
        }
        if (options.peakLevel && peak > 0 &&
            std::abs(20 * std::log10(peak) - *options.peakLevel) > peakTolerance) {
            result |= PeakChange;
        }
        return result;
    }

    std::optional<AudioFileProcessor::Inspection>
        AudioFileProcessor::inspect(QByteArrayView bytes, DiagnosticList &diagnostics) {
        const auto audio = audioOf(bytes, diagnostics);
        if (!audio) {
            return std::nullopt;
        }
        const auto metadata = WaveMetadata::find(bytes, diagnostics);
        if (!metadata) {
            return std::nullopt;
        }
        return inspectionOf(*audio, *metadata);
    }

    std::optional<AudioFileProcessor::Inspection>
        AudioFileProcessor::inspect(const std::filesystem::path &file,
                                    DiagnosticList &diagnostics) {
        QFile in(textOf(file));
        if (!in.open(QIODevice::ReadOnly)) {
            fail(diagnostics, tr("\"%1\" could not be read.").arg(textOf(file)));
            return std::nullopt;
        }
        const auto bytes = in.readAll();
        DiagnosticList found;
        auto inspection = inspect(bytes, found);
        for (auto &diagnostic : found) {
            diagnostic.message = QStringLiteral("%1: %2").arg(textOf(file), diagnostic.message);
            diagnostics.push_back(diagnostic);
        }
        return inspection;
    }

    // The dither is TPDF of one step of the target format, the sum of two uniform variables,
    // which makes the error of the rounding independent of the signal. The generator is seeded
    // alike for every file, so that a file processed twice is written alike.
    std::optional<QByteArray> AudioFileProcessor::processed(QByteArrayView bytes,
                                                            const Options &options,
                                                            DiagnosticList &diagnostics) {
        const auto audio = audioOf(bytes, diagnostics);
        if (!audio) {
            return std::nullopt;
        }
        const auto metadata = WaveMetadata::find(bytes, diagnostics);
        if (!metadata) {
            return std::nullopt;
        }
        const auto changes = inspectionOf(*audio, *metadata).changes(options);
        const bool keepingMetadata = !options.removingMetadata;
        if (!(changes & sampleChanges)) {
            std::optional<QByteArray> format;
            if (changes & HeaderChange) {
                format = audio->formatChunk();
            }
            return WaveMetadata::rebuilt(bytes, format, std::nullopt, keepingMetadata, diagnostics);
        }

        auto out = *audio;
        if (changes & ChannelChange) {
            out.samples = mixed(out, *options.mono);
            out.channels = 1;
        }
        if (changes & DcOffsetChange) {
            const auto means = meansOf(out);
            for (size_t i = 0; i < out.samples.size(); ++i) {
                out.samples[i] -= float(means[i % size_t(out.channels)]);
            }
        }
        if (changes & SampleRateChange) {
            out = out.resampled(*options.sampleRate);
        }
        if (changes & PeakChange) {
            const double gain = std::pow(10.0, *options.peakLevel / 20) / peakOf(out.samples);
            for (auto &sample : out.samples) {
                sample = float(sample * gain);
            }
        }
        if (options.sixteenBits) {
            out.bitsPerSample = 16;
            out.floatingPoint = false;
        }
        if (!out.floatingPoint) {
            // Samples that are no integers of the target format are rounded.
            const bool rounded = (changes & ~Changes(BitDepthChange) & sampleChanges) ||
                                 audio->floatingPoint || audio->bitsPerSample > out.bitsPerSample;
            if (options.dithered && rounded) {
                std::mt19937 generator(0);
                std::uniform_real_distribution<float> uniform(0.0f, 1.0f);
                const float step = 1.0f / float(qint64(1) << (out.bitsPerSample - 1));
                for (auto &sample : out.samples) {
                    sample += (uniform(generator) - uniform(generator)) * step;
                }
            }
            const auto beyond = std::count_if(out.samples.begin(), out.samples.end(),
                                              [](float sample) { return std::abs(sample) > 1; });
            if (beyond > 0) {
                diagnostics.push_back(
                    {DiagnosticSeverity::Warning,
                     tr("%n sample(s) beyond full scale were limited.", nullptr, int(beyond)),
                     std::nullopt});
            }
        }
        return WaveMetadata::rebuilt(bytes, out.formatChunk(), out.dataChunk(), keepingMetadata,
                                     diagnostics);
    }

    bool AudioFileProcessor::process(const std::filesystem::path &file, const Options &options,
                                     DiagnosticList &diagnostics) {
        QFile in(textOf(file));
        if (!in.open(QIODevice::ReadOnly)) {
            return fail(diagnostics, tr("\"%1\" could not be read.").arg(textOf(file)));
        }
        const auto bytes = in.readAll();
        in.close();
        DiagnosticList found;
        const auto result = processed(bytes, options, found);
        for (auto &diagnostic : found) {
            diagnostic.message = QStringLiteral("%1: %2").arg(textOf(file), diagnostic.message);
            diagnostics.push_back(diagnostic);
        }
        if (!result) {
            return false;
        }
        QSaveFile out(textOf(file));
        if (!out.open(QIODevice::WriteOnly) || out.write(*result) != result->size() ||
            !out.commit()) {
            return fail(diagnostics, tr("\"%1\" could not be written.").arg(textOf(file)));
        }
        return true;
    }

}
