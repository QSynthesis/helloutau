#include "WaveAudio.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>

namespace hello::kit {

    namespace {

        // The format tags of WAVEFORMATEX, and the one that defers to a subformat
        constexpr quint16 FormatPcm = 0x0001;
        constexpr quint16 FormatFloat = 0x0003;
        constexpr quint16 FormatExtensible = 0xFFFE;

        // The largest channel count accepted, which covers every layout of
        // WAVEFORMATEXTENSIBLE and keeps a malformed header from requesting absurd frames
        constexpr int MaximumChannels = 32;

        void fail(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Error, message, std::nullopt});
        }

        void warn(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Warning, message, std::nullopt});
        }

        quint16 u16At(const char *data) {
            const auto bytes = reinterpret_cast<const uchar *>(data);
            return quint16(bytes[0] | (bytes[1] << 8));
        }

        quint32 u32At(const char *data) {
            const auto bytes = reinterpret_cast<const uchar *>(data);
            return quint32(bytes[0]) | (quint32(bytes[1]) << 8) | (quint32(bytes[2]) << 16) |
                   (quint32(bytes[3]) << 24);
        }

        // Converts one sample of the given format to a float between -1 and 1.
        float sampleAt(const char *data, quint16 format, int bits) {
            const auto bytes = reinterpret_cast<const uchar *>(data);
            if (format == FormatFloat) {
                if (bits == 32) {
                    float value;
                    const quint32 raw = u32At(data);
                    std::memcpy(&value, &raw, sizeof value);
                    return value;
                }
                double value;
                const quint64 raw = quint64(u32At(data)) | (quint64(u32At(data + 4)) << 32);
                std::memcpy(&value, &raw, sizeof value);
                return float(value);
            }
            switch (bits) {
                case 8:
                    // Unsigned, centered on 128
                    return (int(bytes[0]) - 128) / 128.0f;
                case 16:
                    return qint16(u16At(data)) / 32768.0f;
                case 24: {
                    const qint32 value =
                        qint32((quint32(bytes[0]) << 8) | (quint32(bytes[1]) << 16) |
                               (quint32(bytes[2]) << 24)) >>
                        8;
                    return float(value / 8388608.0);
                }
                default:
                    return float(qint32(u32At(data)) / 2147483648.0);
            }
        }

    }

    double WaveAudio::duration() const {
        return sampleRate > 0 ? double(frameCount()) * 1000 / sampleRate : 0;
    }

    std::optional<WaveAudio> WaveAudio::read(const std::filesystem::path &path,
                                             DiagnosticList &diagnostics) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            fail(diagnostics, tr("The audio file could not be opened."));
            return std::nullopt;
        }
        const std::string bytes((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
        return fromBytes(QByteArrayView(bytes.data(), qsizetype(bytes.size())), diagnostics);
    }

    std::optional<WaveAudio> WaveAudio::fromBytes(QByteArrayView bytes,
                                                  DiagnosticList &diagnostics) {
        const auto data = bytes.data();
        const auto size = bytes.size();
        if (size < 12 || std::memcmp(data, "RIFF", 4) != 0 ||
            std::memcmp(data + 8, "WAVE", 4) != 0) {
            fail(diagnostics, tr("This is not a WAVE file."));
            return std::nullopt;
        }

        // The chunks follow the header, each padded to an even size. The size in the RIFF header
        // is not trusted, since the file itself bounds the chunks.
        std::optional<QByteArrayView> format;
        std::optional<QByteArrayView> audio;
        qsizetype position = 12;
        while (position + 8 <= size && !(format && audio)) {
            const auto id = data + position;
            const qsizetype chunkSize = u32At(data + position + 4);
            const qsizetype start = position + 8;
            const qsizetype available = std::min(chunkSize, size - start);
            if (std::memcmp(id, "fmt ", 4) == 0) {
                format = bytes.sliced(start, available);
            } else if (std::memcmp(id, "data", 4) == 0) {
                if (available < chunkSize) {
                    warn(diagnostics, tr("The audio file ends before its audio data does, and is "
                                         "read up to its end."));
                }
                audio = bytes.sliced(start, available);
            }
            position = start + chunkSize + (chunkSize & 1);
        }
        if (!format || format->size() < 16) {
            fail(diagnostics, tr("The WAVE file has no format."));
            return std::nullopt;
        }
        if (!audio) {
            fail(diagnostics, tr("The WAVE file has no audio data."));
            return std::nullopt;
        }

        const auto fmt = format->data();
        quint16 tag = u16At(fmt);
        const int channels = u16At(fmt + 2);
        const auto sampleRate = u32At(fmt + 4);
        const int blockAlign = u16At(fmt + 12);
        const int bits = u16At(fmt + 14);
        // The subformat GUID begins with the format tag it stands for.
        if (tag == FormatExtensible && format->size() >= 26) {
            tag = u16At(fmt + 24);
        }

        const bool supported =
            (tag == FormatPcm && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) ||
            (tag == FormatFloat && (bits == 32 || bits == 64));
        if (!supported) {
            fail(diagnostics, tr("The WAVE format %1 with %2 bits per sample is not supported.")
                                  .arg(tag)
                                  .arg(bits));
            return std::nullopt;
        }
        const int sampleBytes = bits / 8;
        if (channels < 1 || channels > MaximumChannels || sampleRate == 0 ||
            sampleRate > quint32(std::numeric_limits<int>::max()) ||
            blockAlign != channels * sampleBytes) {
            fail(diagnostics, tr("The WAVE format is inconsistent."));
            return std::nullopt;
        }

        WaveAudio result;
        result.sampleRate = int(sampleRate);
        result.channels = channels;
        const qsizetype frames = audio->size() / blockAlign;
        result.samples.resize(size_t(frames * channels));
        const auto source = audio->data();
        for (qsizetype i = 0; i < frames * channels; ++i) {
            result.samples[size_t(i)] = sampleAt(source + i * sampleBytes, tag, bits);
        }
        return result;
    }

}
