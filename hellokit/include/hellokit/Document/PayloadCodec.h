#ifndef HELLOKIT_DOCUMENT_PAYLOADCODEC_H
#define HELLOKIT_DOCUMENT_PAYLOADCODEC_H

#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>

#include <hellokit/Document/HelloKitDocumentGlobal.h>

namespace hello::kit {

    /// Encodes and decodes the payload of the control note.
    ///
    /// A UST entry cannot hold arbitrary bytes. UTAU truncates a value at an equals sign or a
    /// tab, replaces every space with a comma, and drops an entry whose value is empty. Unpadded
    /// base64url avoids all of these, because its alphabet consists only of letters, digits,
    /// \c - and \c _ .
    ///
    /// The result is also plain ASCII, which the reader requires: the control note records the
    /// encoding of the entire file, so that entry must be located and read before anything is
    /// decoded.
    ///
    /// See docs/claude/utau-ust-preservation.md for the underlying measurements.
    class HELLOKIT_DOCUMENT_EXPORT PayloadCodec {
    public:
        /// Returns \a data as unpadded base64url. An empty input yields an empty result, which a
        /// caller must not write to a note, because UTAU drops an entry without a value.
        static QByteArray encode(QByteArrayView data);

        /// Returns the bytes encoded by \a text , or \c std::nullopt if \a text is not valid
        /// base64url.
        ///
        /// Stricter than \c QByteArray::fromBase64Encoding() in two respects, each of which would
        /// otherwise accept input that could never be written back. Padding is rejected rather
        /// than tolerated, because an equals sign does not survive UTAU. A length that leaves six
        /// trailing bits is rejected rather than silently truncated, because six bits cannot
        /// originate from a whole byte.
        static std::optional<QByteArray> decode(QByteArrayView text);
    };

}

#endif // HELLOKIT_DOCUMENT_PAYLOADCODEC_H
