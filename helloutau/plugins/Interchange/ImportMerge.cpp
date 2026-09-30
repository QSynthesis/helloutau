#include "ImportMerge.h"

#include <QtCore/QStringList>

#include <hellokit/Document/Project.h>
#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Edit/ProjectSession.h>

namespace hello::daw {

    namespace {

        void report(kit::DiagnosticList &diagnostics, kit::DiagnosticSeverity severity,
                    const QString &message) {
            diagnostics.push_back({severity, message, std::nullopt});
        }

        // Returns the tempo in effect before notes[index]: the last explicit tempo of a preceding
        // note, or the project tempo if no preceding note specifies one.
        double tempoBefore(const QList<kit::Note> &notes, int index, double projectTempo) {
            for (int i = index - 1; i >= 0; --i) {
                if (notes[i].tempo) {
                    return *notes[i].tempo;
                }
            }
            return projectTempo;
        }

        bool sameTempo(double a, double b) {
            return qFuzzyCompare(a, b);
        }

    }

    std::optional<ImportMerge::Range> ImportMerge::merge(const kit::Project &imported,
                                                         const kit::ProjectRef &project,
                                                         const Options &options,
                                                         kit::DiagnosticList &diagnostics) {
        const auto current = project.toProject();
        if (imported.tracks.isEmpty() || current.tracks.isEmpty()) {
            report(diagnostics, kit::DiagnosticSeverity::Error,
                   tr("The imported file has no track."));
            return std::nullopt;
        }
        const auto &track = imported.tracks.first();
        const auto &existing = current.tracks.first().notes;
        const auto projectTempo = current.settings.tempo;

        // The discarded settings, reported in a single diagnostic
        QStringList dropped;
        if (!imported.settings.name.isEmpty()) {
            dropped.push_back(tr("the name \"%1\"").arg(imported.settings.name));
        }
        if (!track.name.isEmpty()) {
            dropped.push_back(tr("the track name \"%1\"").arg(track.name));
        }
        if (!track.voiceDir.isEmpty()) {
            dropped.push_back(tr("the voice folder \"%1\"").arg(track.voiceDir));
        }
        if (!imported.settings.flags.isEmpty()) {
            dropped.push_back(tr("the flags \"%1\"").arg(imported.settings.flags));
        }
        if (!dropped.isEmpty()) {
            report(diagnostics, kit::DiagnosticSeverity::Note,
                   tr("The settings of the imported project were discarded: %1.")
                       .arg(dropped.join(QStringLiteral(", "))));
        }

        QList<kit::Note> notes = track.notes;
        if (!options.keepLeadingRest) {
            qsizetype rests = 0;
            while (rests < notes.size() && notes[rests].isRest()) {
                ++rests;
            }
            notes.remove(0, rests);
        }
        if (notes.isEmpty()) {
            report(diagnostics, kit::DiagnosticSeverity::Warning,
                   tr("The imported file has no notes to insert."));
            return Range{};
        }

        // A project uses one pitch mode, so pitch data of the other mode is removed.
        bool pitchDropped = false;
        for (auto &note : notes) {
            if (current.settings.mode2 && note.pitchBend) {
                note.pitchBend.reset();
                pitchDropped = true;
            } else if (!current.settings.mode2 && !note.portamento.isEmpty()) {
                note.portamento.clear();
                pitchDropped = true;
            }
        }
        if (pitchDropped) {
            report(diagnostics, kit::DiagnosticSeverity::Warning,
                   current.settings.mode2
                       ? tr("The Mode1 pitch of the imported notes was removed because this "
                            "project uses Mode2.")
                       : tr("The Mode2 pitch of the imported notes was removed because this "
                            "project uses Mode1."));
        }

        int index = int(existing.size());
        int removed = 0;
        switch (options.position) {
            case After:
                index = options.first + options.count;
                break;
            case Before:
                index = options.first;
                break;
            case Replace:
                index = options.first;
                removed = options.count;
                break;
            case AtEnd:
                break;
        }
        if (index < 0 || removed < 0 || index + removed > existing.size()) {
            report(diagnostics, kit::DiagnosticSeverity::Error,
                   tr("The selection lies outside the track."));
            return std::nullopt;
        }

        const double tempoAt = tempoBefore(existing, index, projectTempo);
        if (options.keepTempo) {
            if (!notes.first().tempo && !sameTempo(imported.settings.tempo, tempoAt)) {
                notes.first().tempo = imported.settings.tempo;
            }
        } else {
            for (auto &note : notes) {
                note.tempo.reset();
            }
        }

        // The first note after the inserted notes. If the insertion changes the tempo in effect
        // before it, the note records its previous tempo explicitly.
        const int follower = index + removed;
        std::optional<double> followerTempo;
        if (follower < existing.size() && !existing[follower].tempo) {
            const double before = tempoBefore(existing, follower, projectTempo);
            const double after = tempoBefore(notes, int(notes.size()), tempoAt);
            if (!sameTempo(before, after)) {
                followerTempo = before;
            }
        }

        const auto list = project.tracks().at(0).notes();
        auto transaction = project.session()->transaction(tr("Import"));
        if (removed > 0) {
            list.remove(index, removed);
        }
        list.insert(index, notes);
        if (followerTempo) {
            list.at(index + int(notes.size())).setTempo(followerTempo);
        }
        if (!transaction.commit(diagnostics)) {
            return std::nullopt;
        }
        return Range{index, int(notes.size())};
    }

}
