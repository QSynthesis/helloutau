#include "BuiltinFrequencyFormats.h"

#include <hellokit/Support/BinaryStream.h>

#include "FrequencyFormat.h"

namespace hello::kit {

    namespace {

        // The frq of resampler.exe, beside the audio file as a_wav.frq. The layout, as the frq
        // reader of OpenUtau reads it
        // (https://github.com/stakira/OpenUtau/blob/0.1.565/OpenUtau.Core/Classic/Frq.cs, and the
        // name in GetFrqFile() of VoicebankFiles.cs of the same tag): FREQ0003, the int32 hop in
        // samples, the double average frequency, 16 reserved bytes, the int32 frame count, and
        // each frame a double frequency and a double amplitude.
        class Frq : public FrequencyFormat {
        public:
            QString id() const override {
                return QStringLiteral("frq");
            }

            QString name() const override {
                return tr("frq (resampler.exe)");
            }

            QStringList resamplerPatterns() const override {
                return {QStringLiteral("resampler*.exe")};
            }

            QList<bool> mapsExist(const std::filesystem::path &directory,
                                  const QList<std::filesystem::path> &wavs) const override {
                return sidecarsExist(directory, wavs);
            }

            std::optional<FrequencyMap> read(const std::filesystem::path &wav, int sampleRate,
                                             DiagnosticList &diagnostics) const override {
                const auto path = wav.parent_path() / *sidecarNameOf(wav);
                const auto bytes = contentsOf(path, diagnostics);
                if (!bytes) {
                    return std::nullopt;
                }
                if (!bytes->startsWith("FREQ0003")) {
                    return damaged(path, diagnostics);
                }
                BinaryStream stream(*bytes, 8);
                const auto hop = stream.read<qint32>();
                const auto average = stream.read<double>();
                if (!hop || !average || !stream.skip(16) || *hop <= 0 || sampleRate <= 0) {
                    return damaged(path, diagnostics);
                }
                const auto count = stream.read<qint32>();
                if (!count || *count < 0 || stream.left() < qsizetype(*count) * 16) {
                    return damaged(path, diagnostics);
                }
                FrequencyMap table;
                table.averageFrequency = *average;
                table.frames.reserve(size_t(*count));
                for (qint32 i = 0; i < *count; ++i) {
                    FrequencyMap::Frame frame;
                    frame.time = double(i) * *hop * 1000 / sampleRate;
                    frame.frequency = *stream.read<double>();
                    frame.amplitude = *stream.read<double>();
                    table.frames.push_back(frame);
                }
                return table;
            }

            std::optional<std::filesystem::path>
                sidecarNameOf(const std::filesystem::path &wav) const override {
                auto name = wav.stem().u16string();
                auto extension = wav.extension().u16string();
                for (auto &c : extension) {
                    if (c == u'.') {
                        c = u'_';
                    }
                }
                return std::filesystem::path(name + extension + u".frq");
            }
        };

    }

    BuiltinFrequencyFormats::BuiltinFrequencyFormats(FrequencyFormatRegistry &registry) {
        m_registrations.push_back(FrequencyFormatRegistry::Add<Frq>(registry, "frq", {}));
    }

    BuiltinFrequencyFormats::~BuiltinFrequencyFormats() = default;

}
