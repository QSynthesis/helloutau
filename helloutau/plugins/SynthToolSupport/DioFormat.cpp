#include "DioFormat.h"

#include <cmath>

#include <hellokit/AudioFile/WaveAudio.h>
#include <hellokit/Support/BinaryStream.h>

namespace hello::daw {

    // The layout, as world4utau writes it (makeFilename() and the writing of the .dio file in
    // src/world4utau.cpp,
    // https://github.com/LucasCTN/world4utau/blob/2b48d4cc1e348b1a12c32134dbbd8e7b8e2818ef/src/world4utau.cpp):
    // wrld-dio, the int32 signal length, the int32 sample rate, the int32 frame count, and each
    // frame a double time in seconds and a double frequency, 0 if unvoiced. world4utau analyzes
    // in frames of FRAMEPERIOD, 5.80498866213152 milliseconds, and its synthesis indexes the
    // frames by that period.

    namespace {

        constexpr double framePeriod = 5.80498866213152;

    }

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
        if (!isComplete(*bytes)) {
            return damaged(path, diagnostics);
        }
        kit::BinaryStream stream(*bytes, 12);
        const auto rate = stream.read<qint32>();
        const auto count = stream.read<qint32>();
        kit::FrequencyMap table;
        table.hop = hopOf(*rate);
        table.frames.reserve(size_t(*count));
        for (qint32 i = 0; i < *count; ++i) {
            kit::FrequencyMap::Frame frame;
            frame.time = *stream.read<double>() * 1000;
            frame.frequency = *stream.read<double>();
            table.frames.push_back(frame);
        }
        return table;
    }

    double DioFormat::hopOf(int sampleRate) const {
        return sampleRate * framePeriod / 1000;
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

    QList<kit::FrequencyFormat::MapFileDamage>
        DioFormat::checkMapFiles(const std::filesystem::path &directory,
                                 const QList<std::filesystem::path> &wavs) const {
        QList<MapFileDamage> damages;
        for (const auto &wav : wavs) {
            const auto name = *sidecarNameOf(wav);
            std::error_code error;
            if (!std::filesystem::is_regular_file(directory / name, error)) {
                continue;
            }
            kit::DiagnosticList ignored;
            const auto bytes = contentsOf(directory / name, ignored);
            if (!bytes || !isComplete(*bytes)) {
                damages.push_back(
                    {name, tr("The file is no frequency map of the format, or it is incomplete."),
                     false});
            }
        }
        return damages;
    }

    bool DioFormat::canWrite() const {
        return true;
    }

    // The signal length and the sample rate of an existing map are kept. A time unchanged since
    // read() keeps its bytes, since seconds converted to milliseconds and back may differ in the
    // last bit.
    std::optional<QList<kit::FrequencyFormat::FileChange>>
        DioFormat::changesWriting(const std::filesystem::path &directory,
                                  const QList<MapWrite> &writes,
                                  kit::DiagnosticList &diagnostics) const {
        QList<FileChange> changes;
        for (const auto &write : writes) {
            const auto name = *sidecarNameOf(write.wav);
            std::optional<qint32> signalLength;
            std::optional<qint32> rate;
            QList<double> seconds;
            std::error_code error;
            if (std::filesystem::is_regular_file(directory / name, error)) {
                const auto existing = contentsOf(directory / name, diagnostics);
                if (!existing) {
                    return std::nullopt;
                }
                if (existing->startsWith("wrld-dio")) {
                    kit::BinaryStream stream(*existing, 8);
                    signalLength = stream.read<qint32>();
                    rate = stream.read<qint32>();
                    const auto count = stream.read<qint32>().value_or(0);
                    for (qint32 i = 0; i < count && stream.bytesAvailable() >= 16; ++i) {
                        seconds.push_back(*stream.read<double>());
                        stream.skip(8);
                    }
                }
            }
            if (!signalLength || !rate) {
                const auto audio = kit::WaveAudio::read(directory / write.wav, diagnostics);
                if (!audio) {
                    return std::nullopt;
                }
                signalLength = qint32(audio->frameCount());
                rate = audio->sampleRate;
            }
            const auto hop = hopOf(*rate);
            if (std::abs(write.map.hop - hop) > hop * 1e-9) {
                return unwritable(write.wav,
                                  tr("The format records a frame every %1 samples at %2 Hz, and "
                                     "the frames of the map are %3 samples apart.")
                                      .arg(hop)
                                      .arg(*rate)
                                      .arg(write.map.hop),
                                  diagnostics);
            }
            const auto &frames = write.map.frames;
            QByteArray bytes("wrld-dio");
            kit::BinaryStream stream(&bytes, bytes.size());
            stream.write(*signalLength);
            stream.write(*rate);
            stream.write(qint32(frames.size()));
            for (size_t i = 0; i < frames.size(); ++i) {
                const auto &frame = frames[i];
                const bool unchanged =
                    qsizetype(i) < seconds.size() && seconds[qsizetype(i)] * 1000 == frame.time;
                stream.write(unchanged ? seconds[qsizetype(i)] : frame.time / 1000);
                stream.write(frame.frequency);
            }
            changes.push_back(FileChange::rewritten(name, bytes));
        }
        return changes;
    }

    bool DioFormat::isComplete(const QByteArray &bytes) {
        if (!bytes.startsWith("wrld-dio")) {
            return false;
        }
        kit::BinaryStream stream(bytes, 8);
        const auto signalLength = stream.read<qint32>();
        const auto rate = stream.read<qint32>();
        const auto count = stream.read<qint32>();
        return signalLength && rate && count && *count >= 0 &&
               stream.bytesAvailable() >= qsizetype(*count) * 16;
    }

}
