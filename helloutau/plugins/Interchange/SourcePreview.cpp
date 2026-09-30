#include "SourcePreview.h"

#include <algorithm>

#include <hellokit/Support/TextCodec.h>

namespace hello::daw {

    QList<QByteArray> SourcePreview::textsOf(const kit::InterchangeSource &source) {
        QList<QByteArray> texts;
        for (const auto &entry : source.entries) {
            texts.push_back(entry.rawName);
            texts += entry.rawLyrics;
        }
        texts += source.rawLabels;
        return texts;
    }

    bool SourcePreview::decodes(const QList<QByteArray> &texts, const QString &encoding) {
        const kit::TextCodec codec(encoding);
        return codec.isValid() && std::all_of(texts.begin(), texts.end(), [&](const auto &text) {
                   return codec.decode(text).has_value();
               });
    }

    QString SourcePreview::defaultEncoding(const QList<QByteArray> &texts,
                                           const QStringList &candidates, const QString &system) {
        const auto utf8 = QStringLiteral("UTF-8");
        if (candidates.contains(utf8) && decodes(texts, utf8)) {
            return utf8;
        }
        if (candidates.contains(system) && decodes(texts, system)) {
            return system;
        }
        return kit::TextCodec::ranked(texts, candidates).value(0);
    }

    QString SourcePreview::decodedOrMarker(const QByteArray &bytes, const QString &encoding) {
        const auto decoded = kit::TextCodec(encoding).decode(bytes);
        return decoded ? *decoded : tr("(not valid in this encoding)");
    }

    QString SourcePreview::preview(const kit::InterchangeSource &source, const QString &encoding) {
        QStringList sections;
        for (const auto &entry : source.entries) {
            QStringList lyrics;
            for (const auto &lyric : entry.rawLyrics) {
                lyrics.push_back(decodedOrMarker(lyric, encoding));
            }
            sections.push_back(
                tr("Track %1: %2\n%3")
                    .arg(entry.index + 1)
                    .arg(decodedOrMarker(entry.rawName, encoding), lyrics.join(QLatin1Char(' '))));
        }
        if (!source.rawLabels.isEmpty()) {
            QStringList labels;
            for (const auto &label : source.rawLabels) {
                labels.push_back(decodedOrMarker(label, encoding));
            }
            sections.push_back(tr("Markers:\n%1").arg(labels.join(QLatin1Char('\n'))));
        }
        return sections.join(QStringLiteral("\n\n"));
    }

}
