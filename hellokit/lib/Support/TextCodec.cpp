#include "TextCodec.h"

#ifdef _WIN32
#  include <stdcorelib/platform/windows/stdc_windows.h>
#endif

#include <QtCore/QStringConverter>
#include <QtCore/QStringDecoder>
#include <QtCore/QStringEncoder>

namespace hello::kit {

    // Held as a name, and the converters are built from that name every time.
    //
    // Not as a QStringConverter::Encoding, which was the first attempt and was wrong.
    // QStringConverter::encodingForName() only answers for the handful of encodings the enum
    // lists, which is the Unicode family plus Latin-1. Everything else this project needs comes
    // from ICU, is listed by availableCodecs(), and is reachable only by handing the name to
    // QStringDecoder or QStringEncoder. Going through the enum silently turned Shift_JIS, GBK
    // and the rest into an invalid codec.
    class TextCodec::Impl {
    public:
        explicit Impl(const QString &requested)
            : requested(requested.isEmpty() ? TextCodec::systemName() : requested) {
            valid = makeDecoder().isValid();
        }

        QStringDecoder makeDecoder() const {
            return QStringDecoder(requested);
        }

        QStringEncoder makeEncoder() const {
            return QStringEncoder(requested);
        }

        QString requested;
        bool valid = false;
    };

    TextCodec::TextCodec(const QString &name) : _impl(std::make_unique<Impl>(name)) {
    }

    TextCodec::~TextCodec() = default;

    TextCodec::TextCodec(const TextCodec &RHS) : _impl(std::make_unique<Impl>(*RHS._impl)) {
    }

    TextCodec &TextCodec::operator=(const TextCodec &RHS) {
        if (this != &RHS) {
            *_impl = *RHS._impl;
        }
        return *this;
    }

    bool TextCodec::isValid() const {
        return _impl->valid;
    }

    QString TextCodec::name() const {
        if (!_impl->valid) {
            return {};
        }
        return QString::fromLatin1(_impl->makeDecoder().name());
    }

    bool TextCodec::isUtf8() const {
        if (!_impl->valid) {
            return false;
        }
        return name().compare(QLatin1String("UTF-8"), Qt::CaseInsensitive) == 0;
    }

    std::optional<QString> TextCodec::decode(QByteArrayView bytes) const {
        if (!_impl->valid) {
            return std::nullopt;
        }
        auto decoder = _impl->makeDecoder();
        QString text = decoder.decode(bytes);
        if (decoder.hasError()) {
            return std::nullopt;
        }
        return text;
    }

    QByteArray TextCodec::encode(QStringView text) const {
        if (!_impl->valid) {
            return {};
        }
        auto encoder = _impl->makeEncoder();
        return encoder.encode(text);
    }

    bool TextCodec::canEncode(QStringView text) const {
        if (!_impl->valid) {
            return false;
        }

        // Encoded and then read back, rather than trusting the encoder to report a failure. An
        // encoding that cannot hold a character substitutes a question mark for it and says
        // nothing, so only comparing the result catches that. Comparing also keeps a text that
        // was already a question mark from looking like a failure.
        auto encoder = _impl->makeEncoder();
        const QByteArray bytes = encoder.encode(text);
        if (encoder.hasError()) {
            return false;
        }

        auto decoder = _impl->makeDecoder();
        const QString back = decoder.decode(bytes);
        return !decoder.hasError() && back == text;
    }

    QString TextCodec::systemName() {
#ifdef _WIN32
        // What UTAU writes when it writes nothing about the encoding. Named rather than left as
        // QStringConverter::System, whose own name is the word "Locale" and cannot be recorded.
        //
        // Only the code pages a UTAU user is actually on are spelled out. Anything else is
        // handed to ICU by number, and an encoding ICU does not know leaves the codec invalid,
        // which is the honest outcome.
        switch (::GetACP()) {
            case 932:
                return QStringLiteral("Shift_JIS");
            case 936:
                return QStringLiteral("GBK");
            case 950:
                return QStringLiteral("Big5");
            case 949:
                return QStringLiteral("EUC-KR");
            case 65001:
                return QStringLiteral("UTF-8");
            default:
                return QStringLiteral("windows-%1").arg(::GetACP());
        }
#else
        // No such thing outside Windows. A file written there and carrying no declaration is
        // UTF-8 in every setting anyone still runs.
        return QStringLiteral("UTF-8");
#endif
    }

    QStringList TextCodec::ustCandidates() {
        QStringList names{
            QStringLiteral("UTF-8"),
            QStringLiteral("Shift_JIS"), // Japan
            QStringLiteral("GBK"),       // mainland China
            QStringLiteral("Big5"),      // Taiwan
        };
        const QString system = systemName();
        if (!names.contains(system, Qt::CaseInsensitive)) {
            names.append(system);
        }
        return names;
    }

    QStringList TextCodec::availableNames() {
        return QStringConverter::availableCodecs();
    }


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

    QString TextCodec::escape(const QString &text) const {
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

            if (canEncode(unit)) {
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

    QString TextCodec::unescape(const QString &text) {
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
