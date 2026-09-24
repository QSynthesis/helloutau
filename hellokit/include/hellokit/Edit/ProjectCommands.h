#ifndef HELLOKIT_EDIT_PROJECTCOMMANDS_H
#define HELLOKIT_EDIT_PROJECTCOMMANDS_H

#include <QtCore/QCoreApplication>
#include <QtCore/QStringList>
#include <QtCore/QStringView>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Edit/HelloKitEditGlobal.h>
#include <hellokit/Edit/ProjectSession.h>

namespace hello::kit {

    /// The commands of a project, the text interface of the handles and of ProjectEdits. See the
    /// section on commands in docs/Editing.md.
    ///
    /// A command is one line, split into arguments by CommandSyntax. The commands are:
    ///
    /// - <tt>set</tt>, <tt>insert</tt>, <tt>remove</tt>, <tt>move</tt> and <tt>replace</tt>, the
    ///   node operations on the field at a path such as <tt>/tracks/0/notes/12/lyric</tt>.
    /// - <tt>note transpose \<semitones\> \<note\>...</tt>, <tt>note split \<notes\> \<index\>
    ///   \<ticks\></tt>, <tt>note insert \<notes\> \<index\> \<note\></tt> and
    ///   <tt>note tempo \<note\> \<tempo\></tt>, the domain functions. A note is given by its
    ///   path, the notes of a track by the path of their list, and a new note by its JSON.
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
    };

}

#endif // HELLOKIT_EDIT_PROJECTCOMMANDS_H
