#include "ProjectEdits.h"

#include <algorithm>

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

    bool ProjectEdits::removeNotes(const NoteListRef &notes, const QList<int> &indices,
                                   DiagnosticList &diagnostics) {
        auto sorted = indices;
        std::sort(sorted.begin(), sorted.end());
        sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
        if (sorted.isEmpty()) {
            return true;
        }
        if (sorted.first() < 0 || sorted.last() >= notes.size()) {
            return fail(diagnostics, tr("The track has %1 notes, not a note %2.")
                                         .arg(notes.size())
                                         .arg(sorted.first() < 0 ? sorted.first() : sorted.last()));
        }

        // Each run of consecutive notes is one removal, the last run first, so that the indices
        // of the runs before it still hold.
        auto transaction = notes.session()->transaction(tr("Delete Notes"));
        int end = int(sorted.size());
        while (end > 0) {
            int begin = end - 1;
            while (begin > 0 && sorted[begin - 1] == sorted[begin] - 1) {
                --begin;
            }
            notes.remove(sorted[begin], end - begin);
            end = begin;
        }
        return transaction.commit(diagnostics);
    }

    bool ProjectEdits::setLength(const NoteRef &note, int ticks, DiagnosticList &diagnostics) {
        auto transaction = note.session()->transaction(tr("Change Length"));
        note.setLength(ticks);
        return transaction.commit(diagnostics);
    }

    bool ProjectEdits::moveNotes(const NoteListRef &notes, int index, int count, int destination,
                                 DiagnosticList &diagnostics) {
        const int size = notes.size();
        if (count <= 0 || index < 0 || index + count > size || destination < 0 ||
            destination + count > size) {
            return fail(diagnostics,
                        tr("A track of %1 notes cannot move %2 notes from position %3 to %4.")
                            .arg(size)
                            .arg(count)
                            .arg(index)
                            .arg(destination));
        }
        if (destination == index) {
            return true;
        }
        auto transaction = notes.session()->transaction(tr("Move Notes"));
        notes.move(index, count, destination);
        return transaction.commit(diagnostics);
    }
}
