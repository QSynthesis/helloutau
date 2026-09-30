#ifndef HELLOKIT_SUPPORT_TEXTSEARCH_H
#define HELLOKIT_SUPPORT_TEXTSEARCH_H

#include <QtCore/QList>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>

#include <hellokit/Support/HelloKitSupportGlobal.h>

namespace hello::kit {

    /// A search pattern with the options of the find widget of VS Code: case sensitivity, whole
    /// words and regular expressions. The find bars of the windows search lyrics, aliases and file
    /// names with it. See the section on find and replace in docs/Widgets.md.
    ///
    /// A regular expression has the syntax of QRegularExpression, which is that of PCRE2. A literal
    /// pattern matches its text exactly. A whole word is a match that is neither preceded nor
    /// followed by a word character, which is a letter, a digit or an underscore of any script.
    /// A match may be empty, such as the match of <tt>^</tt>, which lets a replacement insert text.
    class HELLOKIT_SUPPORT_EXPORT TextSearch {
    public:
        enum Option {
            NoOption = 0,
            CaseSensitive = 0x1,
            WholeWord = 0x2,
            RegularExpression = 0x4,
        };
        Q_DECLARE_FLAGS(Options, Option)

        /// A match in a text, in UTF-16 code units.
        struct Match {
            qsizetype start = 0;
            qsizetype length = 0;

            inline bool operator==(const Match &other) const {
                return start == other.start && length == other.length;
            }
        };

        /// Creates an empty search, which matches nothing.
        TextSearch();
        TextSearch(const QString &pattern, Options options);

        QString pattern() const;
        Options options() const;

        /// Returns whether the search can match: the pattern is not empty and, for a regular
        /// expression, valid.
        bool isValid() const;

        /// Returns the reason why the regular expression is invalid, or an empty string if it is
        /// valid or the pattern is literal.
        QString errorString() const;

        /// Returns whether \a text contains a match.
        bool matches(const QString &text) const;

        /// Returns the matches in \a text from its start, none overlapping another.
        QList<Match> matchesIn(const QString &text) const;

        /// Returns \a text with every match replaced by \a replacement, or \a text unchanged if the
        /// search is invalid.
        ///
        /// For a regular expression, <tt>$0</tt> to <tt>$99</tt> in \a replacement insert the
        /// text of a capturing group, and <tt>$$</tt> inserts a dollar sign, as in VS Code. A
        /// literal pattern inserts \a replacement unchanged.
        QString replaced(const QString &text, const QString &replacement) const;

    private:
        QString m_pattern;
        Options m_options;
        QRegularExpression m_expression;
        QString m_error;

        QString expanded(const QRegularExpressionMatch &match, const QString &replacement) const;
    };

}

Q_DECLARE_OPERATORS_FOR_FLAGS(hello::kit::TextSearch::Options)

#endif // HELLOKIT_SUPPORT_TEXTSEARCH_H
