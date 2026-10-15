#include "FrcFormat.h"

#include <algorithm>

#include <hellokit/Support/BinaryStream.h>

namespace hello::daw {

    // The layout, as model4 writes it and frqeditor reads and writes it, is that of the frq of
    // resampler.exe: FREQ0003, the int32 hop in samples, the double average frequency, 16
    // reserved bytes, the int32 frame count, and each frame a double frequency and a double
    // amplitude. The reserved bytes of model4 are m4 and six spaces, followed by eight zero
    // bytes, in place of the zeros of frq.

    namespace {

        const QByteArray reservedBytes = QByteArray("m4      ") + QByteArray(8, '\0');

    }

    QString FrcFormat::id() const {
        return QLatin1String(formatId);
    }

    QString FrcFormat::name() const {
        return tr("frc (model4)");
    }

    QStringList FrcFormat::resamplerPatterns() const {
        return {QStringLiteral("model4*.exe")};
    }

    QList<bool> FrcFormat::mapsExist(const std::filesystem::path &directory,
                                     const QList<std::filesystem::path> &wavs) const {
        return sidecarsExist(directory, wavs);
    }

    std::optional<kit::FrequencyMap> FrcFormat::read(const std::filesystem::path &wav,
                                                     int sampleRate,
                                                     kit::DiagnosticList &diagnostics) const {
        const auto path = wav.parent_path() / *sidecarNameOf(wav);
        const auto bytes = contentsOf(path, diagnostics);
        if (!bytes) {
            return std::nullopt;
        }
        if (!isComplete(*bytes) || sampleRate <= 0) {
            return damaged(path, diagnostics);
        }
        kit::BinaryStream stream(*bytes, 8);
        const auto hop = stream.read<qint32>();
        const auto average = stream.read<double>();
        stream.skip(16);
        const auto count = stream.read<qint32>();
        kit::FrequencyMap table;
        table.hop = *hop;
        table.averageFrequency = *average;
        table.frames.reserve(size_t(*count));
        for (qint32 i = 0; i < *count; ++i) {
            kit::FrequencyMap::Frame frame;
            frame.time = double(i) * *hop * 1000 / sampleRate;
            frame.frequency = *stream.read<double>();
            frame.amplitude = *stream.read<double>();
            table.frames.push_back(frame);
        }
        return table;
    }

    // The name follows the rule of frq with the extension frc: a.wav has a_wav.frc.
    std::optional<std::filesystem::path>
        FrcFormat::sidecarNameOf(const std::filesystem::path &wav) const {
        auto extension = wav.extension().u16string();
        std::replace(extension.begin(), extension.end(), u'.', u'_');
        return std::filesystem::path(wav.stem().u16string() + extension + u".frc");
    }

    QList<kit::FrequencyFormat::MapFileDamage>
        FrcFormat::checkMapFiles(const std::filesystem::path &directory,
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

    bool FrcFormat::canWrite() const {
        return true;
    }

    std::optional<QList<kit::FrequencyFormat::FileChange>>
        FrcFormat::changesWriting(const std::filesystem::path &directory,
                                  const QList<MapWrite> &writes,
                                  kit::DiagnosticList &diagnostics) const {
        QList<FileChange> changes;
        for (const auto &write : writes) {
            const auto hop = integralHop(write, diagnostics);
            if (!hop) {
                return std::nullopt;
            }
            const auto &frames = write.map.frames;
            if (std::any_of(frames.begin(), frames.end(),
                            [](const auto &frame) { return !frame.amplitude; })) {
                return unwritable(write.wav,
                                  tr("The format records an amplitude for each frame, and the "
                                     "map lacks amplitudes."),
                                  diagnostics);
            }
            const auto name = *sidecarNameOf(write.wav);
            auto reserved = reservedBytes;
            std::error_code error;
            if (std::filesystem::is_regular_file(directory / name, error)) {
                const auto existing = contentsOf(directory / name, diagnostics);
                if (!existing) {
                    return std::nullopt;
                }
                if (existing->startsWith("FREQ0003") && existing->size() >= 36) {
                    reserved = existing->mid(20, 16);
                }
            }
            QByteArray bytes("FREQ0003");
            kit::BinaryStream stream(&bytes, bytes.size());
            stream.write(*hop);
            stream.write(write.map.averageFrequency.value_or(write.map.pitchAverage()));
            stream.writeBytes(reserved);
            stream.write(qint32(frames.size()));
            for (const auto &frame : frames) {
                stream.write(frame.frequency);
                stream.write(*frame.amplitude);
            }
            changes.push_back(FileChange::rewritten(name, bytes));
        }
        return changes;
    }

    bool FrcFormat::isComplete(const QByteArray &bytes) {
        if (!bytes.startsWith("FREQ0003")) {
            return false;
        }
        kit::BinaryStream stream(bytes, 8);
        const auto hop = stream.read<qint32>();
        const auto average = stream.read<double>();
        if (!hop || !average || !stream.skip(16) || *hop <= 0) {
            return false;
        }
        const auto count = stream.read<qint32>();
        return count && *count >= 0 && stream.bytesAvailable() >= qsizetype(*count) * 16;
    }

}
