#include "PayloadCodec.h"

namespace hello::kit {

    QByteArray PayloadCodec::encode(QByteArrayView data) {
        return data.toByteArray().toBase64(QByteArray::Base64UrlEncoding |
                                           QByteArray::OmitTrailingEquals);
    }

    std::optional<QByteArray> PayloadCodec::decode(QByteArrayView text) {
        // Qt accepts well-formed padding, which is rejected here. UTAU truncates values at an
        // equals sign, so text containing one was not written by this program and could not be
        // written back.
        if (text.contains('=')) {
            return std::nullopt;
        }

        // Qt silently discards leftover bits. Six leftover bits cannot originate from a byte.
        if (text.size() % 4 == 1) {
            return std::nullopt;
        }

        auto result = QByteArray::fromBase64Encoding(text.toByteArray(),
                                                     QByteArray::Base64UrlEncoding |
                                                         QByteArray::AbortOnBase64DecodingErrors);
        if (!result) {
            return std::nullopt;
        }
        return result.decoded;
    }

}
