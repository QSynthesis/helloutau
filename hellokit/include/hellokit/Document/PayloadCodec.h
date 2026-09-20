#ifndef HELLOKIT_DOCUMENT_PAYLOADCODEC_H
#define HELLOKIT_DOCUMENT_PAYLOADCODEC_H

#include <optional>

#include <QByteArray>
#include <QByteArrayView>

#include <hellokit/Document/HelloKitDocumentGlobal.h>

namespace hello::kit {

    /// Encodes and decodes what the control note carries.
    ///
    /// A UST entry is far from able to hold arbitrary bytes. UTAU truncates a value at an equals
    /// sign or a tab, turns every space into a comma, and drops an entry whose value is empty.
    /// base64url without padding steps around all of that, since its alphabet is letters, digits,
    /// \c - and \c _ and nothing else.
    ///
    /// The result is also plain ASCII, which the reader needs: the encoding of the file as a whole
    /// is one of the things the control note says, so that entry has to be found and read before
    /// anything is decoded.
    ///
    /// \sa docs/claude/utau-ust-preservation.md, for the measurements behind all of this
    class HELLOKIT_DOCUMENT_EXPORT PayloadCodec {
    public:
        /// Returns \a data as base64url without padding. An empty input gives an empty result,
        /// which is not what a caller should write to a note, since UTAU drops an entry that has
        /// no value.
        static QByteArray encode(QByteArrayView data);

        /// Returns the bytes \a text stands for, or nothing when \a text is not base64url.
        ///
        /// Stricter than \c QByteArray::fromBase64Encoding() in two places, each of which would
        /// otherwise take in what could never be written back out. Padding is refused rather than
        /// tolerated, since an equals sign does not survive UTAU. A length leaving six bits over
        /// is refused rather than quietly dropped, since six bits came from no byte.
        static std::optional<QByteArray> decode(QByteArrayView text);
    };

}

#endif // HELLOKIT_DOCUMENT_PAYLOADCODEC_H
