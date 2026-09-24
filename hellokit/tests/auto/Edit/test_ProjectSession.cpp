#include <QtCore/QRandomGenerator>
#include <QtTest/QTest>

#include <hellokit/Edit/ProjectSession.h>
#include <hellokit/Edit/ProjectRefs.h>

#include "ProjectSamples.h"

using namespace hello::kit;

class test_ProjectSession : public QObject {
    Q_OBJECT

private:
    // Applies one random transaction of one to three modifications through the handles.
    static void editAtRandom(ProjectSession &session, QRandomGenerator &random, int step) {
        const auto notes = ProjectRef(&session).tracks().at(0).notes();
        auto transaction = session.transaction(QStringLiteral("Step %1").arg(step));
        for (int i = 1 + random.bounded(3); i > 0; --i) {
            const int size = notes.size();
            const auto note = size ? notes.at(random.bounded(size)) : NoteRef();
            switch (size ? random.bounded(9) : 0) {
                case 0: {
                    Note added;
                    added.lyric = QStringLiteral("n%1").arg(step);
                    added.length = 480;
                    added.noteNum = 60;
                    added.pitchBend = PitchBend{
                        std::nullopt, {1, 2, 3}
                    };
                    notes.insert(random.bounded(size + 1), {added});
                    break;
                }
                case 1:
                    notes.remove(random.bounded(size), 1);
                    break;
                case 2:
                    if (size > 1) {
                        const int index = random.bounded(size);
                        int destination = random.bounded(size - 1);
                        destination += destination >= index ? 1 : 0;
                        notes.move(index, 1, destination);
                    }
                    break;
                case 3:
                    note.setLyric(QStringLiteral("l%1").arg(step));
                    break;
                case 4:
                    note.setIntensity(random.bounded(2) ? std::optional<double>(step)
                                                        : std::nullopt);
                    break;
                case 5:
                    note.setVibrato(Vibrato{double(step), 180, 35, 20, 20, 0, 0, 0});
                    break;
                case 6:
                    note.userData().setValue(QStringLiteral("$k"), QString::number(step));
                    break;
                case 7:
                    if (const auto pitchBend = note.pitchBend(); pitchBend.isValid()) {
                        pitchBend.replaceValues(0, {double(step)});
                    } else {
                        note.setPitchBend(PitchBend{double(step), {0}});
                    }
                    break;
                case 8:
                    note.portamento().insert(0, {
                                                    {double(step), 0, PortamentoPoint::R}
                    });
                    break;
            }
        }
        transaction.commit();
    }

private Q_SLOTS:
    // Acceptance criterion 1 of docs/Editing.md. The comparison uses the .usth serialization,
    // which covers every field and distinguishes an absent optional field from zero.
    void every_field_survives_the_round_trip_through_the_tree() {
        const auto project = richProject();
        const ProjectSession session(project);
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    void a_random_project_survives_the_round_trip_through_the_tree() {
        for (quint32 seed = 1; seed <= 20; ++seed) {
            const auto project = randomProject(seed);
            const ProjectSession session(project);
            QVERIFY2(session.snapshot().toJson() == project.toJson(),
                     qPrintable(QStringLiteral("seed %1").arg(seed)));
        }
    }

    void an_empty_track_survives_the_round_trip_through_the_tree() {
        Project project;
        project.tracks.push_back(Track());
        const ProjectSession session(project);
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    // A value equal to the current one creates no change, which relies on the equality of the
    // value types stored in the slots.
    void an_unchanged_value_creates_no_undo_step() {
        const auto project = richProject();
        ProjectSession session(project);
        const auto note = ProjectRef(&session).tracks().at(0).notes().at(0);

        auto transaction = session.transaction(QStringLiteral("Nothing"));
        note.setLyric(note.lyric());
        note.setIntensity(note.intensity());
        note.setEnvelope(note.envelope());
        note.setVibrato(note.vibrato());
        note.userData().setValue(QStringLiteral("$custom"), QStringLiteral("kept"));
        transaction.commit();
        QVERIFY(!session.canUndo());
    }

    // Acceptance criteria 3 and 5 of docs/Editing.md: undoing every step restores the original
    // project, redoing every step restores the edited one, and every position in between
    // matches the snapshot taken when it was first reached.
    void random_edits_undo_and_redo_to_every_recorded_state() {
        for (quint32 seed = 1; seed <= 10; ++seed) {
            QRandomGenerator random(seed);
            ProjectSession session(richProject());

            QList<QByteArray> states{session.snapshot().toJson()};
            for (int step = 1; step <= 40; ++step) {
                editAtRandom(session, random, step);
                if (session.canUndo() &&
                    session.undoMessage() == QStringLiteral("Step %1").arg(step)) {
                    states.push_back(session.snapshot().toJson());
                }
            }

            QVERIFY(states.size() > 30);

            for (qsizetype i = states.size() - 1; i > 0; --i) {
                QVERIFY2(session.snapshot().toJson() == states[i],
                         qPrintable(QStringLiteral("seed %1, undo to %2").arg(seed).arg(i)));
                session.undo();
            }
            QVERIFY(!session.canUndo());
            QCOMPARE(session.snapshot().toJson(), states.first());

            for (qsizetype i = 1; i < states.size(); ++i) {
                session.redo();
                QVERIFY2(session.snapshot().toJson() == states[i],
                         qPrintable(QStringLiteral("seed %1, redo to %2").arg(seed).arg(i)));
            }
            QVERIFY(!session.canRedo());
        }
    }
};

QTEST_APPLESS_MAIN(test_ProjectSession)

#include "test_ProjectSession.moc"
