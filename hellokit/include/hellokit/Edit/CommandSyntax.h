#ifndef HELLOKIT_EDIT_COMMANDSYNTAX_H
#define HELLOKIT_EDIT_COMMANDSYNTAX_H

#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonValue>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringView>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Edit/HelloKitEditGlobal.h>

namespace hello::kit {

    /// An argument of a command, see CommandSyntax.
    struct CommandArgument {
        enum Kind {
            /// Text without whitespace. value holds it as a JSON string.
            Word,

            /// A JSON string. value holds the decoded string.
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
    /// Arguments are separated by whitespace and take one of three forms:
    ///
    /// - A word, which does not begin with a double quote, a brace or a bracket, extends to the
    ///   next whitespace and contains no double quote.
    /// - A string, which is a JSON string, with the escapes of JSON.
    /// - A structure, which is a JSON object or array and may contain whitespace.
    ///
    /// The quoting and escaping are those of JSON, therefore a value of a \c .usth file is written
    /// in a command as in the file. A line that is empty or begins with \c # after optional
    /// whitespace contains no command.
    struct HELLOKIT_EDIT_EXPORT CommandSyntax {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::CommandSyntax)
    public:
        /// Returns the arguments of \a line, which are none for an empty line or a comment, or
        /// \c std::nullopt if \a line is malformed, with the reason in \a diagnostics.
        static std::optional<QList<CommandArgument>> split(QStringView line,
                                                           DiagnosticList &diagnostics);
    };

}

#endif // HELLOKIT_EDIT_COMMANDSYNTAX_H
