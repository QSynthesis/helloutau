#ifndef HELLOKIT_SUPPORT_TEXTCODEC_H
#define HELLOKIT_SUPPORT_TEXTCODEC_H

#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>
#include <QtCore/QString>
#include <QtCore/QStringConverter>
#include <QtCore/QStringList>

#include <hellokit/Support/HelloKitSupportGlobal.h>

namespace hello::kit {

    /// A text encoding, together with the encoding policy of this project.
    ///
    /// Most code does not use this class. In memory all text is UTF-8, and \c .usth is UTF-8 by
    /// definition. The class exists for files that are not: a \c .ust, an \c oto.ini or a
    /// plugin's temporary file, whose bytes were written in the encoding that another program
    /// used at the time.
    ///
    /// Compared with \c QStringEncoder and \c QStringDecoder, it centralizes three decisions that
    /// are easy to get wrong:
    ///
    /// - **Names are resolved by the method that works.** See the note on availableNames().
    /// - **Invalid byte sequences are rejected** rather than replaced, because they indicate an
    ///   incorrect encoding choice that must be reported.
    /// - **Representability is determined by a round trip**, not by querying the encoder for
    ///   errors.
    ///
    /// \note No encoding detection is performed. The encoding of a file is taken from its
    ///       recorded metadata or specified by the user.
    ///
    /// \sa canEncode(), docs/note.md
    class HELLOKIT_SUPPORT_EXPORT TextCodec {
    public:
        /// \param name an encoding name, or empty for the system encoding, which is the ANSI code
        ///        page on Windows and UTF-8 elsewhere
        ///
        /// \note An empty name is resolved to a concrete name immediately, so that name() returns
        ///       a value that can be recorded. Qt names the system encoding \c Locale, which is
        ///       meaningless to a later reader of the file.
        explicit TextCodec(const QString &name = {});

        bool isValid() const;

        /// The canonical name, which is the name recorded in files. Empty for an invalid codec.
        QString name() const;

        bool isUtf8() const;

        /// \return the decoded text, or \c std::nullopt if \a bytes is not valid in this encoding
        std::optional<QString> decode(QByteArrayView bytes) const;

        /// Decodes \a bytes , replacing each invalid byte sequence with U+FFFD.
        ///
        /// For a file that remains usable despite a few invalid bytes, such as a \c readme.txt
        /// with one mistyped character. The caller reports the count, and must not write the
        /// replaced text back, because the original bytes are lost in it.
        ///
        /// Decoding resumes at the byte after the start of an invalid sequence, at the shortest
        /// sequence of up to four bytes that decodes. A stateful encoding, such as ISO-2022-JP,
        /// is therefore not resumed correctly after an invalid sequence.
        ///
        /// \param invalid receives the number of invalid sequences, zero if decode() succeeds
        /// \return the decoded text, or empty for an invalid codec
        QString decodeReplacing(QByteArrayView bytes, qsizetype *invalid = nullptr) const;

        /// Encodes \a text without escaping. Unrepresentable characters are replaced, on the ANSI
        /// code pages with a question mark as Windows does. Call escape() first if
        /// replacement is unacceptable.
        QByteArray encode(QStringView text) const;

        /// Returns whether every character of \a text is preserved by this encoding.
        ///
        /// The text is encoded, decoded and compared, rather than checked through the error
        /// state of the encoder. An encoder substitutes a question mark for an unrepresentable
        /// character without reporting an error, and the comparison also prevents a literal
        /// question mark from being mistaken for a failure.
        bool canEncode(QStringView text) const;

        /// \name Encoding unrepresentable text
        ///
        /// A simplified Chinese lyric has no Shift_JIS representation, yet a file for which the
        /// user selected Shift_JIS must still be written. Unrepresentable characters are written
        /// as escape sequences instead of question marks, so that reading the file restores the
        /// lyric.
        ///
        /// The escape sequences are those of JSON, which are plain ASCII and therefore
        /// representable in every encoding this is used with. <tt>\\uXXXX</tt> is one UTF-16
        /// code unit as four lowercase hexadecimal digits, and <tt>\\\\</tt> is one backslash. A
        /// character outside the Basic Multilingual Plane becomes two <tt>\\uXXXX</tt>
        /// sequences, one per surrogate, as in JSON.
        ///
        /// \warning **Escaping doubles every backslash, even if nothing else is
        ///          escaped.** Otherwise an escape sequence present in the text could not be
        ///          distinguished from one inserted by escaping, and a lyric containing
        ///          <tt>\\u0041</tt> would be read back as \c A.
        ///
        /// \warning **Applies only to files written by HelloUtau in an encoding other than
        ///          UTF-8.** A UST from UTAU or another editor does not use this scheme, and
        ///          unescaping it would remove its backslashes. Files written this way are
        ///          identified by the control note, which records the encoding.
        /// @{

        QString escape(const QString &text) const;

        /// The inverse of escape(), which requires no encoding. A backslash sequence that
        /// matches neither form is preserved rather than dropped, because it is part of the
        /// original text.
        static QString unescape(const QString &text);

        /// @}

        /// The name of the system default encoding, in a form that can be recorded.
        ///
        /// A UST without a \c Charset line is in the system encoding of the machine that
        /// **wrote** the file, not of the machine reading it.
        static QString systemName();

        /// The encodings a UST is likely to use, in the order in which they are offered.
        ///
        /// The list is short by design. UTAU writes either UTF-8, marked by a \c Charset line,
        /// or the ANSI code page of the writing machine, with no marker. Choosing an encoding
        /// therefore amounts to identifying the region where the file was written, which in
        /// practice is Japan, mainland China or Taiwan. The system encoding is appended if it is
        /// none of these.
        static QStringList ustCandidates();

        /// All encodings available to this class, for files in none of the above.
        ///
        /// \note A superset of \c QStringConverter::Encoding. That enumeration covers only the
        ///       Unicode encodings and Latin-1, and \c encodingForName() resolves only those.
        ///       Other encodings are provided by ICU and are reachable only by passing the name
        ///       to \c QStringDecoder, as this class does. Resolving a name through the
        ///       enumeration silently yields an invalid codec for Shift_JIS, GBK and the others.
        static QStringList availableNames();

    private:
        // The conversion paths, in the order described in the implementation file.
        enum class Path {
            Invalid,
            Builtin,
            AnsiCodePage,
            WindowsCodePage,
            ByName,
        };

        Path m_path = Path::Invalid;
        QString m_name;
        QStringConverter::Encoding m_builtin = QStringConverter::Utf8; // Path::Builtin
        // The code page number for Path::AnsiCodePage and Path::WindowsCodePage. For
        // Path::AnsiCodePage it is the value of a winacp::CodePage, which is a private dependency.
        int m_codePage = 0;
        QString m_converterName; // Path::ByName
    };

}

#endif // HELLOKIT_SUPPORT_TEXTCODEC_H
