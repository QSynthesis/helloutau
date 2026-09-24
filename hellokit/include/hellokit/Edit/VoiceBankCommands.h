#ifndef HELLOKIT_EDIT_VOICEBANKCOMMANDS_H
#define HELLOKIT_EDIT_VOICEBANKCOMMANDS_H

#include <QtCore/QCoreApplication>
#include <QtCore/QMap>
#include <QtCore/QStringList>
#include <QtCore/QStringView>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Edit/HelloKitEditGlobal.h>
#include <hellokit/Edit/VoiceBankSession.h>

namespace hello::kit {

    /// The commands of a voice bank, the text interface of the handles and of VoiceBankEdits,
    /// with the conventions of ProjectCommands. The commands are:
    ///
    /// - <tt>set</tt>, <tt>insert</tt>, <tt>remove</tt>, <tt>move</tt> and <tt>replace</tt>, the
    ///   node operations on the field at a path such as <tt>/directories/0/otoEntries/3/alias</tt>.
    /// - <tt>entry set \<entry\> \<entry JSON\></tt>, <tt>entry insert \<directory\> \<entry
    ///   JSON\>...</tt>, <tt>entry include \<directory\> \<file name\>...</tt> and <tt>entry remove
    ///   \<directory\> \<index\>...</tt>, where an entry and a directory are given by their paths.
    /// - <tt>prefix set \<note number\> \<prefix JSON\></tt> and <tt>prefix remove \<note
    ///   number\></tt>.
    /// - <tt>directory charset \<directory\> \<encoding\></tt>.
    class HELLOKIT_EDIT_EXPORT VoiceBankCommands {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::VoiceBankCommands)
    public:
        /// Executes the command of \a line on \a session in one transaction.
        ///
        /// \sa ProjectCommands::execute()
        static bool execute(VoiceBankSession &session, QStringView line,
                            DiagnosticList &diagnostics);

        /// Returns the names of the commands.
        ///
        /// \sa ProjectCommands::names()
        static QStringList names();

        /// Returns the domain function registry of a voice bank: each function of
        /// VoiceBankEdits by name, with the name of the command that calls it.
        ///
        /// \sa ProjectCommands::domainFunctions()
        static QMap<QString, QString> domainFunctions();
    };

}

#endif // HELLOKIT_EDIT_VOICEBANKCOMMANDS_H
