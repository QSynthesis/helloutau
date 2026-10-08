#include <QtTest/QTest>

#include <hellokit/Document/DocumentConstants.h>
#include <hellokit/Edit/ProjectEdits.h>
#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Edit/ProjectSession.h>

#include "ProjectSamples.h"

using namespace hello::kit;

class test_ProjectEdits : public QObject {
    Q_OBJECT

private:
    static NoteListRef notesOf(ProjectSession &session) {
        return ProjectRef(&session).tracks().at(0).notes();
    }

    // Notes a to e of different lengths, where b sets the tempo 150
    static Project fiveNotes() {
        Track track;
        int length = 120;
        for (const auto lyric : {"a", "b", "c", "d", "e"}) {
            Note note;
            note.lyric = QString::fromLatin1(lyric);
            note.length = length;
            note.noteNum = 60;
            track.notes.push_back(note);
            length += 120;
        }
        track.notes[1].tempo = 150;
        Project project;
        project.tracks.push_back(track);
        return project;
    }

    static QString lyricsOf(const ProjectSession &session) {
        const auto project = session.snapshot();
        QStringList lyrics;
        for (const auto &note : project.tracks[0].notes) {
            lyrics.push_back(note.lyric);
        }
        return lyrics.join(u' ');
    }

private Q_SLOTS:
    // Rests are transposed as well, and the whole operation is one undo step.
    void transposition_includes_rests_and_is_one_step() {
        const auto project = richProject();
        ProjectSession session(project);
        const auto notes = notesOf(session);

        DiagnosticList diagnostics;
        QVERIFY(ProjectEdits::transpose({notes.at(0), notes.at(1)}, 2, diagnostics));
        QCOMPARE(notes.at(0).noteNum(), 62);
        QCOMPARE(notes.at(1).noteNum(), 62); // the rest
        QCOMPARE(session.currentStep(), 1);
        QCOMPARE(session.undoMessage(), ProjectEdits::tr("Transpose"));

        session.undo();
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    // A transposition beyond the range of note numbers violates a constraint, therefore none of
    // the notes changes.
    void a_transposition_out_of_range_changes_nothing() {
        const auto project = richProject();
        ProjectSession session(project);
        const auto notes = notesOf(session);

        DiagnosticList diagnostics;
        QVERIFY(!ProjectEdits::transpose({notes.at(0), notes.at(1)}, 70, diagnostics));
        QCOMPARE(diagnostics.size(), 2);
        QCOMPARE(session.snapshot().toJson(), project.toJson());
        QVERIFY(!session.canUndo());
    }

    void a_split_note_keeps_its_fields_and_is_followed_by_a_new_note() {
        const auto project = richProject();
        ProjectSession session(project);
        const auto notes = notesOf(session);

        DiagnosticList diagnostics;
        QVERIFY(ProjectEdits::splitNote(notes, 0, 120, diagnostics));
        QCOMPARE(notes.size(), 3);

        auto first = project.tracks[0].notes[0];
        first.length = 120;
        auto expected = project;
        expected.tracks[0].notes[0] = first;
        Note second;
        second.lyric = QString::fromLatin1(defaultLyric);
        second.length = 360;
        second.noteNum = 60;
        expected.tracks[0].notes.insert(1, second);
        QCOMPARE(session.snapshot().toJson(), expected.toJson());

        QCOMPARE(session.undoMessage(), ProjectEdits::tr("Split Note"));
        session.undo();
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    void a_split_outside_the_note_is_refused() {
        const auto project = richProject();
        ProjectSession session(project);
        const auto notes = notesOf(session);

        // The position is checked before the transaction, which reports the position rather than
        // the resulting note of length 0.
        for (const int ticks : {0, 480, 600, -1}) {
            DiagnosticList diagnostics;
            QVERIFY(!ProjectEdits::splitNote(notes, 0, ticks, diagnostics));
            QCOMPARE(diagnostics.size(), 1);
            QCOMPARE(diagnostics.first().message,
                     ProjectEdits::tr("A note of %1 ticks cannot be split after %2 ticks.")
                         .arg(480)
                         .arg(ticks));
        }
        QCOMPARE(session.snapshot().toJson(), project.toJson());
        QVERIFY(!session.canUndo());
    }

    // A note is inserted at a boundary between notes, and no existing note changes.
    void a_note_is_inserted_between_notes() {
        const auto project = richProject();
        ProjectSession session(project);
        const auto notes = notesOf(session);

        Note added;
        added.lyric = QStringLiteral("u");
        added.length = 240;
        added.noteNum = 64;

        DiagnosticList diagnostics;
        QVERIFY(ProjectEdits::insertNotes(notes, 1, {added}, diagnostics));
        QVERIFY(ProjectEdits::insertNotes(notes, 3, {added}, diagnostics));
        QVERIFY(ProjectEdits::insertNotes(notes, 0, {added}, diagnostics));

        auto expected = project;
        auto &expectedNotes = expected.tracks[0].notes;
        expectedNotes.insert(1, added);
        expectedNotes.insert(3, added);
        expectedNotes.insert(0, added);
        QCOMPARE(session.snapshot().toJson(), expected.toJson());
        QCOMPARE(session.currentStep(), 3);
        QCOMPARE(session.undoMessage(), ProjectEdits::tr("Insert Note"));
    }

    void an_insertion_outside_the_track_is_refused() {
        ProjectSession session(richProject());
        const auto notes = notesOf(session);
        Note added;
        added.length = 240;

        DiagnosticList diagnostics;
        QVERIFY(!ProjectEdits::insertNotes(notes, 3, {added}, diagnostics));
        QVERIFY(!ProjectEdits::insertNotes(notes, -1, {added}, diagnostics));
        QCOMPARE(diagnostics.size(), 2);

        // A note that violates a constraint is refused as well.
        added.length = 0;
        diagnostics.clear();
        QVERIFY(!ProjectEdits::insertNotes(notes, 0, {added}, diagnostics));
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(notes.size(), 2);
    }

    // The tempo is written to the note even if it equals the tempo in effect there.
    void a_tempo_is_written_even_if_it_is_in_effect() {
        auto project = richProject();
        project.tracks[0].notes[1].tempo = std::nullopt;
        ProjectSession session(project);
        const auto notes = notesOf(session);

        DiagnosticList diagnostics;
        QVERIFY(ProjectEdits::setTempo(notes.at(1), 128, diagnostics));
        QCOMPARE(notes.at(1).tempo(), std::optional<double>(128));
        QCOMPARE(session.undoMessage(), ProjectEdits::tr("Change Tempo"));

        QVERIFY(!ProjectEdits::setTempo(notes.at(1), 0, diagnostics));
        QCOMPARE(notes.at(1).tempo(), std::optional<double>(128));
    }

    // A domain function called within a transaction joins it, so that a composed operation is
    // one undo step.
    void domain_functions_compose_into_one_step() {
        const auto project = richProject();
        ProjectSession session(project);
        const auto notes = notesOf(session);

        auto transaction = session.transaction(QStringLiteral("Split and transpose"));
        DiagnosticList diagnostics;
        QVERIFY(ProjectEdits::splitNote(notes, 0, 240, diagnostics));
        QVERIFY(ProjectEdits::transpose({notes.at(0), notes.at(1)}, -12, diagnostics));
        QVERIFY(transaction.commit(diagnostics));

        QCOMPARE(session.currentStep(), 1);
        QCOMPARE(session.undoMessage(), QStringLiteral("Split and transpose"));
        QCOMPARE(notes.at(1).noteNum(), 48);
        session.undo();
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    // Removed in one step whatever the order of the indices, and the following notes close up.
    void notes_are_removed_in_one_step() {
        const auto project = fiveNotes();
        ProjectSession session(project);
        const auto notes = notesOf(session);

        DiagnosticList diagnostics;
        QVERIFY(ProjectEdits::removeNotes(notes, {3, 0, 1, 3}, diagnostics));
        QCOMPARE(lyricsOf(session), QStringLiteral("c e"));
        QCOMPARE(session.currentStep(), 1);
        QCOMPARE(session.undoMessage(), ProjectEdits::tr("Delete Notes"));
        session.undo();
        QCOMPARE(session.snapshot().toJson(), project.toJson());

        // Removing nothing creates no step, and so keeps the redo history.
        QVERIFY(ProjectEdits::removeNotes(notes, {}, diagnostics));
        QCOMPARE(session.currentStep(), 0);
        QVERIFY(session.canRedo());
        QVERIFY(!ProjectEdits::removeNotes(notes, {1, 5}, diagnostics));
        QVERIFY(!ProjectEdits::removeNotes(notes, {-1}, diagnostics));
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    void a_length_is_set_within_its_range() {
        const auto project = fiveNotes();
        ProjectSession session(project);
        const auto notes = notesOf(session);

        DiagnosticList diagnostics;
        QVERIFY(ProjectEdits::setLength(notes.at(2), 960, diagnostics));
        QCOMPARE(notes.at(2).length(), 960);
        QCOMPARE(session.undoMessage(), ProjectEdits::tr("Change Length"));

        QVERIFY(!ProjectEdits::setLength(notes.at(2), 0, diagnostics));
        QCOMPARE(notes.at(2).length(), 960);
    }

    // Reordering changes no length, so the track keeps its length, and a tempo moves with the
    // note that sets it.
    void moved_notes_keep_their_lengths_and_tempos() {
        const auto project = fiveNotes();
        ProjectSession session(project);
        const auto notes = notesOf(session);

        DiagnosticList diagnostics;
        QVERIFY(ProjectEdits::moveNotes(notes, 1, 2, 3, diagnostics));
        QCOMPARE(lyricsOf(session), QStringLiteral("a d e b c"));
        auto expected = project;
        expected.tracks[0].notes.move(1, 4);
        expected.tracks[0].notes.move(1, 4);
        QCOMPARE(session.snapshot().toJson(), expected.toJson());
        QCOMPARE(notes.at(3).tempo(), std::optional<double>(150));
        QCOMPARE(session.undoMessage(), ProjectEdits::tr("Move Notes"));

        session.undo();
        QVERIFY(ProjectEdits::moveNotes(notes, 3, 2, 0, diagnostics));
        QCOMPARE(lyricsOf(session), QStringLiteral("d e a b c"));

        // Moving to where the notes are creates no step.
        const int step = session.currentStep();
        QVERIFY(ProjectEdits::moveNotes(notes, 1, 2, 1, diagnostics));
        QCOMPARE(session.currentStep(), step);

        QVERIFY(!ProjectEdits::moveNotes(notes, 4, 2, 0, diagnostics));
        QVERIFY(!ProjectEdits::moveNotes(notes, 0, 2, 4, diagnostics));
        QVERIFY(!ProjectEdits::moveNotes(notes, 0, 0, 1, diagnostics));
        QCOMPARE(lyricsOf(session), QStringLiteral("d e a b c"));
    }

    // The points that stay keep their nodes; the list grows and shrinks at its end; the order
    // of the points is a constraint.
    void the_points_of_a_note_are_replaced_in_one_step() {
        const auto project = fiveNotes();
        ProjectSession session(project);
        const auto note = notesOf(session).at(2);

        const auto point = [](double x, double y, PortamentoPoint::Type type) {
            PortamentoPoint p;
            p.x = x;
            p.y = y;
            p.type = type;
            return p;
        };
        DiagnosticList diagnostics;
        const QList<PortamentoPoint> three = {point(-40, -100, PortamentoPoint::S),
                                              point(0, 50, PortamentoPoint::R),
                                              point(60, 0, PortamentoPoint::J)};
        QVERIFY(ProjectEdits::setPortamento(note, three, diagnostics));
        QCOMPARE(session.snapshot().tracks[0].notes[2].portamento, three);
        QCOMPARE(session.undoMessage(), ProjectEdits::tr("Change Pitch"));

        const auto first = note.portamento().at(0).id();
        const QList<PortamentoPoint> two = {point(-40, -100, PortamentoPoint::S),
                                            point(30, 0, PortamentoPoint::Linear)};
        QVERIFY(ProjectEdits::setPortamento(note, two, diagnostics));
        QCOMPARE(session.snapshot().tracks[0].notes[2].portamento, two);
        QCOMPARE(note.portamento().at(0).id(), first);

        // Unchanged points create no step.
        const int step = session.currentStep();
        QVERIFY(ProjectEdits::setPortamento(note, two, diagnostics));
        QCOMPARE(session.currentStep(), step);

        QVERIFY(!ProjectEdits::setPortamento(
            note, {point(0, 0, PortamentoPoint::S), point(-10, 0, PortamentoPoint::S)},
            diagnostics));
        QCOMPARE(session.snapshot().tracks[0].notes[2].portamento, two);

        session.undo();
        session.undo();
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    void an_envelope_is_set_on_several_notes_and_removed() {
        const auto project = fiveNotes();
        ProjectSession session(project);
        const auto notes = notesOf(session);

        const auto envelope = Envelope::fromTimeOrder({
            {0,  0  },
            {5,  100},
            {35, 90 },
            {10, 0  }
        });
        QVERIFY(envelope);
        DiagnosticList diagnostics;
        QVERIFY(ProjectEdits::setEnvelope({notes.at(0), notes.at(4)}, envelope, diagnostics));
        QCOMPARE(notes.at(0).envelope(), envelope);
        QCOMPARE(notes.at(4).envelope(), envelope);
        QVERIFY(!notes.at(2).envelope());
        QCOMPARE(session.undoMessage(), ProjectEdits::tr("Change Envelope"));

        QVERIFY(ProjectEdits::setEnvelope({notes.at(0)}, std::nullopt, diagnostics));
        QVERIFY(!notes.at(0).envelope());
        session.undo();
        session.undo();
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    void a_vibrato_is_set_on_several_notes_and_removed() {
        const auto project = fiveNotes();
        ProjectSession session(project);
        const auto notes = notesOf(session);

        Vibrato vibrato;
        vibrato.length = 65;
        vibrato.period = 180;
        vibrato.amplitude = 35;
        DiagnosticList diagnostics;
        QVERIFY(ProjectEdits::setVibrato({notes.at(1), notes.at(3)}, vibrato, diagnostics));
        QCOMPARE(notes.at(1).vibrato(), std::optional<Vibrato>(vibrato));
        QCOMPARE(notes.at(3).vibrato(), std::optional<Vibrato>(vibrato));
        QVERIFY(!notes.at(2).vibrato());
        QCOMPARE(session.undoMessage(), ProjectEdits::tr("Change Vibrato"));
        QCOMPARE(session.currentStep(), 1);

        QVERIFY(ProjectEdits::setVibrato({notes.at(1)}, std::nullopt, diagnostics));
        QVERIFY(!notes.at(1).vibrato());
        session.undo();
        session.undo();
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    // Several notes are inserted in their order, as one step.
    void several_notes_are_inserted_in_their_order() {
        const auto project = richProject();
        ProjectSession session(project);
        const auto notes = notesOf(session);

        Note a;
        a.lyric = QStringLiteral("a");
        a.length = 240;
        a.noteNum = 64;
        Note b = a;
        b.lyric = QStringLiteral("b");
        b.tempo = 150;

        DiagnosticList diagnostics;
        QVERIFY(ProjectEdits::insertNotes(notes, 1, {a, b}, diagnostics));
        auto expected = project;
        expected.tracks[0].notes.insert(1, a);
        expected.tracks[0].notes.insert(2, b);
        QCOMPARE(session.snapshot().toJson(), expected.toJson());
        QCOMPARE(session.currentStep(), 1);
        QCOMPARE(session.undoMessage(), ProjectEdits::tr("Insert Notes"));

        // Nothing to insert is no step.
        QVERIFY(ProjectEdits::insertNotes(notes, 0, {}, diagnostics));
        QCOMPARE(session.currentStep(), 1);
    }

    // The heights of the points and the depth of the vibrato are scaled, to whole cents.
    void the_pitch_is_scaled() {
        const auto project = richProject();
        ProjectSession session(project);
        const auto notes = notesOf(session);

        DiagnosticList diagnostics;
        QVERIFY(ProjectEdits::scalePitch({notes.at(0), notes.at(1)}, 1.5, 2, diagnostics));
        const auto note = session.snapshot().tracks[0].notes[0];
        QList<double> heights;
        for (const auto &point : note.portamento) {
            heights.push_back(point.y);
        }
        QCOMPARE(heights, (QList<double>{0, -8, 5, 0}));
        QCOMPARE(note.portamento[1].x, 20.0);
        QCOMPARE(note.vibrato->amplitude, 70.0);
        QCOMPARE(note.vibrato->period, 180.0);
        QCOMPARE(session.currentStep(), 1);
        QCOMPARE(session.undoMessage(), ProjectEdits::tr("Scale Pitch"));
        session.undo();
        QCOMPARE(session.snapshot().toJson(), project.toJson());

        QVERIFY(!ProjectEdits::scalePitch({notes.at(0)}, -1, 1, diagnostics));
        QVERIFY(!ProjectEdits::scalePitch({notes.at(0)}, 1, -1, diagnostics));
        QCOMPARE(diagnostics.size(), 2);
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    // Each value is set on the notes given, or removed, in one step; the others stay.
    void a_parameter_is_set_on_several_notes_and_removed() {
        const auto project = richProject();
        ProjectSession session(project);
        const auto notes = notesOf(session);

        DiagnosticList diagnostics;
        QVERIFY(ProjectEdits::setParameter({notes.at(0), notes.at(1)}, ProjectEdits::Modulation, 40,
                                           diagnostics));
        auto snapshot = session.snapshot().tracks[0].notes;
        QCOMPARE(snapshot[0].modulation, std::optional<double>(40));
        QCOMPARE(snapshot[1].modulation, std::optional<double>(40));
        QCOMPARE(snapshot[0].intensity, project.tracks[0].notes[0].intensity);
        QCOMPARE(snapshot[0].velocity, project.tracks[0].notes[0].velocity);
        QCOMPARE(session.currentStep(), 1);
        QCOMPARE(session.undoMessage(), ProjectEdits::tr("Change Modulation"));

        QVERIFY(
            ProjectEdits::setParameter({notes.at(0)}, ProjectEdits::Intensity, 120, diagnostics));
        QVERIFY(ProjectEdits::setParameter({notes.at(0)}, ProjectEdits::Velocity, std::nullopt,
                                           diagnostics));
        snapshot = session.snapshot().tracks[0].notes;
        QCOMPARE(snapshot[0].intensity, std::optional<double>(120));
        QVERIFY(!snapshot[0].velocity);
        QCOMPARE(session.undoMessage(), ProjectEdits::tr("Change Velocity"));
        session.undo();
        session.undo();
        session.undo();
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    // Values beyond those UTAU accepts are kept, within -1000 and 1000.
    void a_parameter_is_kept_within_a_thousand_either_way() {
        const auto project = richProject();
        ProjectSession session(project);
        const auto notes = notesOf(session);

        DiagnosticList diagnostics;
        for (const auto parameter :
             {ProjectEdits::Intensity, ProjectEdits::Modulation, ProjectEdits::Velocity}) {
            QVERIFY(ProjectEdits::setParameter({notes.at(0)}, parameter, -1000, diagnostics));
            QVERIFY(ProjectEdits::setParameter({notes.at(0)}, parameter, 1000, diagnostics));
            QVERIFY(!ProjectEdits::setParameter({notes.at(0)}, parameter, 1000.5, diagnostics));
            QVERIFY(!ProjectEdits::setParameter({notes.at(0)}, parameter, -1001, diagnostics));
        }
        QCOMPARE(diagnostics.size(), 6);
        const auto note = session.snapshot().tracks[0].notes[0];
        QCOMPARE(note.intensity, std::optional<double>(1000));
        QCOMPARE(note.modulation, std::optional<double>(1000));
        QCOMPARE(note.velocity, std::optional<double>(1000));
    }

    // Two notes at 125 BPM, where a tick is a millisecond; the first bends up to 60 cents at
    // its end.
    static Project bentNotes() {
        Project project;
        project.settings.tempo = 125;
        Track track;
        Note first;
        first.lyric = QStringLiteral("a");
        first.length = 480;
        first.noteNum = 60;
        first.pitchBend = PitchBend{
            450, {40, 60}
        };
        Note second = first;
        second.pitchBend.reset();
        track.notes = {first, second};
        project.tracks.push_back(track);
        return project;
    }

    static std::optional<PitchBend> bendOf(const ProjectSession &session, int index) {
        return session.snapshot().tracks[0].notes[index].pitchBend;
    }

    // A note without Mode1 values starts them where they are drawn.
    void mode1_values_start_where_first_drawn() {
        const auto project = bentNotes();
        ProjectSession session(project);
        const auto notes = notesOf(session);

        DiagnosticList diagnostics;
        QVERIFY(ProjectEdits::drawPitchBend(notes, 1, -20.0004, {1, 2, 3}, diagnostics));
        QCOMPARE(bendOf(session, 1), std::optional(PitchBend{
                                         -20, {1, 2, 3}
        }));
        QCOMPARE(session.currentStep(), 1);
        QCOMPARE(session.undoMessage(), ProjectEdits::tr("Draw Pitch"));

        // The same values again change nothing.
        QVERIFY(ProjectEdits::drawPitchBend(notes, 1, -20, {1, 2}, diagnostics));
        QCOMPARE(session.currentStep(), 1);
        session.undo();
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    // Drawn values replace those at their places and extend them either way; the gaps take the
    // curve as it was: before the note that of the previous note, after the values the last one
    // for an interval, then 0.
    void mode1_values_are_drawn_over_and_extended() {
        auto project = bentNotes();
        project.tracks[0].notes[1].pitchBend = PitchBend{
            -20, {10, 20, 30}
        };
        ProjectSession session(project);
        const auto notes = notesOf(session);

        DiagnosticList diagnostics;
        // At -32 ticks, the nearest place is -30.
        QVERIFY(ProjectEdits::drawPitchBend(notes, 1, -32, {5}, diagnostics));
        QCOMPARE(bendOf(session, 1), std::optional(PitchBend{
                                         -30, {5, 60, 10, 20, 30}
        }));
        QVERIFY(ProjectEdits::drawPitchBend(notes, 1, 5, {1, 2}, diagnostics));
        QCOMPARE(bendOf(session, 1), std::optional(PitchBend{
                                         -30, {5, 60, 10, 20, 30, 0, 0, 1, 2}
        }));
        QVERIFY(ProjectEdits::drawPitchBend(notes, 1, -20, {99}, diagnostics));
        QCOMPARE(bendOf(session, 1), std::optional(PitchBend{
                                         -30, {5, 60, 99, 20, 30, 0, 0, 1, 2}
        }));
        QCOMPARE(session.currentStep(), 3);
        QCOMPARE(bendOf(session, 0), project.tracks[0].notes[0].pitchBend);

        QVERIFY(!ProjectEdits::drawPitchBend(notes, 2, 0, {1}, diagnostics));
        QCOMPARE(session.currentStep(), 3);
        while (session.canUndo()) {
            session.undo();
        }
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    // A label is set on a note and removed by an empty text. A region names its first note with
    // $region and its last with $region_end, one step each; beyond the notes it is refused.
    void labels_and_regions_are_named() {
        const auto project = richProject();
        ProjectSession session(project);
        const auto notes = notesOf(session);
        DiagnosticList diagnostics;

        QVERIFY(ProjectEdits::setLabel(notes.at(0), QStringLiteral("A"), diagnostics));
        QCOMPARE(session.snapshot().tracks[0].notes[0].label, QStringLiteral("A"));
        QCOMPARE(session.undoMessage(), ProjectEdits::tr("Set Label"));
        QVERIFY(ProjectEdits::setLabel(notes.at(0), QString(), diagnostics));
        QVERIFY(session.snapshot().tracks[0].notes[0].label.isEmpty());

        QVERIFY(ProjectEdits::nameRegion(notes, 0, 2, QStringLiteral("Verse"), diagnostics));
        auto snapshot = session.snapshot().tracks[0].notes;
        QCOMPARE(snapshot[0].regions, (QStringList{"A", "Verse"}));
        QCOMPARE(snapshot[0].regionEnds, project.tracks[0].notes[0].regionEnds);
        QCOMPARE(snapshot[1].regionEnds, QStringList{"Verse"});
        QCOMPARE(snapshot[1].regions, project.tracks[0].notes[1].regions);
        QCOMPARE(session.undoMessage(), ProjectEdits::tr("Name Region"));
        const int step = session.currentStep();

        QVERIFY(ProjectEdits::nameRegion(notes, 0, 2, QString(), diagnostics));
        snapshot = session.snapshot().tracks[0].notes;
        QVERIFY(snapshot[0].regions.isEmpty());
        QVERIFY(snapshot[1].regionEnds.isEmpty());

        QVERIFY(
            !ProjectEdits::nameRegion(notes, 1, notes.size(), QStringLiteral("X"), diagnostics));
        QVERIFY(!ProjectEdits::nameRegion(notes, 0, 0, QStringLiteral("X"), diagnostics));
        QCOMPARE(session.currentStep(), step + 1);
    }

    void regions_sharing_an_endpoint_are_joined_and_pipe_names_are_rejected() {
        Project project;
        project.tracks.push_back({});
        for (int i = 0; i < 3; ++i) {
            Note note;
            note.length = 480;
            note.noteNum = 60;
            project.tracks[0].notes.push_back(note);
        }
        ProjectSession session(project);
        const auto notes = notesOf(session);
        DiagnosticList diagnostics;

        QVERIFY(ProjectEdits::nameRegion(notes, 0, 3, QStringLiteral("A"), diagnostics));
        QVERIFY(ProjectEdits::nameRegion(notes, 1, 2, QStringLiteral("B"), diagnostics));
        auto snapshot = session.snapshot().tracks[0].notes;
        QCOMPARE(snapshot[0].regions, QStringList{"A"});
        QCOMPARE(snapshot[1].regions, QStringList{"B"});
        QCOMPARE(snapshot[2].regionEnds, (QStringList{"A", "B"}));

        QVERIFY(!ProjectEdits::nameRegion(notes, 0, 1, QStringLiteral("A|B"), diagnostics));
        QVERIFY(!diagnostics.isEmpty());
        QCOMPARE(session.snapshot().tracks[0].notes[0].regions, QStringList{"A"});
    }

    // Mode1 values replace those of a note whole and are removed by an empty value; equal
    // values make no step.
    void mode1_values_are_replaced_and_removed() {
        auto project = bentNotes();
        project.tracks[0].notes[1].pitchBend = PitchBend{
            -20, {10, 20, 30}
        };
        ProjectSession session(project);
        const auto notes = notesOf(session);

        DiagnosticList diagnostics;
        const PitchBend bend{
            -40.5, {1, 2}
        };
        QVERIFY(ProjectEdits::setPitchBend(notes.at(1), bend, diagnostics));
        QCOMPARE(bendOf(session, 1), std::optional(bend));
        QCOMPARE(session.undoMessage(), ProjectEdits::tr("Change Pitch"));
        QVERIFY(ProjectEdits::setPitchBend(notes.at(1), bend, diagnostics));
        QCOMPARE(session.currentStep(), 1);
        QVERIFY(ProjectEdits::setPitchBend(notes.at(1), std::nullopt, diagnostics));
        QCOMPARE(bendOf(session, 1), std::nullopt);
        QCOMPARE(session.currentStep(), 2);
        while (session.canUndo()) {
            session.undo();
        }
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    // The properties set change on every note in one step, a cleared one goes back to the
    // default, and the others stay; none that differs makes no step.
    void the_note_properties_change_in_one_step() {
        const auto project = richProject();
        ProjectSession session(project);
        const auto notes = ProjectRef(&session).tracks().at(0).notes();
        const QList<NoteRef> both{notes.at(0), notes.at(1)};

        NotePropertyChanges changes;
        changes.tempo = std::optional(150.0);
        changes.intensity = std::optional<double>();
        changes.flags = QStringLiteral("B0");
        DiagnosticList diagnostics;
        QVERIFY(ProjectEdits::setNoteProperties(both, changes, diagnostics));
        const auto edited = session.snapshot();
        for (int i : {0, 1}) {
            const auto &note = edited.tracks[0].notes[i];
            QCOMPARE(note.tempo, std::optional(150.0));
            QVERIFY(!note.intensity);
            QCOMPARE(note.flags, QStringLiteral("B0"));
            QCOMPARE(note.lyric, project.tracks[0].notes[i].lyric);
        }
        QCOMPARE(session.currentStep(), 1);
        QCOMPARE(session.undoMessage(), ProjectEdits::tr("Change Note Properties"));

        QVERIFY(ProjectEdits::setNoteProperties(both, changes, diagnostics));
        QVERIFY(ProjectEdits::setNoteProperties(both, {}, diagnostics));
        QVERIFY(ProjectEdits::setNoteProperties({}, changes, diagnostics));
        QCOMPARE(session.currentStep(), 1);

        // Clearing the tempo, and a length that is not positive, refused with nothing changed
        NotePropertyChanges wrong;
        wrong.tempo = std::optional<double>();
        wrong.length = 0;
        QVERIFY(!ProjectEdits::setNoteProperties(both, wrong, diagnostics));
        QVERIFY(hasError(diagnostics));
        QCOMPARE(session.snapshot().tracks[0].notes[0].tempo, std::optional(150.0));

        NotePropertyChanges clear;
        clear.tempo = std::optional<double>();
        QVERIFY(ProjectEdits::setNoteProperties({notes.at(1)}, clear, diagnostics));
        QVERIFY(!session.snapshot().tracks[0].notes[1].tempo);

        session.undo();
        session.undo();
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    // Merged notes become the first of them, as long as all of them; not fewer than two, nor
    // past a note that sets a tempo.
    void notes_are_merged_into_the_first() {
        Project project;
        Track track;
        for (const auto lyric : {"a", "ka", "sa", "ta"}) {
            Note note;
            note.lyric = QString::fromLatin1(lyric);
            note.length = 240;
            note.noteNum = 60;
            track.notes.push_back(note);
        }
        track.notes[1].length = 480;
        track.notes[3].tempo = 90;
        project.tracks.push_back(track);
        ProjectSession session(project);
        const auto notes = ProjectRef(&session).tracks().at(0).notes();

        DiagnosticList diagnostics;
        QVERIFY(ProjectEdits::mergeNotes(notes, 0, 3, diagnostics));
        auto merged = session.snapshot().tracks[0].notes;
        QCOMPARE(merged.size(), 2);
        QCOMPARE(merged[0].lyric, QStringLiteral("a"));
        QCOMPARE(merged[0].length, 960);
        QCOMPARE(merged[1].lyric, QStringLiteral("ta"));
        QCOMPARE(session.undoMessage(), ProjectEdits::tr("Merge Notes"));

        QVERIFY(!ProjectEdits::mergeNotes(notes, 0, 2, diagnostics));
        QVERIFY(hasError(diagnostics));
        diagnostics.clear();
        QVERIFY(!ProjectEdits::mergeNotes(notes, 0, 1, diagnostics));
        QVERIFY(!ProjectEdits::mergeNotes(notes, 1, 2, diagnostics));
        QCOMPARE(session.currentStep(), 1);

        session.undo();
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    // The properties set change in one step, the others stay; none that differs makes no step.
    void the_properties_change_in_one_step() {
        const auto project = richProject();
        ProjectSession session(project);
        const auto ref = ProjectRef(&session);

        ProjectPropertyChanges changes;
        changes.name = QStringLiteral("song");
        changes.tempo = 90;
        changes.flags = QStringLiteral("g-5");
        changes.voiceDir = QStringLiteral("%VOICE%other");
        changes.resampler = QStringLiteral("C:/tools/moresampler.exe");
        DiagnosticList diagnostics;
        QVERIFY(ProjectEdits::setProperties(ref, changes, diagnostics));
        const auto edited = session.snapshot();
        QCOMPARE(edited.settings.name, QStringLiteral("song"));
        QCOMPARE(edited.settings.tempo, 90.0);
        QCOMPARE(edited.settings.flags, QStringLiteral("g-5"));
        QCOMPARE(edited.tracks[0].voiceDir, QStringLiteral("%VOICE%other"));
        QCOMPARE(edited.settings.resampler, QStringLiteral("C:/tools/moresampler.exe"));
        QCOMPARE(edited.settings.outputFile, project.settings.outputFile);
        QCOMPARE(edited.settings.wavtool, project.settings.wavtool);
        QCOMPARE(edited.settings.mode2, project.settings.mode2);
        QCOMPARE(session.currentStep(), 1);
        QCOMPARE(session.undoMessage(), ProjectEdits::tr("Change Project Properties"));

        // The same again, or nothing, makes no step.
        QVERIFY(ProjectEdits::setProperties(ref, changes, diagnostics));
        QVERIFY(ProjectEdits::setProperties(ref, {}, diagnostics));
        QCOMPARE(session.currentStep(), 1);

        // A tempo that is not positive is refused, and changes nothing.
        ProjectPropertyChanges wrong;
        wrong.name = QStringLiteral("other");
        wrong.tempo = 0;
        QVERIFY(!ProjectEdits::setProperties(ref, wrong, diagnostics));
        QVERIFY(hasError(diagnostics));
        QCOMPARE(session.snapshot().settings.name, QStringLiteral("song"));

        session.undo();
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    // Mode2 is turned on and off as an undo step, which keeps the data of either mode.
    void mode2_is_turned_on_and_off() {
        const auto project = richProject();
        ProjectSession session(project);
        const auto settings = ProjectRef(&session).settings();
        const bool mode2 = settings.mode2();

        DiagnosticList diagnostics;
        QVERIFY(ProjectEdits::setMode2(settings, !mode2, diagnostics));
        QCOMPARE(settings.mode2(), !mode2);
        QCOMPARE(session.undoMessage(),
                 mode2 ? ProjectEdits::tr("Turn Mode2 Off") : ProjectEdits::tr("Turn Mode2 On"));
        QVERIFY(ProjectEdits::setMode2(settings, !mode2, diagnostics));
        QCOMPARE(session.currentStep(), 1);
        session.undo();
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }
};

QTEST_APPLESS_MAIN(test_ProjectEdits)

#include "test_ProjectEdits.moc"
