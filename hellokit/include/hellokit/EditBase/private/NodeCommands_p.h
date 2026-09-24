#ifndef HELLOKIT_EDITBASE_PRIVATE_NODECOMMANDS_P_H
#define HELLOKIT_EDITBASE_PRIVATE_NODECOMMANDS_P_H

#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QStringView>

#include <qsubstate/StructNode.h>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/EditBase/CommandSyntax.h>
#include <hellokit/EditBase/EditSession.h>
#include <hellokit/EditBase/HelloKitEditBaseGlobal.h>

#include "FieldTable_p.h"

namespace hello::kit::edit {

    /// The commands that modify the nodes of a tree addressed by path: \c set, \c insert,
    /// \c remove, \c move and \c replace. See the section on commands in docs/Editing.md.
    ///
    /// A path consists of field names and list indices, each preceded by a slash, such as
    /// <tt>/tracks/0/notes/12/lyric</tt>. The path \c / denotes the root. A path that ends at a
    /// value field may continue with members of the JSON of the value, such as
    /// <tt>/tracks/0/notes/12/vibrato/period</tt>. Setting a member replaces the whole value.
    ///
    /// A command applies exactly the values that it states. A value that reading a file would
    /// correct or omit, such as a member of another type or an unknown field, is refused.
    ///
    /// A path that ends at a read-only field is refused by every command, and an internal field is
    /// treated as absent.
    ///
    /// \sa FieldInfo::readOnly, FieldInfo::internal
    struct HELLOKIT_EDITBASE_EXPORT NodeCommands {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::edit::NodeCommands)
    public:
        /// The end of a path.
        struct Target {
            /// The record at the end of the path, or the record of the field at the end.
            ss::StructNodeBase *record = nullptr;

            const RecordInfo *info = nullptr;

            /// The field at the end of the path, or \c nullptr if the path ends at the record.
            const FieldInfo *field = nullptr;

            /// The members of the JSON of a value field that the path continues with.
            QStringList members;

            /// Returns the node in the slot of \c field, or \c nullptr if the slot is empty.
            inline ss::Node *child() const {
                return record->child(field->index);
            }
        };

        /// Returns the end of \a path in the tree of \a session, whose root is a record of type
        /// \a root, or \c std::nullopt with the reason in \a diagnostics.
        static std::optional<Target> resolve(const EditSession &session, const RecordInfo &root,
                                             QStringView path, DiagnosticList &diagnostics);

        /// Returns the names of the commands.
        static QStringList names();

        /// Executes the command \a name with \a arguments, the arguments that follow the name, on
        /// the tree of \a session in the transaction in progress.
        ///
        /// \return whether the command was executed. A refused command modifies nothing and
        ///         reports the reason in \a diagnostics.
        static bool execute(EditSession &session, const RecordInfo &root, QStringView name,
                            const QList<CommandArgument> &arguments, DiagnosticList &diagnostics);

        /// Returns the value of \a argument as an integer, or \c std::nullopt with the reason in
        /// \a diagnostics. \a what names the argument in the message.
        static std::optional<int> integerOf(const CommandArgument &argument, const QString &what,
                                            DiagnosticList &diagnostics);

        /// Returns the value of \a argument as a number, or \c std::nullopt with the reason in
        /// \a diagnostics. \a what names the argument in the message.
        static std::optional<double> numberOf(const CommandArgument &argument, const QString &what,
                                              DiagnosticList &diagnostics);

        /// Returns the value of \a argument as a string, or \c std::nullopt with the reason in
        /// \a diagnostics. \a what names the argument in the message.
        static std::optional<QString> stringOf(const CommandArgument &argument, const QString &what,
                                               DiagnosticList &diagnostics);

        /// Returns the tree of the record of type \a info written as \a json, or \c nullptr with
        /// the reason in \a diagnostics. Each field of \a json is checked against the field
        /// table, so that an unknown field or a value of another type is refused rather than
        /// ignored.
        static std::unique_ptr<ss::Node> treeOf(const RecordInfo &info, const QJsonObject &json,
                                                DiagnosticList &diagnostics);

        /// Appends an error with \a message to \a diagnostics and returns \c false.
        static bool fail(DiagnosticList &diagnostics, const QString &message);
    };

}

#endif // HELLOKIT_EDITBASE_PRIVATE_NODECOMMANDS_P_H
