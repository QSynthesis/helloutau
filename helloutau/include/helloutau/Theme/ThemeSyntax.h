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

    /// One value of the value syntax of themes, as ThemeSyntax reads it. See the section on the
    /// syntax in docs/Theme.md.
    struct HELLOUTAU_THEME_EXPORT ThemeValue {
        enum Kind {
            /// A run of characters other than blanks, commas, parentheses, equal signs and
            /// quotes: \c white, \c #FFF, \c 1px, \c solid.
            Word,
            /// A quoted text, without its quotes and escapes.
            String,
            /// A word followed at once by parentheses: \c qpen(white, 1px).
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

        /// The offset of the value in the text it was read from, for messages.
        qsizetype position = 0;

        /// Returns the value as it would be written.
        QString toString() const;
    };

    /// An argument of a function or a group: a value, with the key it was given by, if any.
    struct HELLOUTAU_THEME_EXPORT ThemeArgument {
        /// Empty for a positional argument.
        QString key;
        ThemeValue value;
    };

    /// A problem in a value, at an offset in its text.
    struct ThemeError {
        qsizetype position = 0;
        QString message;
    };

    /// Reads the value syntax of themes. Positional arguments precede keyword arguments, and a
    /// positional argument after a keyword argument is an error, as is anything unbalanced or
    /// left over.
    class HELLOUTAU_THEME_EXPORT ThemeSyntax {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::ThemeSyntax)
    public:
        /// Reads \a text as one value, or returns \c std::nullopt with the reason in \a error.
        static std::optional<ThemeValue> parse(QStringView text, ThemeError *error = nullptr);

        /// Reads \a text as the arguments of a function, without its parentheses, as a style
        /// sheet passes them for \c func(...).
        static std::optional<std::vector<ThemeArgument>>
            parseArguments(QStringView text, ThemeError *error = nullptr);
    };

}

#endif // HELLOUTAU_THEME_THEMESYNTAX_H
