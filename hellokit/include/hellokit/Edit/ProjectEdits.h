#ifndef HELLOKIT_EDIT_PROJECTEDITS_H
#define HELLOKIT_EDIT_PROJECTEDITS_H

#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QList>
#include <QtCore/QString>

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
    /// Changes to the properties of a project, the fields of the dialog of UTAU and the tempo:
    /// each field that is set replaces the property, the others stay.
    struct ProjectPropertyChanges {
        std::optional<QString> name;
        std::optional<double> tempo;
        std::optional<QString> flags;
        std::optional<QString> outputFile;

        /// The voice directory of the first track, as the file writes it
        std::optional<QString> voiceDir;

        /// \warning Untrusted, stored and never executed, see ProjectSettings::wavtool.
        std::optional<QString> wavtool;
        std::optional<QString> resampler;

        std::optional<bool> mode2;

        inline bool isEmpty() const {
            return !name && !tempo && !flags && !outputFile && !voiceDir && !wavtool &&
                   !resampler && !mode2;
        }
    };

    class HELLOKIT_EDIT_EXPORT ProjectEdits {
        Q_GADGET
        Q_DECLARE_TR_FUNCTIONS(hello::kit::ProjectEdits)
    public:
        /// The values of a note that setParameter() sets
        enum NoteParameter {
            Intensity,
            Modulation,
            Velocity,
        };
        Q_ENUM(NoteParameter)

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

        /// Inserts \a inserted in their order before the note at \a index of \a notes, or after
        /// the last note if \a index is the number of notes. The following notes start later by
        /// the length of \a inserted. No note is divided. Every field of an inserted note is
        /// written as it is, its tempo included.
        Q_INVOKABLE static bool insertNotes(const NoteListRef &notes, int index,
                                            const QList<Note> &inserted,
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

        /// Replaces the Mode2 points of \a note with \a points. Only the points that differ are
        /// written, so that the others keep their identity; points beyond the new ones are
        /// removed, and new ones appended. The points must keep their order: from the second
        /// on, none precedes the one before it.
        Q_INVOKABLE static bool setPortamento(const NoteRef &note,
                                              const QList<PortamentoPoint> &points,
                                              DiagnosticList &diagnostics);

        /// Sets the vibrato of each of \a notes to \a vibrato, or removes it where \a vibrato is
        /// empty. The notes must belong to one session.
        Q_INVOKABLE static bool setVibrato(const QList<NoteRef> &notes,
                                           const std::optional<Vibrato> &vibrato,
                                           DiagnosticList &diagnostics);

        /// Sets the envelope of each of \a notes to \a envelope, or removes it where
        /// \a envelope is empty, which leaves the default of UTAU. The notes must belong to one
        /// session.
        Q_INVOKABLE static bool setEnvelope(const QList<NoteRef> &notes,
                                            const std::optional<Envelope> &envelope,
                                            DiagnosticList &diagnostics);

        /// Multiplies the height of each Mode2 point of each of \a notes by \a portamento, and
        /// the depth of its vibrato by \a vibrato. The results are rounded to whole cents. The
        /// factors must not be negative. The notes must belong to one session.
        Q_INVOKABLE static bool scalePitch(const QList<NoteRef> &notes, double portamento,
                                           double vibrato, DiagnosticList &diagnostics);

        /// Sets \a parameter of each of \a notes to \a value, or removes it where \a value is
        /// empty, which leaves the default of UTAU. The value is written even if it equals the
        /// default. The notes must belong to one session.
        Q_INVOKABLE static bool setParameter(const QList<NoteRef> &notes, NoteParameter parameter,
                                             std::optional<double> value,
                                             DiagnosticList &diagnostics);

        /// Draws \a values into the Mode1 values of the note at \a index of \a notes, the first
        /// at \a tick from the start of the note and the others every five ticks after it, as
        /// PitchBend::drawn() gives them at the tempo of the note. See step 5 in
        /// docs/Tuning.md.
        Q_INVOKABLE static bool drawPitchBend(const NoteListRef &notes, int index, double tick,
                                              const QList<double> &values,
                                              DiagnosticList &diagnostics);

        /// Turns Mode2 of the project of \a settings on or off: which of the Mode2 points and
        /// vibratos, or the Mode1 values, the synthesis uses, as UTAU does. The other is kept.
        Q_INVOKABLE static bool setMode2(const SettingsRef &settings, bool mode2,
                                         DiagnosticList &diagnostics);

        /// Changes the properties of \a project that \a changes sets, in one step, and makes no
        /// step if none differs. The voice directory is that of the first track.
        Q_INVOKABLE static bool setProperties(const ProjectRef &project,
                                              const ProjectPropertyChanges &changes,
                                              DiagnosticList &diagnostics);
    };

}

#endif // HELLOKIT_EDIT_PROJECTEDITS_H
