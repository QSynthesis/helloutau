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
        QVERIFY(ProjectEdits::insertNote(notes, 1, added, diagnostics));
        QVERIFY(ProjectEdits::insertNote(notes, 3, added, diagnostics));
        QVERIFY(ProjectEdits::insertNote(notes, 0, added, diagnostics));

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
        QVERIFY(!ProjectEdits::insertNote(notes, 3, added, diagnostics));
        QVERIFY(!ProjectEdits::insertNote(notes, -1, added, diagnostics));
        QCOMPARE(diagnostics.size(), 2);

        // A note that violates a constraint is refused as well.
        added.length = 0;
        diagnostics.clear();
        QVERIFY(!ProjectEdits::insertNote(notes, 0, added, diagnostics));
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
};

QTEST_APPLESS_MAIN(test_ProjectEdits)

#include "test_ProjectEdits.moc"
