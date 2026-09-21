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

    /// One text encoding, and everything this project does about encodings.
    ///
    /// Almost nothing needs it. In memory there is only UTF-8, and \c .usth is UTF-8 by
    /// definition. It exists for the one place where that is not so: a \c .ust, an \c oto.ini or
    /// a plugin's temporary file, whose bytes were written in whatever encoding some other
    /// program was using at the time.
    ///
    /// What it adds over \c QStringEncoder and \c QStringDecoder is three decisions that are
    /// easy to get wrong and are therefore made once, here:
    ///
    /// - **A name is resolved the way that actually works.** See the note on availableNames().
    /// - **Bytes that do not decode are refused** rather than turned into replacement
    ///   characters, because they mean the wrong encoding was chosen and somebody has to be
    ///   told.
    /// - **Whether a character fits is decided by trying it**, not by asking the encoder whether
    ///   it failed. See canEncode().
    ///
    /// \note Nothing here guesses. Which encoding a file is in comes from what was recorded
    ///       about it, or from the user. See docs/note.md.
    class HELLOKIT_SUPPORT_EXPORT TextCodec {
    public:
        /// \param name an encoding name, or empty for the system's own, which is the code page
        ///        on Windows and UTF-8 elsewhere
        ///
        /// \note An empty name is resolved to a real name at once, so that name() answers with
        ///       something that can be written down. Qt's own name for the system encoding is
        ///       the word \c Locale, which says nothing to anyone reading the file later.
        explicit TextCodec(const QString &name = {});
        ~TextCodec();

        TextCodec(const TextCodec &RHS);
        TextCodec &operator=(const TextCodec &RHS);

        bool isValid() const;

        /// The canonical name, which is what gets recorded. Empty for an invalid codec.
        QString name() const;

        bool isUtf8() const;

        /// \return the text, or nothing where \a bytes are not valid in this encoding
        std::optional<QString> decode(QByteArrayView bytes) const;

        /// Encodes as it stands. Whatever this encoding cannot hold is replaced by Qt, so call
        /// escape() first where that matters.
        QByteArray encode(QStringView text) const;

        /// Whether every character of \a text survives this encoding unchanged.
        ///
        /// Encoded and then read back and compared, rather than asking the encoder whether it
        /// had an error. An encoding that cannot hold a character writes a question mark and
        /// says nothing, and comparing is also what keeps a text that was already a question
        /// mark from looking like a failure.
        bool canEncode(QStringView text) const;

        /// \name Writing text this encoding cannot hold
        ///
        /// A simplified Chinese lyric has no Shift_JIS spelling, and a file the user asked for
        /// in Shift_JIS has to be written anyway. What cannot be spelled is written as an escape
        /// instead of becoming a question mark, so that reading it back gives the lyric again.
        ///
        /// The escapes are the ones JSON uses, which are plain ASCII and so survive every
        /// encoding this would be used with. <tt>\\uXXXX</tt> is one UTF-16 code unit, four
        /// lower case hexadecimal digits, and <tt>\\\\</tt> is one backslash. A character
        /// outside the basic plane becomes two <tt>\\uXXXX</tt>, one per surrogate, as in JSON.
        ///
        /// \warning **Escaping doubles every backslash, including where nothing else was
        ///          escaped.** It has to: without that there is no telling an escape the text
        ///          asked for from one this put there, and a lyric holding <tt>\\u0041</tt>
        ///          would come back as \c A.
        ///
        /// \warning **Only files HelloUTAU wrote, and only those that are not UTF-8.** A UST
        ///          from UTAU or another editor knows nothing of this, and unescaping one would
        ///          eat its backslashes. What says a file was written this way is the control
        ///          note, which carries the encoding.
        /// @{

        QString escape(const QString &text) const;

        /// The other direction, which needs no encoding. An escape that is not one of the two
        /// forms is left as it stands rather than dropped, since it came from somewhere.
        static QString unescape(const QString &text);

        /// @}

        /// The encoding the system writes by default, by a name that can be recorded.
        ///
        /// This is what a UST with no \c Charset line is in, except that it is the code page of
        /// the machine that **wrote** the file rather than of the one reading it.
        static QString systemName();

        /// The encodings a UST is realistically in, in the order to offer them.
        ///
        /// A short list on purpose. UTAU writes exactly two things: UTF-8, which says so with a
        /// \c Charset line, and the writer's own code page, which says nothing at all. So the
        /// question a reader has to put is never "which of two hundred encodings" but "which
        /// country was this written in", and in practice that is Japan, mainland China or
        /// Taiwan. The system's own encoding is added where it is none of those.
        static QStringList ustCandidates();

        /// Every encoding Qt can manage, for the rare file that is none of the above.
        ///
        /// \note Far more than \c QStringConverter::Encoding lists. That enum covers the Unicode
        ///       family and Latin-1 and nothing else, and \c encodingForName() answers only for
        ///       those. Everything else comes from ICU and is reachable only by handing the name
        ///       to \c QStringDecoder, which is what this class does. Resolving a name through
        ///       the enum turns Shift_JIS, GBK and the rest into an invalid codec without a word
        ///       of complaint.
        static QStringList availableNames();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOKIT_SUPPORT_TEXTCODEC_H
