#include "PmkFormat.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <hellokit/Support/BinaryStream.h>

namespace hello::daw {

    // The layout, as TIPS writes it and frqeditor reads and writes it, little-endian: the two
    // bytes 13 00, the double base period, the int32 count of marks, and each mark the int32
    // position of its end and the int32 period, all in samples at 44100 Hz. The base period is
    // that of the average frequency, or 0 if the average frequency is 55 Hz or below. An unvoiced
    // span is written as marks of 49 samples.

    namespace {

        constexpr qsizetype headerSize = 14;
        constexpr int unvoicedPeriod = 49;

        struct Mark {
            qint32 end = 0;
            qint32 period = 0;
        };

        struct Marks {
            double basePeriod = 0;
            std::vector<Mark> marks;
            std::size_t frameCount = 0;
        };

        // Returns the marks of bytes, or std::nullopt if bytes is no complete pmk. The frames reach
        // the end of the last period. A pmk with more than markHop frames for each mark is
        // rejected, which bounds the frames of a crafted file. A legitimate pmk has a mark at
        // least every few frames, since the longest voiced period is about 800 samples.
        std::optional<Marks> marksOf(const QByteArray &bytes) {
            if (bytes.size() < headerSize || bytes.at(0) != 0x13 || bytes.at(1) != 0) {
                return std::nullopt;
            }
            kit::BinaryStream stream(bytes, 2);
            Marks result;
            result.basePeriod = *stream.read<double>();
            const auto count = *stream.read<qint32>();
            if (count < 0 || stream.bytesAvailable() < qsizetype(count) * 8) {
                return std::nullopt;
            }
            result.marks.reserve(std::size_t(count));
            for (qint32 i = 0; i < count; ++i) {
                Mark mark;
                mark.end = *stream.read<qint32>();
                mark.period = *stream.read<qint32>();
                result.marks.push_back(mark);
            }
            if (!result.marks.empty()) {
                const auto &last = result.marks.back();
                const auto end = qint64(last.end) + last.period;
                const auto frames =
                    end > 0 ? (end + PmkFormat::markHop - 1) / PmkFormat::markHop : qint64(0);
                if (frames > qint64(result.marks.size()) * PmkFormat::markHop) {
                    return std::nullopt;
                }
                result.frameCount = std::size_t(frames);
            }
            return result;
        }

        // Returns the average frequency of basePeriod, as read() reports it.
        double averageOf(double basePeriod) {
            return basePeriod >= PmkFormat::shortestPeriod ? PmkFormat::markRate / basePeriod : 0;
        }

    }

    QString PmkFormat::id() const {
        return QLatin1String(formatId);
    }

    QString PmkFormat::name() const {
        return tr("pmk (TIPS)");
    }

    QStringList PmkFormat::resamplerPatterns() const {
        return {QStringLiteral("tips*.exe")};
    }

    QList<bool> PmkFormat::mapsExist(const std::filesystem::path &directory,
                                     const QList<std::filesystem::path> &wavs) const {
        return sidecarsExist(directory, wavs);
    }

    // The period of each frame is that of the current mark at the start of the frame, after which
    // the current mark advances to the first mark that ends at the start of the next frame or
    // later, as frqeditor reads it.
    std::optional<kit::FrequencyMap> PmkFormat::read(const std::filesystem::path &wav,
                                                     int sampleRate,
                                                     kit::DiagnosticList &diagnostics) const {
        const auto path = wav.parent_path() / *sidecarNameOf(wav);
        const auto bytes = contentsOf(path, diagnostics);
        if (!bytes) {
            return std::nullopt;
        }
        const auto marks = marksOf(*bytes);
        if (!marks || sampleRate <= 0) {
            return damaged(path, diagnostics);
        }
        kit::FrequencyMap table;
        table.hop = hopOf(sampleRate);
        table.averageFrequency = averageOf(marks->basePeriod);
        table.frames.reserve(marks->frameCount);
        std::size_t current = 0;
        for (std::size_t i = 0; i < marks->frameCount; ++i) {
            const auto period = marks->marks[current].period;
            kit::FrequencyMap::Frame frame;
            frame.time = double(i) * markHop * 1000 / markRate;
            frame.frequency = period >= shortestPeriod ? double(markRate) / period : 0;
            table.frames.push_back(frame);
            const auto next = qint64(i + 1) * markHop;
            while (current + 1 < marks->marks.size() && marks->marks[current].end < next) {
                ++current;
            }
        }
        return table;
    }

