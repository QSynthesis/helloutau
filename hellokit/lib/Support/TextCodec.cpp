#include "TextCodec.h"

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
        explicit Impl(const QString &requested) : requested(requested) {
            valid = makeDecoder().isValid();
        }

        QStringDecoder makeDecoder() const {
            return requested.isEmpty() ? QStringDecoder(QStringConverter::System)
                                       : QStringDecoder(requested);
        }

        QStringEncoder makeEncoder() const {
            return requested.isEmpty() ? QStringEncoder(QStringConverter::System)
                                       : QStringEncoder(requested);
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

    QStringList TextCodec::availableNames() {
        return QStringConverter::availableCodecs();
    }

}
