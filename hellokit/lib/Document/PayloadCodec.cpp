#include "PayloadCodec.h"

namespace hello::kit {

    QByteArray PayloadCodec::encode(QByteArrayView data) {
        return data.toByteArray().toBase64(QByteArray::Base64UrlEncoding |
                                           QByteArray::OmitTrailingEquals);
    }

    std::optional<QByteArray> PayloadCodec::decode(QByteArrayView text) {
        // Qt takes padding when it is well formed, and this is where it stops being welcome. An
        // equals sign is truncated by UTAU, so text carrying one was never written by us and could
        // never be written back.
        if (text.contains('=')) {
            return std::nullopt;
        }

        // Qt drops the bits left over without a word. Six of them came from no byte.
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
