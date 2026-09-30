#include "WaveMetadata.h"

#include <algorithm>
#include <functional>

#include <QtCore/QFile>
#include <QtCore/QSaveFile>
#include <QtCore/QString>

namespace hello::kit {

    namespace {

        bool fail(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Error, message, std::nullopt});
            return false;
        }

        quint32 u32At(const QByteArray &bytes, qsizetype at) {
            quint32 value = 0;
            for (int i = 3; i >= 0; --i) {
                value = (value << 8) | quint8(bytes.at(at + i));
            }
            return value;
        }

        void put32(QByteArray &bytes, quint32 value) {
            for (int i = 0; i < 4; ++i) {
                bytes.push_back(char((value >> (8 * i)) & 0xff));
            }
        }

        // A chunk found: where its bytes start, and how many the file holds of them
        struct Span {
            qint64 start = 0;
            qint64 size = 0;
        };

        struct Layout {
            WaveMetadata::Report report;
            Span format;
            Span data;
        };

        // Reads length bytes at offset, fewer at the end of the file
        using Reader = std::function<QByteArray(qint64 offset, qint64 length)>;

        // Walks the chunks of a file of size bytes.
        std::optional<Layout> walk(const Reader &read, qint64 size, DiagnosticList &diagnostics) {
            const auto header = read(0, 12);
            if (header.size() < 12 || !header.startsWith("RIFF") || header.mid(8, 4) != "WAVE") {
                fail(diagnostics, WaveMetadata::tr("The file is no RIFF WAVE file."));
                return std::nullopt;
            }
            Layout layout;
            bool format = false;
            bool data = false;
            qint64 position = 12;
            while (position + 8 <= size) {
                const auto chunk = read(position, 8);
                if (chunk.size() < 8) {
                    break;
                }
                const auto id = chunk.left(4);
                const qint64 declared = u32At(chunk, 4);
                const Span span{position + 8, std::min(declared, size - position - 8)};
                if (id == "fmt " && !format) {
                    format = true;
                    layout.format = span;
                } else if (id == "data" && !data) {
                    data = true;
                    layout.data = span;
                } else {
                    layout.report.chunks.push_back(id);
                }
                // A chunk that claims more than the file holds ends the file.
                position = std::min(size, span.start + declared + (declared & 1));
            }
            layout.report.trailingBytes = size - position;
            if (!format || !data) {
                fail(diagnostics,
                     WaveMetadata::tr("The file has no %1 chunk.")
                         .arg(format ? QStringLiteral("data") : QStringLiteral("fmt")));
                return std::nullopt;
            }
            return layout;
        }

        Reader readerOf(QByteArrayView bytes) {
            return [bytes](qint64 offset, qint64 length) {
                if (offset >= bytes.size()) {
                    return QByteArray();
                }
                return bytes.sliced(offset, std::min<qint64>(length, bytes.size() - offset))
                    .toByteArray();
            };
        }

        QString textOf(const std::filesystem::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

    }

    std::optional<WaveMetadata::Report> WaveMetadata::find(QByteArrayView bytes,
                                                           DiagnosticList &diagnostics) {
        const auto layout = walk(readerOf(bytes), bytes.size(), diagnostics);
        return layout ? std::optional(layout->report) : std::nullopt;
    }

    std::optional<WaveMetadata::Report> WaveMetadata::find(const std::filesystem::path &file,
                                                           DiagnosticList &diagnostics) {
        QFile in(textOf(file));
        if (!in.open(QIODevice::ReadOnly)) {
            fail(diagnostics, tr("\"%1\" could not be read.").arg(textOf(file)));
            return std::nullopt;
        }
        const auto reader = [&in](qint64 offset, qint64 length) {
            return in.seek(offset) ? in.read(length) : QByteArray();
        };
        DiagnosticList found;
        const auto layout = walk(reader, in.size(), found);
        for (auto &diagnostic : found) {
            diagnostic.message = QStringLiteral("%1: %2").arg(textOf(file), diagnostic.message);
            diagnostics.push_back(diagnostic);
        }
        return layout ? std::optional(layout->report) : std::nullopt;
    }

    std::optional<QByteArray> WaveMetadata::stripped(QByteArrayView bytes,
                                                     DiagnosticList &diagnostics) {
        const auto layout = walk(readerOf(bytes), bytes.size(), diagnostics);
        if (!layout) {
            return std::nullopt;
        }
        const auto chunk = [&bytes](QByteArray &out, const char *id, const Span &span) {
            out.append(id, 4);
            put32(out, quint32(span.size));
            out.append(bytes.sliced(span.start, span.size));
            if (span.size & 1) {
                out.push_back('\0');
            }
        };
        QByteArray body("WAVE");
        chunk(body, "fmt ", layout->format);
        chunk(body, "data", layout->data);
        QByteArray out("RIFF");
        put32(out, quint32(body.size()));
        out.append(body);
        return out;
    }

    bool WaveMetadata::strip(const std::filesystem::path &file, DiagnosticList &diagnostics) {
        QFile in(textOf(file));
        if (!in.open(QIODevice::ReadOnly)) {
            return fail(diagnostics, tr("\"%1\" could not be read.").arg(textOf(file)));
        }
        const auto bytes = in.readAll();
        in.close();
        DiagnosticList found;
        const auto result = stripped(bytes, found);
        if (!result) {
            for (auto &diagnostic : found) {
                diagnostic.message = QStringLiteral("%1: %2").arg(textOf(file), diagnostic.message);
                diagnostics.push_back(diagnostic);
            }
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
