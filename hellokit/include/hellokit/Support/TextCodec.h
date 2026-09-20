#ifndef HELLOKIT_SUPPORT_TEXTCODEC_H
#define HELLOKIT_SUPPORT_TEXTCODEC_H

#include <memory>
#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Support/HelloKitSupportGlobal.h>

namespace hello::kit {

    /// One text encoding, named.
    ///
    /// Everything in memory is UTF-8 or UTF-16 already, so this exists for the edge where bytes
    /// are read from or written to a file whose encoding something else decided. Nothing here
    /// guesses: the name comes from what was recorded, or from the user.
    ///
    /// \sa docs/note.md, for where each file's encoding is recorded
    class HELLOKIT_SUPPORT_EXPORT TextCodec {
    public:
        /// \param name an encoding name Qt knows, such as \c UTF-8 or \c Shift_JIS. An empty
        ///        name means the system's own eight bit encoding, which is the code page on
        ///        Windows and almost always UTF-8 elsewhere.
        explicit TextCodec(const QString &name = {});
        ~TextCodec();

        TextCodec(const TextCodec &RHS);
        TextCodec &operator=(const TextCodec &RHS);

        /// Whether the name was one Qt knows. Nothing else works on an invalid codec.
        bool isValid() const;

        /// The canonical name, which is not always the one that was asked for.
        QString name() const;

        bool isUtf8() const;

        /// \return the text, or nothing where \a bytes are not valid in this encoding
        ///
        /// Refused rather than patched up with replacement characters, since a decoding that
        /// went wrong means the wrong encoding was chosen and the user has to be told, not shown
        /// a page of question marks.
        std::optional<QString> decode(QByteArrayView bytes) const;

        /// Encodes as it stands. Whatever this encoding cannot hold is replaced by Qt, so escape
        /// first where that matters.
        ///
        /// \sa escapeText()
        QByteArray encode(QStringView text) const;

        /// Whether every character of \a text survives this encoding unchanged.
        bool canEncode(QStringView text) const;

        /// The encodings that can be offered to the user.
        static QStringList availableNames();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOKIT_SUPPORT_TEXTCODEC_H
