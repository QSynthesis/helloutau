#include <QtCore/QJsonValue>
#include <QtCore/QRandomGenerator>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <hellokit/Edit/EditSession.h>
#include <hellokit/Edit/ProjectRefs.h>

#include "ProjectSamples.h"

using namespace hello::kit;

class test_EditSession : public QObject {
    Q_OBJECT

private:
    // Applies one random transaction of one to three modifications through the handles.
    static void editAtRandom(EditSession &session, QRandomGenerator &random, int step) {
        const auto notes = ProjectRef(&session).track(0).notes();
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
        const EditSession session(project);
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    void a_random_project_survives_the_round_trip_through_the_tree() {
        for (quint32 seed = 1; seed <= 20; ++seed) {
            const auto project = randomProject(seed);
            const EditSession session(project);
            QVERIFY2(session.snapshot().toJson() == project.toJson(),
                     qPrintable(QStringLiteral("seed %1").arg(seed)));
        }
    }

    void an_empty_track_survives_the_round_trip_through_the_tree() {
        Project project;
        project.tracks.push_back(Track());
        const EditSession session(project);
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    // The functions by identifier and slot, on which the handles and the commands are built.
    void nodes_are_addressed_by_identifier_and_slot() {
        EditSession session(richProject());
        const auto unknownFields = session.child(session.root(), ProjectSlots::UnknownFields);
        QCOMPARE(session.size(unknownFields), 6);
        QCOMPARE(session.entry(unknownFields, QStringLiteral("number")).value<QJsonValue>(),
                 QJsonValue(2.5));

        const auto tracks = session.child(session.root(), ProjectSlots::Tracks);
        const auto notes = session.child(session.at(tracks, 0), TrackSlots::Notes);
        const auto note = session.at(notes, 0);
        QCOMPARE(session.value(note, NoteSlots::Lyric), QString::fromUtf8("あ"));
        QCOMPARE(session.value(note, NoteSlots::Lyric.index).toString(), QString::fromUtf8("あ"));

        const auto values =
            session.child(session.child(note, NoteSlots::PitchBend), PitchBendSlots::Values);
        QCOMPARE(session.size(values), 4);

        // A node of another kind reads as default values.
        QVERIFY(!session.value(notes, 0).isValid());
        QVERIFY(session.keys(note).isEmpty());
        QCOMPARE(session.note(notes).lyric, QString());
    }

    void a_transaction_without_commit_is_rolled_back() {
        const auto project = richProject();
        EditSession session(project);
        const auto notes = ProjectRef(&session).track(0).notes();
        {
            auto transaction = session.transaction(QStringLiteral("Discarded"));
            notes.at(0).setLyric(QStringLiteral("x"));
            notes.remove(1, 1);
            notes.at(0).userData().remove(QStringLiteral("$custom"));
            QVERIFY(session.inTransaction());
        }
        QVERIFY(!session.inTransaction());
        QVERIFY(!session.canUndo());
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    void a_committed_transaction_is_one_undo_step_with_its_message() {
        const auto project = richProject();
        EditSession session(project);
        const auto notes = ProjectRef(&session).track(0).notes();

        auto transaction = session.transaction(QString::fromUtf8("移动 1 个音符"));
        notes.at(0).setLyric(QStringLiteral("x"));
        notes.move(0, 1, 1);
        transaction.commit();
        const auto edited = session.snapshot().toJson();
        QVERIFY(edited != project.toJson());

        QVERIFY(session.canUndo());
        QCOMPARE(session.undoMessage(), QString::fromUtf8("移动 1 个音符"));
        QVERIFY(session.redoMessage().isEmpty());

        session.undo();
        QCOMPARE(session.snapshot().toJson(), project.toJson());
        QVERIFY(!session.canUndo());
        QCOMPARE(session.redoMessage(), QString::fromUtf8("移动 1 个音符"));

        session.redo();
        QCOMPARE(session.snapshot().toJson(), edited);
    }

    // A value equal to the current one creates no change, which relies on the equality of the
    // value types stored in the slots.
    void an_unchanged_value_creates_no_undo_step() {
        const auto project = richProject();
        EditSession session(project);
        const auto note = ProjectRef(&session).track(0).notes().at(0);

        auto transaction = session.transaction(QStringLiteral("Nothing"));
        note.setLyric(note.lyric());
        note.setIntensity(note.intensity());
        note.setEnvelope(note.envelope());
        note.setVibrato(note.vibrato());
        note.userData().setValue(QStringLiteral("$custom"), QStringLiteral("kept"));
        transaction.commit();
        QVERIFY(!session.canUndo());
    }

    void the_signals_report_the_changes_in_the_applied_direction() {
        EditSession session(richProject());
        const auto notes = ProjectRef(&session).track(0).notes();
        const auto note = notes.at(0);

        QSignalSpy valueChanged(&session, &EditSession::valueChanged);
        QSignalSpy entryChanged(&session, &EditSession::entryChanged);
        QSignalSpy arrayChanged(&session, &EditSession::arrayChanged);
        QSignalSpy inserted(&session, &EditSession::itemsInserted);
        QSignalSpy aboutToBeRemoved(&session, &EditSession::itemsAboutToBeRemoved);
        QSignalSpy removed(&session, &EditSession::itemsRemoved);
        QSignalSpy moved(&session, &EditSession::itemsMoved);
        QSignalSpy stepChanged(&session, &EditSession::stepChanged);

        auto transaction = session.transaction(QStringLiteral("Signals"));
        note.setLyric(QStringLiteral("x"));
        note.userData().setValue(QStringLiteral("$new"), QStringLiteral("v"));
        note.pitchBend().removeValues(0, 1);
        notes.insert(2, {Note(), Note()});
        notes.move(0, 1, 3);
        QCOMPARE(stepChanged.count(), 0);
        transaction.commit();

        QCOMPARE(valueChanged.count(), 1);
        QCOMPARE(valueChanged.at(0).at(0).value<NodeId>(), note.id());
        QCOMPARE(valueChanged.at(0).at(1).toInt(), NoteSlots::Lyric.index);
        QCOMPARE(entryChanged.count(), 1);
        QCOMPARE(entryChanged.at(0).at(1).toString(), QStringLiteral("$new"));
        QCOMPARE(arrayChanged.count(), 1);
        QCOMPARE(inserted.count(), 1);
        QCOMPARE(inserted.at(0), QVariantList({QVariant::fromValue(notes.id()), 2, 2}));
        QCOMPARE(moved.count(), 1);
        QCOMPARE(moved.at(0), QVariantList({QVariant::fromValue(notes.id()), 0, 1, 3}));
        QCOMPARE(stepChanged.count(), 1);

        // Undo applies the inverse changes in reverse order: the move back, then the removal of
        // the inserted notes, which is announced while they are still in the list.
        session.undo();
        QCOMPARE(moved.count(), 2);
        QCOMPARE(moved.at(1), QVariantList({QVariant::fromValue(notes.id()), 3, 1, 0}));
        QCOMPARE(aboutToBeRemoved.count(), 1);
        QCOMPARE(aboutToBeRemoved.at(0), QVariantList({QVariant::fromValue(notes.id()), 2, 2}));
        QCOMPARE(removed.count(), 1);
        QCOMPARE(removed.at(0), QVariantList({QVariant::fromValue(notes.id()), 2, 2}));
        QCOMPARE(valueChanged.count(), 2);
        QCOMPARE(entryChanged.count(), 2);
        QCOMPARE(arrayChanged.count(), 2);
        QCOMPARE(stepChanged.count(), 2);
    }

    void a_rollback_reports_the_inverse_changes() {
        EditSession session(richProject());
        const auto notes = ProjectRef(&session).track(0).notes();
        QSignalSpy inserted(&session, &EditSession::itemsInserted);
        QSignalSpy removed(&session, &EditSession::itemsRemoved);
        QSignalSpy stepChanged(&session, &EditSession::stepChanged);
        {
            auto transaction = session.transaction(QStringLiteral("Discarded"));
            notes.insert(0, {Note()});
        }
        QCOMPARE(inserted.count(), 1);
        QCOMPARE(removed.count(), 1);
        QCOMPARE(stepChanged.count(), 0);
    }

    // Acceptance criteria 3 and 5 of docs/Editing.md: undoing every step restores the original
    // project, redoing every step restores the edited one, and every position in between
    // matches the snapshot taken when it was first reached.
    void random_edits_undo_and_redo_to_every_recorded_state() {
        for (quint32 seed = 1; seed <= 10; ++seed) {
            QRandomGenerator random(seed);
            EditSession session(richProject());

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

QTEST_APPLESS_MAIN(test_EditSession)

#include "test_EditSession.moc"
