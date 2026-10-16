#ifndef HELLOKIT_DOCUMENT_USTNOTEENTRIES_H
#define HELLOKIT_DOCUMENT_USTNOTEENTRIES_H

#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QMap>
#include <QtCore/QStringList>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Document/HelloKitDocumentGlobal.h>
#include <hellokit/Document/Note.h>

namespace hello::kit {

    /// The entries of a note that the note property dialog of UTAU does not edit, as lines of the
    /// form key=value, for the dialog that UTAU opens with the Others button.
    ///
    /// The built-in entries are those of a UST with a field of Note: the envelope, the vibrato,
    /// the pitch of both modes, the label, \c $direct, \c $patch and the regions. They are
    /// written and read as a UST in UTF-8 writes and reads them. The user data are the entries of
    /// Note::userData.
    class HELLOKIT_DOCUMENT_EXPORT UstNoteEntries {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::UstNoteEntries)
    public:
        /// Returns the built-in entries of \a note, one line each, in the order of a UST.
        static QStringList builtInLinesOf(const Note &note);

        /// Returns the user data of \a note, one line each, in the order of the keys.
        static QStringList userDataLinesOf(const Note &note);

        /// Returns \a note with its built-in entries replaced by those of \a lines, or
        /// \c std::nullopt with an error for each line that is not a built-in entry, repeats a
        /// key, or differs from the entry that a UST writes for the value read from the line.
        /// The parts of a value between \c , and \c ; are compared as numbers if both read as
        /// numbers, so that the formatting of a number does not matter. Trailing parts that are
        /// zero or \c % are not compared, because a UST may omit them. A value that the reading
        /// of a UST corrects or otherwise ignores, such as a part that is not a number, is
        /// therefore refused. Empty lines are skipped.
        static std::optional<Note> withBuiltInLines(Note note, const QStringList &lines,
                                                    DiagnosticList &diagnostics);

        /// Returns the user data of \a lines, the part of each line before the first \c = as the
        /// key and the rest as the value, or \c std::nullopt with an error for each line without
        /// \c =, with an empty key or a key that contains white space, with a key of an entry that
        /// UTAU reads, or with a repeated key. Empty lines are skipped.
        static std::optional<QMap<QString, QString>> userDataOf(const QStringList &lines,
                                                                DiagnosticList &diagnostics);

        /// Returns a warning for each entry of \a userData that UTAU drops or changes when it
        /// saves a file: a key without \c $, and a value that is empty or has \c =, a space or a
        /// tab.
        ///
        /// \sa docs/claude/utau-ust-preservation.md
        static DiagnosticList utauChangesOf(const QMap<QString, QString> &userData);
    };

}

#endif // HELLOKIT_DOCUMENT_USTNOTEENTRIES_H
