#include "ProjectEdits.h"

#include <hellokit/Document/DocumentConstants.h>

namespace hello::kit {

    namespace {

        bool fail(DiagnosticList &diagnostics, const QString &message) {
            Diagnostic diagnostic;
            diagnostic.severity = DiagnosticSeverity::Error;
            diagnostic.message = message;
            diagnostics.push_back(diagnostic);
            return false;
        }

    }

    bool ProjectEdits::transpose(const QList<NoteRef> &notes, int semitones,
                                 DiagnosticList &diagnostics) {
        if (notes.isEmpty() || semitones == 0) {
            return true;
        }
        auto transaction = notes.first().session()->transaction(tr("Transpose"));
        for (const auto &note : notes) {
            note.setNoteNum(note.noteNum() + semitones);
        }
        return transaction.commit(diagnostics);
    }

    bool ProjectEdits::splitNote(const NoteListRef &notes, int index, int ticks,
                                 DiagnosticList &diagnostics) {
        const auto note = notes.at(index);
        const int length = note.length();
        if (ticks <= 0 || ticks >= length) {
            return fail(
                diagnostics,
                tr("A note of %1 ticks cannot be split after %2 ticks.").arg(length).arg(ticks));
        }

        auto transaction = notes.session()->transaction(tr("Split Note"));
        Note second;
        second.lyric = QString::fromLatin1(defaultLyric);
        second.length = length - ticks;
        second.noteNum = note.noteNum();
        note.setLength(ticks);
        notes.insert(index + 1, {second});
        return transaction.commit(diagnostics);
    }

    bool ProjectEdits::insertNote(const NoteListRef &notes, int index, const Note &note,
                                  DiagnosticList &diagnostics) {
        if (index < 0 || index > notes.size()) {
            return fail(diagnostics, tr("A track of %1 notes has no position %2 for a new note.")
                                         .arg(notes.size())
                                         .arg(index + 1));
        }
        auto transaction = notes.session()->transaction(tr("Insert Note"));
        notes.insert(index, {note});
        return transaction.commit(diagnostics);
    }

    bool ProjectEdits::setTempo(const NoteRef &note, double tempo, DiagnosticList &diagnostics) {
        auto transaction = note.session()->transaction(tr("Change Tempo"));
        note.setTempo(tempo);
        return transaction.commit(diagnostics);
    }

}
