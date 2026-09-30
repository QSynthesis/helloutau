#ifndef HELLOUTAU_THEME_THEMESYNTAX_H
#define HELLOUTAU_THEME_THEMESYNTAX_H

#include <optional>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QList>
#include <QtCore/QString>

#include <helloutau/Theme/HelloUtauThemeGlobal.h>

namespace hello::daw {

    struct ThemeArgument;

    /// A value of the value syntax of themes, as parsed by ThemeSyntax. See the section on the
    /// syntax in docs/Theme.md.
    struct HELLOUTAU_THEME_EXPORT ThemeValue {
        enum Kind {
            /// A run of characters other than blanks, commas, parentheses, equal signs and
            /// quotes: \c white, \c #FFF, \c 1px, \c solid.
            Word,
            /// A quoted string, stored without its quotes and escape characters.
            String,
            /// A word immediately followed by parentheses: \c qpen(white, 1px).
            Function,
            /// Parenthesized values without a name, such as a group of button states.
            Group,
            /// Values separated by blanks, such as \c 2px \c 2px.
            Sequence,
        };

        Kind kind = Word;

        /// The word, the text of the string, or the name of the function.
        QString text;

        /// The arguments of a function or a group.
        std::vector<ThemeArgument> arguments;

        /// The values of a sequence.
        std::vector<ThemeValue> items;

        /// The offset of the value in the source text, for error messages.
        qsizetype position = 0;

        /// Returns the value in its written form.
        QString toString() const;
    };

    /// An argument of a function or a group: a value and its optional key.
    struct HELLOUTAU_THEME_EXPORT ThemeArgument {
        /// Empty for a positional argument.
        QString key;
        ThemeValue value;
    };

    /// An error in a value, with its offset in the source text.
    struct ThemeError {
        qsizetype position = 0;
        QString message;
    };

    /// The parser of the value syntax of themes. Positional arguments precede keyword arguments.
    /// A positional argument after a keyword argument, unbalanced delimiters and trailing text
    /// are errors.
    class HELLOUTAU_THEME_EXPORT ThemeSyntax {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::ThemeSyntax)
    public:
        /// Parses \a text as one value. Returns \c std::nullopt, with the reason in \a error, if
        /// \a text is malformed.
        static std::optional<ThemeValue> parse(QStringView text, ThemeError *error = nullptr);

        /// Parses \a text as the arguments of a function without the enclosing parentheses, in
        /// the form in which Qt passes the arguments of \c func(...) from a style sheet. Returns
        /// \c std::nullopt, with the reason in \a error, if \a text is malformed.
        static std::optional<std::vector<ThemeArgument>>
            parseArguments(QStringView text, ThemeError *error = nullptr);
    };

}

#endif // HELLOUTAU_THEME_THEMESYNTAX_H
