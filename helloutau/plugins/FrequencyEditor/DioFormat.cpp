#include "DioFormat.h"

#include <hellokit/Support/BinaryStream.h>

namespace hello::daw {

    // The layout, as world4utau writes it (makeFilename() and the writing of the .dio file in
    // src/world4utau.cpp,
    // https://github.com/LucasCTN/world4utau/blob/2b48d4cc1e348b1a12c32134dbbd8e7b8e2818ef/src/world4utau.cpp):
    // wrld-dio, the int32 signal length, the int32 sample rate, the int32 frame count, and each
    // frame a double time in seconds and a double frequency, 0 where unvoiced.

    QString DioFormat::id() const {
        return QLatin1String(formatId);
    }

    QString DioFormat::name() const {
        return tr("dio (world4utau)");
    }

    QStringList DioFormat::resamplerPatterns() const {
        return {QStringLiteral("w4u*.exe"), QStringLiteral("world4utau*.exe")};
    }

    QList<bool> DioFormat::mapsExist(const std::filesystem::path &directory,
                                     const QList<std::filesystem::path> &wavs) const {
        return sidecarsExist(directory, wavs);
    }

    std::optional<kit::FrequencyMap> DioFormat::read(const std::filesystem::path &wav,
                                                     int sampleRate,
                                                     kit::DiagnosticList &diagnostics) const {
        Q_UNUSED(sampleRate);
        const auto path = wav.parent_path() / *sidecarNameOf(wav);
        const auto bytes = contentsOf(path, diagnostics);
        if (!bytes) {
            return std::nullopt;
        }
        if (!bytes->startsWith("wrld-dio")) {
            return damaged(path, diagnostics);
        }
        kit::BinaryStream stream(*bytes, 8);
        const auto signalLength = stream.read<qint32>();
        const auto rate = stream.read<qint32>();
        const auto count = stream.read<qint32>();
        if (!signalLength || !rate || !count || *count < 0 ||
            stream.left() < qsizetype(*count) * 16) {
            return damaged(path, diagnostics);
        }
        kit::FrequencyMap table;
        table.frames.reserve(size_t(*count));
        for (qint32 i = 0; i < *count; ++i) {
            kit::FrequencyMap::Frame frame;
            frame.time = *stream.read<double>() * 1000;
            frame.frequency = *stream.read<double>();
            table.frames.push_back(frame);
        }
        return table;
    }

    std::optional<std::filesystem::path>
        DioFormat::sidecarNameOf(const std::filesystem::path &wav) const {
        return wav.filename().replace_extension(".dio");
    }

    // writeSTARParam() and writePlatinumParam() in the source above
    QList<kit::FrequencyFormat::AnalysisCache> DioFormat::analysisCaches() const {
        return {
            {QStringLiteral("STAR"),     AnalysisCache::ReplacedExtension, QStringLiteral(".star")},
            {QStringLiteral("PLATINUM"), AnalysisCache::ReplacedExtension,
             QStringLiteral(".platinum")                                                          },
        };
    }

}
