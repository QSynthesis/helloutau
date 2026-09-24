#ifndef HELLOKIT_EDIT_PROJECTCOMMANDS_H
#define HELLOKIT_EDIT_PROJECTCOMMANDS_H

#include <QtCore/QCoreApplication>
#include <QtCore/QMap>
#include <QtCore/QStringList>
#include <QtCore/QStringView>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Edit/HelloKitEditGlobal.h>
#include <hellokit/Edit/ProjectSession.h>

namespace hello::kit {

    /// The commands of a project, the text interface of the handles and of ProjectEdits.
    ///
    /// A command is one line, split into arguments by CommandSyntax. The commands are:
    ///
    /// - <tt>set</tt>, <tt>insert</tt>, <tt>remove</tt>, <tt>move</tt> and <tt>replace</tt>, the
    ///   node operations on the field at a path such as <tt>/tracks/0/notes/12/lyric</tt>.
    /// - <tt>note transpose \<semitones\> \<note\>...</tt>, <tt>note split \<notes\> \<index\>
    ///   \<ticks\></tt>, <tt>note insert \<notes\> \<index\> \<note\></tt> and
    ///   <tt>note tempo \<note\> \<tempo\></tt>, the domain functions. A note is given by its
    ///   path, the notes of a track by the path of their list, and a new note by its JSON.
    ///
    /// See the section on commands in docs/Editing.md.
    class HELLOKIT_EDIT_EXPORT ProjectCommands {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::ProjectCommands)
    public:
        /// Executes the command of \a line on \a session in one transaction, with the command as
        /// its message, which therefore forms one undo step. A line without a command succeeds
        /// without a transaction.
        ///
        /// \return whether the command was executed and its transaction committed. Otherwise
        ///         the session is unchanged and \a diagnostics contains the reason.
        static bool execute(ProjectSession &session, QStringView line, DiagnosticList &diagnostics);

        /// Returns the names of the commands. The name of a domain command consists of its noun
        /// and its verb separated by a space, such as <tt>note split</tt>.
        static QStringList names();

        /// Returns the domain function registry of a project: each function of ProjectEdits by
        /// name, with the name of the command that calls it.
        ///
        /// Each domain function is Q_INVOKABLE, so that the meta-object of ProjectEdits lists
        /// them, and a test compares that list with this one. A domain function without a
        /// command, or a command of a function that no longer exists, fails the test.
        ///
        /// See the section on commands in docs/Editing.md.
        static QMap<QString, QString> domainFunctions();
    };

}

#endif // HELLOKIT_EDIT_PROJECTCOMMANDS_H
