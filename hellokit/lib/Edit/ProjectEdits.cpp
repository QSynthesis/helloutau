#include "ProjectEdits.h"

#include <algorithm>
#include <cmath>

#include <hellokit/Document/DocumentConstants.h>
#include <hellokit/Document/TempoMap.h>

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

    bool ProjectEdits::insertNotes(const NoteListRef &notes, int index, const QList<Note> &inserted,
                                   DiagnosticList &diagnostics) {
        if (index < 0 || index > notes.size()) {
            return fail(diagnostics, tr("A track of %1 notes has no position %2 for a new note.")
                                         .arg(notes.size())
                                         .arg(index + 1));
        }
        if (inserted.isEmpty()) {
            return true;
        }
        auto transaction = notes.session()->transaction(inserted.size() == 1 ? tr("Insert Note")
                                                                             : tr("Insert Notes"));
        notes.insert(index, inserted);
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

    bool ProjectEdits::setPortamento(const NoteRef &note, const QList<PortamentoPoint> &points,
                                     DiagnosticList &diagnostics) {
        auto transaction = note.session()->transaction(tr("Change Pitch"));
        const auto list = note.portamento();
        const int kept = std::min(list.size(), int(points.size()));
        for (int i = 0; i < kept; ++i) {
            const auto point = list.at(i);
            const auto &wanted = points[i];
            if (point.x() != wanted.x) {
                point.setX(wanted.x);
            }
            if (point.y() != wanted.y) {
                point.setY(wanted.y);
            }
            if (point.type() != wanted.type) {
                point.setType(wanted.type);
            }
        }
        if (list.size() > kept) {
            list.remove(kept, list.size() - kept);
        } else if (points.size() > kept) {
            list.insert(kept, points.mid(kept));
        }
        return transaction.commit(diagnostics);
    }

    bool ProjectEdits::setVibrato(const QList<NoteRef> &notes,
                                  const std::optional<Vibrato> &vibrato,
                                  DiagnosticList &diagnostics) {
        if (notes.isEmpty()) {
            return true;
        }
        auto transaction = notes.first().session()->transaction(tr("Change Vibrato"));
        for (const auto &note : notes) {
            if (note.vibrato() != vibrato) {
                note.setVibrato(vibrato);
            }
        }
        return transaction.commit(diagnostics);
    }

    bool ProjectEdits::setEnvelope(const QList<NoteRef> &notes,
                                   const std::optional<Envelope> &envelope,
                                   DiagnosticList &diagnostics) {
        if (notes.isEmpty()) {
            return true;
        }
        auto transaction = notes.first().session()->transaction(tr("Change Envelope"));
        for (const auto &note : notes) {
            if (note.envelope() != envelope) {
                note.setEnvelope(envelope);
            }
        }
        return transaction.commit(diagnostics);
    }

    bool ProjectEdits::scalePitch(const QList<NoteRef> &notes, double portamento, double vibrato,
                                  DiagnosticList &diagnostics) {
        if (portamento < 0 || vibrato < 0) {
            return fail(diagnostics,
                        tr("The pitch cannot be scaled by a negative factor, %1 or %2.")
                            .arg(portamento)
                            .arg(vibrato));
        }
        if (notes.isEmpty()) {
            return true;
        }
        auto transaction = notes.first().session()->transaction(tr("Scale Pitch"));
        for (const auto &note : notes) {
            const auto points = note.portamento();
            for (int i = 0; i < points.size(); ++i) {
                const auto point = points.at(i);
                const double y = std::round(point.y() * portamento);
                if (point.y() != y) {
                    point.setY(y);
                }
            }
            if (auto value = note.vibrato()) {
                const double amplitude = std::round(value->amplitude * vibrato);
                if (value->amplitude != amplitude) {
                    value->amplitude = amplitude;
                    note.setVibrato(value);
                }
            }
        }
        return transaction.commit(diagnostics);
    }

    bool ProjectEdits::setParameter(const QList<NoteRef> &notes, NoteParameter parameter,
                                    std::optional<double> value, DiagnosticList &diagnostics) {
        if (notes.isEmpty()) {
            return true;
        }
        const char *messages[] = {
            QT_TR_NOOP("Change Intensity"),
            QT_TR_NOOP("Change Modulation"),
            QT_TR_NOOP("Change Velocity"),
        };
        auto transaction = notes.first().session()->transaction(tr(messages[parameter]));
        for (const auto &note : notes) {
            switch (parameter) {
                case Intensity:
                    if (note.intensity() != value) {
                        note.setIntensity(value);
                    }
                    break;
                case Modulation:
                    if (note.modulation() != value) {
                        note.setModulation(value);
                    }
                    break;
                case Velocity:
                    if (note.velocity() != value) {
                        note.setVelocity(value);
                    }
                    break;
            }
        }
        return transaction.commit(diagnostics);
    }

    bool ProjectEdits::drawPitchBend(const NoteListRef &notes, int index, double tick,
                                     const QList<double> &values, DiagnosticList &diagnostics) {
        if (index < 0 || index >= notes.size()) {
            return fail(diagnostics,
                        tr("The track has %1 notes, not a note %2.").arg(notes.size()).arg(index));
        }
        if (values.isEmpty()) {
            return true;
        }

        // The tempo in effect for the note
        TempoMap tempos(ProjectRef(notes.session()).settings().tempo());
        for (int i = 0; i <= index; ++i) {
            const auto note = notes.at(i);
            tempos.append(note.length(), note.tempo());
        }
        const auto note = notes.at(index);
        const auto current = note.toNote();
        const auto &before = current.pitchBend;
        PreviousBend previous;
        if (index > 0) {
            previous = PreviousBend::of(notes.at(index - 1).toNote(), current);
        }
        const auto after = PitchBend::drawn(before, previous, tempos.tempo(index), tick, values);
        if (after == before) {
            return true;
        }

        auto transaction = notes.session()->transaction(tr("Draw Pitch"));
        const auto bend = note.pitchBend();
        if (!bend.isValid()) {
            note.setPitchBend(after);
            return transaction.commit(diagnostics);
        }
        if (bend.start() != after.start) {
            bend.setStart(after.start);
        }
        // A drawing never shortens the values.
        const int size = bend.valuesSize();
        const auto replaced = after.values.mid(0, size);
        if (replaced != before->values) {
            bend.replaceValues(0, replaced);
        }
        if (after.values.size() > size) {
            bend.insertValues(size, after.values.mid(size));
        }
        return transaction.commit(diagnostics);
    }

    bool ProjectEdits::setMode2(const SettingsRef &settings, bool mode2,
                                DiagnosticList &diagnostics) {
        if (settings.mode2() == mode2) {
            return true;
        }
        auto transaction =
            settings.session()->transaction(mode2 ? tr("Turn Mode2 On") : tr("Turn Mode2 Off"));
        settings.setMode2(mode2);
        return transaction.commit(diagnostics);
    }
}
