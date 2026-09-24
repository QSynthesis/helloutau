#ifndef HELLOKIT_EDITBASE_COMMANDSYNTAX_H
#define HELLOKIT_EDITBASE_COMMANDSYNTAX_H

#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonValue>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringView>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/EditBase/HelloKitEditBaseGlobal.h>

namespace hello::kit::edit {

    /// An argument of a command, see CommandSyntax.
    struct CommandArgument {
        enum Kind {
            /// Text without whitespace. value holds it as a JSON string.
            Word,

            /// A JSON string or a verbatim string. value holds the decoded string.
            String,

            /// A JSON object or array.
            Structure,
        };

        Kind kind = Word;
        QJsonValue value;

        /// Returns the text of a word or a string, or an empty string for a structure.
        inline QString text() const {
            return value.toString();
        }

        inline bool operator==(const CommandArgument &RHS) const {
            return kind == RHS.kind && value == RHS.value;
        }
    };

    /// The syntax of the arguments of a command.
    ///
    /// Arguments are separated by whitespace and take one of four forms:
    ///
    /// - A word, which does not begin with a double quote, a brace or a bracket, extends to the
    ///   next whitespace and contains no double quote.
    /// - A string, which is a JSON string, with the escapes of JSON.
    /// - A verbatim string, which begins with <tt>\@"</tt> and ends at the next double quote
    ///   that is not doubled. It contains no escapes, and two double quotes denote one.
    /// - A structure, which is a JSON object or array and may contain whitespace.
    ///
    /// The quoting and escaping of strings and structures are those of JSON, therefore a value of
    /// a \c .usth file is written in a command as in the file. A line that is empty or begins
    /// with \c # after optional whitespace contains no command.
    struct HELLOKIT_EDITBASE_EXPORT CommandSyntax {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::edit::CommandSyntax)
    public:
        /// Returns the arguments of \a line, which are none for an empty line or a comment, or
        /// \c std::nullopt if \a line is malformed, with the reason in \a diagnostics.
        static std::optional<QList<CommandArgument>> split(QStringView line,
                                                           DiagnosticList &diagnostics);

        /// Returns the value of \a argument. A word written as a JSON number, \c true, \c false
        /// or \c null is that value, and any other word is its text. A string and a structure
        /// are their values.
        ///
        /// The value does not depend on the field that receives it. Text that reads as a JSON
        /// literal, such as a lyric \c 12, is therefore written as a string.
        static QJsonValue valueOf(const CommandArgument &argument);
    };

}

#endif // HELLOKIT_EDITBASE_COMMANDSYNTAX_H
