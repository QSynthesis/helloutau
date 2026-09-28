#ifndef HELLOKIT_EDIT_PROJECTEDITS_H
#define HELLOKIT_EDIT_PROJECTEDITS_H

#include <QtCore/QCoreApplication>
#include <QtCore/QList>

#include <hellokit/Document/Note.h>
#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Edit/HelloKitEditGlobal.h>
#include <hellokit/Edit/ProjectRefs.h>

namespace hello::kit {

    /// The domain functions of a project: the modifications that a user performs as one
    /// operation, each one undo step.
    ///
    /// Each function performs its modifications in a transaction with the name of the operation
    /// as its message. Called within another transaction, the function joins it. Each function
    /// returns whether its transaction is committed, and reports the reason otherwise in
    /// \a diagnostics.
    ///
    /// Each function is Q_INVOKABLE, which lists it in the meta-object of the class, and has a
    /// command.
    ///
    /// \sa EditSession::Transaction, ProjectCommands::domainFunctions()
    class HELLOKIT_EDIT_EXPORT ProjectEdits {
        Q_GADGET
        Q_DECLARE_TR_FUNCTIONS(hello::kit::ProjectEdits)
    public:
        /// Adds \a semitones to the note number of each of \a notes, rests included. The notes
        /// must belong to one session.
        Q_INVOKABLE static bool transpose(const QList<NoteRef> &notes, int semitones,
                                          DiagnosticList &diagnostics);

        /// Splits the note at \a index of \a notes after \a ticks, which must be inside the note.
        ///
        /// The note keeps its fields and is shortened to \a ticks. The note inserted after it
        /// has the remaining length, the note number of the note and the default lyric, and no
        /// other field.
        Q_INVOKABLE static bool splitNote(const NoteListRef &notes, int index, int ticks,
                                          DiagnosticList &diagnostics);

        /// Inserts \a note before the note at \a index of \a notes, or after the last note if
        /// \a index is the number of notes. The following notes start later by the length of
        /// \a note. No note is divided.
        Q_INVOKABLE static bool insertNote(const NoteListRef &notes, int index, const Note &note,
                                           DiagnosticList &diagnostics);

        /// Sets the tempo of \a note, which applies from the note until the next note with a
        /// tempo. The tempo is written even if it equals the tempo already in effect.
        Q_INVOKABLE static bool setTempo(const NoteRef &note, double tempo,
                                         DiagnosticList &diagnostics);

        /// Removes the notes at \a indices of \a notes, in any order. The following notes start
        /// earlier by the length removed. Every field of a removed note goes with it, its tempo
        /// included, so the tempo in effect before it continues.
        Q_INVOKABLE static bool removeNotes(const NoteListRef &notes, const QList<int> &indices,
                                            DiagnosticList &diagnostics);

        /// Sets the length of \a note to \a ticks. The following notes start earlier or later by
        /// the difference.
        Q_INVOKABLE static bool setLength(const NoteRef &note, int ticks,
                                          DiagnosticList &diagnostics);

        /// Moves the \a count notes from \a index of \a notes so that the first of them is at
        /// \a destination, an index of the list after the move.
        ///
        /// The notes are reordered in the sequence and no length changes, so the track keeps its
        /// length. Every field moves with its note, its tempo included, so a tempo change moves
        /// with the note that sets it. See step 4 in docs/Widgets.md.
        Q_INVOKABLE static bool moveNotes(const NoteListRef &notes, int index, int count,
                                          int destination, DiagnosticList &diagnostics);
    };

}

#endif // HELLOKIT_EDIT_PROJECTEDITS_H
