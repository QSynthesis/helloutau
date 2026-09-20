#include "TextEscape.h"

namespace hello::kit {

    namespace {

        void appendEscape(QString &out, char16_t unit) {
            out += QStringLiteral("\\u%1").arg(uint(unit), 4, 16, QLatin1Char('0'));
        }

        bool isHexDigit(QChar c) {
            return (c >= QLatin1Char('0') && c <= QLatin1Char('9')) ||
                   (c >= QLatin1Char('a') && c <= QLatin1Char('f')) ||
                   (c >= QLatin1Char('A') && c <= QLatin1Char('F'));
        }

    }

    QString escapeText(const QString &text, const TextCodec &target) {
        QString out;
        out.reserve(text.size());

        for (qsizetype i = 0; i < text.size(); ++i) {
            const QChar c = text.at(i);

            if (c == QLatin1Char('\\')) {
                out += QLatin1String("\\\\");
                continue;
            }

            // A character outside the basic plane is two code units that mean nothing apart, so
            // the pair is tested and written together.
            const qsizetype width =
                (c.isHighSurrogate() && i + 1 < text.size() && text.at(i + 1).isLowSurrogate())
                    ? 2
                    : 1;
            const QStringView unit(text.constData() + i, width);

            if (target.canEncode(unit)) {
                out += unit;
            } else {
                for (qsizetype k = 0; k < width; ++k) {
                    appendEscape(out, unit.at(k).unicode());
                }
            }
            i += width - 1;
        }

        return out;
    }

    QString unescapeText(const QString &text) {
        QString out;
        out.reserve(text.size());

        for (qsizetype i = 0; i < text.size(); ++i) {
            const QChar c = text.at(i);
            if (c != QLatin1Char('\\') || i + 1 >= text.size()) {
                out += c;
                continue;
            }

            const QChar next = text.at(i + 1);
            if (next == QLatin1Char('\\')) {
                out += QLatin1Char('\\');
                ++i;
                continue;
            }

            if (next == QLatin1Char('u') && i + 5 < text.size() && isHexDigit(text.at(i + 2)) &&
                isHexDigit(text.at(i + 3)) && isHexDigit(text.at(i + 4)) &&
                isHexDigit(text.at(i + 5))) {
                bool ok = false;
                const auto unit = QStringView(text).mid(i + 2, 4).toUShort(&ok, 16);
                if (ok) {
                    out += QChar(char16_t(unit));
                    i += 5;
                    continue;
                }
            }

            // Not an escape this understands. Kept as it stands, because it came from somewhere
            // and dropping it would lose whatever it was.
            out += c;
        }

        return out;
    }

}
