#include "WaveMetadata.h"

#include <algorithm>
#include <functional>

#include <QtCore/QString>

namespace hello::kit {

    namespace {

        bool fail(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({Diagnostic::Error, message, std::nullopt});
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

        struct Chunk {
            QByteArray id;
            Span span;
        };

        struct Layout {
            WaveMetadata::Report report;
            Span format;
            Span data;
            // Every chunk in the order of the file, the first fmt and data chunks included
            QList<Chunk> chunks;
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
                layout.chunks.push_back({id, span});
                // A chunk whose declared size exceeds the remaining bytes ends the read.
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

    }

    std::optional<WaveMetadata::Report> WaveMetadata::find(QByteArrayView bytes,
                                                           DiagnosticList &diagnostics) {
        const auto layout = walk(readerOf(bytes), bytes.size(), diagnostics);
        return layout ? std::optional(layout->report) : std::nullopt;
    }

    std::optional<QByteArray> WaveMetadata::stripped(QByteArrayView bytes,
                                                     DiagnosticList &diagnostics) {
        return rebuilt(bytes, std::nullopt, std::nullopt, false, diagnostics);
    }

    std::optional<QByteArray> WaveMetadata::rebuilt(QByteArrayView bytes,
                                                    std::optional<QByteArrayView> format,
                                                    std::optional<QByteArrayView> data,
                                                    bool keepingMetadata,
                                                    DiagnosticList &diagnostics) {
        const auto layout = walk(readerOf(bytes), bytes.size(), diagnostics);
        if (!layout) {
            return std::nullopt;
        }
        const auto append = [](QByteArray &out, const QByteArray &id, QByteArrayView contents) {
            out.append(id);
            put32(out, quint32(contents.size()));
            out.append(contents);
            if (contents.size() & 1) {
                out.push_back('\0');
            }
        };
        QByteArray body("WAVE");
        append(body, "fmt ",
               format.value_or(bytes.sliced(layout->format.start, layout->format.size)));
        for (const auto &chunk : layout->chunks) {
            const auto original = bytes.sliced(chunk.span.start, chunk.span.size);
            if (chunk.span.start == layout->format.start) {
                continue;
            }
            if (chunk.span.start == layout->data.start) {
                append(body, chunk.id, data.value_or(original));
            } else if (keepingMetadata) {
                append(body, chunk.id, original);
            }
        }
        QByteArray out("RIFF");
        put32(out, quint32(body.size()));
        out.append(body);
        return out;
    }

}