    double PmkFormat::hopOf(int sampleRate) const {
        return double(sampleRate) * markHop / markRate;
    }

    // The name follows the rule of frq with the extension pmk: a.wav has a_wav.pmk.
    std::optional<std::filesystem::path>
        PmkFormat::sidecarNameOf(const std::filesystem::path &wav) const {
        auto extension = wav.extension().u16string();
        std::replace(extension.begin(), extension.end(), u'.', u'_');
        return std::filesystem::path(wav.stem().u16string() + extension + u".pmk");
    }

    QList<kit::FrequencyFormat::MapFileDamage>
        PmkFormat::checkMapFiles(const std::filesystem::path &directory,
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
            if (!bytes || !marksOf(*bytes)) {
                damages.push_back(
                    {name, tr("The file is no frequency map of the format, or it is incomplete."),
                     false});
            }
        }
        return damages;
    }

    bool PmkFormat::canWrite() const {
        return true;
    }

    // A period below one sample, a frequency above 44100 Hz, is written as one sample, which
    // bounds the marks of a frame.
    std::optional<QList<kit::FrequencyFormat::FileChange>>
        PmkFormat::changesWriting(const std::filesystem::path &directory,
                                  const QList<MapWrite> &writes,
                                  kit::DiagnosticList &diagnostics) const {
        QList<FileChange> changes;
        for (const auto &write : writes) {
            const auto hop = hopOf(write.sampleRate);
            if (write.sampleRate <= 0 || std::abs(write.map.hop - hop) > hop * 1e-9) {
                return unwritable(write.wav,
                                  tr("The format records a frame every %1 samples at %2 Hz, and "
                                     "the frames of the map are %3 samples apart.")
                                      .arg(hop)
                                      .arg(write.sampleRate)
                                      .arg(write.map.hop),
                                  diagnostics);
            }
            const auto name = *sidecarNameOf(write.wav);
            const double average = write.map.averageFrequency.value_or(write.map.pitchAverage());
            double basePeriod = average > 55 ? markRate / average : 0;
            std::error_code error;
            if (std::filesystem::is_regular_file(directory / name, error)) {
                const auto existing = contentsOf(directory / name, diagnostics);
                if (!existing) {
                    return std::nullopt;
                }
                if (const auto marks = marksOf(*existing);
                    marks && averageOf(marks->basePeriod) == average) {
                    basePeriod = marks->basePeriod;
                }
            }
            std::vector<Mark> marks;
            double position = 0;
            qint64 end = 0;
            for (const auto &frame : write.map.frames) {
                end += markHop;
                const double period = frame.frequency > 55
                                          ? std::max(markRate / frame.frequency, 1.0)
                                          : double(unvoicedPeriod);
                do {
                    const auto rounded = qint32(period + 0.5);
                    marks.push_back({qint32(rounded + position), rounded});
                    position += period;
                } while (position < end);
            }
            QByteArray bytes("\x13\x00", 2);
            kit::BinaryStream stream(&bytes, bytes.size());
            stream.write(basePeriod);
            stream.write(qint32(marks.size()));
            for (const auto &mark : marks) {
                stream.write(mark.end);
                stream.write(mark.period);
            }
            changes.push_back(FileChange::rewritten(name, bytes));
        }
        return changes;
    }

}
