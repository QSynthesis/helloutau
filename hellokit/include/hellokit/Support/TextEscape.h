#ifndef HELLOKIT_SUPPORT_TEXTESCAPE_H
#define HELLOKIT_SUPPORT_TEXTESCAPE_H

#include <QtCore/QString>

#include <hellokit/Support/HelloKitSupportGlobal.h>
#include <hellokit/Support/TextCodec.h>

namespace hello::kit {

    /// \name Writing text an encoding cannot hold
    ///
    /// A Chinese lyric has no Shift_JIS spelling, and a file the user asked for in Shift_JIS has
    /// to be written anyway. What cannot be spelled is written as an escape instead of being
    /// replaced by a question mark, so that reading the file back gives the lyric again.
    ///
    /// The escapes are the ones JSON uses, which are plain ASCII and therefore survive every
    /// encoding this would be used with:
    ///
    /// | Written | Means |
    /// |---|---|
    /// | <tt>\\uXXXX</tt> | the UTF-16 code unit \c XXXX, four hexadecimal digits, lower case |
    /// | <tt>\\\\</tt> | one backslash |
    ///
    /// A character outside the basic plane becomes two <tt>\\uXXXX</tt>, one per surrogate, as
    /// in JSON.
    ///
    /// \warning **Escaping doubles every backslash, including where nothing else was escaped.**
    ///          It has to: without that there is no telling an escape the text asked for from
    ///          one this put there, and unescaping would turn a lyric holding <tt>\\u0041</tt>
    ///          into one holding \c A.
    ///
    /// \warning **This is applied only to files HelloUTAU wrote, and only when they are not
    ///          UTF-8.** A UST from UTAU or from another editor knows nothing of it, and
    ///          unescaping one would eat its backslashes. What says a file was written this way
    ///          is the control note, which carries the encoding: no control note means no
    ///          escaping, and a control note saying UTF-8 means there was nothing to escape.
    /// @{

    /// Returns \a text with every backslash doubled and every character \a target cannot hold
    /// written as an escape.
    HELLOKIT_SUPPORT_EXPORT QString escapeText(const QString &text, const TextCodec &target);

    /// The other direction. An escape that is not one of the two forms above is left as it
    /// stands rather than dropped, since it came from somewhere and guessing loses it.
    HELLOKIT_SUPPORT_EXPORT QString unescapeText(const QString &text);

    /// @}

}

#endif // HELLOKIT_SUPPORT_TEXTESCAPE_H
