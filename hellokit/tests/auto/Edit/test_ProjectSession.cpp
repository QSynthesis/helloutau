#include <QtCore/QJsonObject>
#include <QtCore/QRandomGenerator>
#include <QtTest/QTest>

#include <hellokit/Edit/ProjectSession.h>
#include <hellokit/Edit/ProjectRefs.h>

#include "ProjectSamples.h"

using namespace hello::kit;

class test_ProjectSession : public QObject {
    Q_OBJECT

private:
    // Returns the log entries of the changes that edit applies, in a transaction if
    // inTransaction is true.
    template <class Edit>
    static QList<QJsonObject> logOf(ProjectSession &session, Edit edit, bool inTransaction = true) {
        QList<QJsonObject> entries;
        const auto connection = QObject::connect(
            &session, &edit::EditSession::changed, &session, [&](const edit::ChangePtr &change) {
                if (const auto entry = session.logEntry(*change)) {
                    entries.push_back(*entry);
                }
            });
        if (inTransaction) {
            auto transaction = session.transaction(QStringLiteral("Edit"));
            edit();
            transaction.commit();
        } else {
            edit();
        }
        QObject::disconnect(connection);
        return entries;
    }

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

    // Each constraint of a project rejects a transaction that violates it, and reports one
    // diagnostic.
    void each_constraint_rejects_a_violating_transaction_data() {
        QTest::addColumn<int>("constraint");
        QTest::newRow("note length") << 0;
        QTest::newRow("note number") << 1;
        QTest::newRow("note tempo") << 2;
        QTest::newRow("project tempo") << 3;
        QTest::newRow("envelope anchor") << 4;
        QTest::newRow("portamento point") << 5;
        QTest::newRow("track count") << 6;
    }

    void each_constraint_rejects_a_violating_transaction() {
        QFETCH(int, constraint);
        const auto project = richProject();
        ProjectSession session(project);
        const auto projectRef = ProjectRef(&session);
        const auto note = projectRef.tracks().at(0).notes().at(0);

        auto transaction = session.transaction(QStringLiteral("Violation"));
        switch (constraint) {
            case 0:
                note.setLength(0);
                break;
            case 1:
                note.setNoteNum(128);
                break;
            case 2:
                note.setTempo(0.0);
                break;
            case 3:
                projectRef.settings().setTempo(-1);
                break;
            case 4: {
                auto envelope = *note.envelope();
                envelope.anchors[1].x = -5;
                note.setEnvelope(envelope);
                break;
            }
            case 5:
                note.portamento().at(2).setX(-1);
                break;
            case 6:
                projectRef.tracks().insert(1, {Track()});
                break;
        }
        DiagnosticList diagnostics;
        QVERIFY(!transaction.commit(diagnostics));
        QCOMPARE(diagnostics.size(), 1);
        QVERIFY(hasError(diagnostics));
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    // The message of a range violation states the value and the range, with integer bounds for
    // an integer slot.
    void a_range_violation_states_the_value_and_the_range_data() {
        QTest::addColumn<int>("constraint");
        QTest::addColumn<QString>("message");
        QTest::newRow("between") << 0
                                 << QStringLiteral("The noteNum 128 is outside the range from 0 "
                                                   "to 127.");
        QTest::newRow("at least") << 1 << QStringLiteral("The length 0 is less than 1.");
        QTest::newRow("greater than") << 2 << QStringLiteral("The tempo 0 is not greater than 0.");
    }

    void a_range_violation_states_the_value_and_the_range() {
        QFETCH(int, constraint);
        QFETCH(QString, message);
        ProjectSession session(richProject());
        const auto note = ProjectRef(&session).tracks().at(0).notes().at(0);
        auto transaction = session.transaction(QStringLiteral("Violation"));
        switch (constraint) {
            case 0:
                note.setNoteNum(128);
                break;
            case 1:
                note.setLength(0);
                break;
            case 2:
                note.setTempo(0.0);
                break;
        }
        DiagnosticList diagnostics;
        QVERIFY(!transaction.commit(diagnostics));
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(diagnostics.first().message, message);
    }

    // The first portamento point is relative to the start of the note and may precede it.
    void the_first_portamento_point_may_precede_the_note() {
        ProjectSession session(richProject());
        const auto note = ProjectRef(&session).tracks().at(0).notes().at(0);
        auto transaction = session.transaction(QStringLiteral("Portamento"));
        note.portamento().at(0).setX(-100);
        QVERIFY(transaction.commit());
    }

    // A project read from a file may contain values that the constraints reject. Editing it
    // succeeds unless the edit introduces a violation.
    void a_violation_read_from_a_file_does_not_prevent_editing() {
        auto project = richProject();
        project.tracks[0].notes[0].noteNum = 200;
        project.tracks[0].notes[0].portamento[1].x = -3;
        ProjectSession session(project);
        const auto note = ProjectRef(&session).tracks().at(0).notes().at(0);

        auto lyric = session.transaction(QStringLiteral("Lyric"));
        note.setLyric(QStringLiteral("i"));
        note.portamento().at(3).setY(1);
        QVERIFY(lyric.commit());

        auto pitch = session.transaction(QStringLiteral("Pitch"));
        note.setNoteNum(300);
        DiagnosticList diagnostics;
        QVERIFY(!pitch.commit(diagnostics));
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(note.noteNum(), 200);
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

    // A value is logged with the name of its field and its values as in .usth. An empty
    // optional value is null, as in a command.
    void a_value_is_logged_with_its_field_name_and_values() {
        ProjectSession session(richProject());
        const auto note = ProjectRef(&session).tracks().at(0).notes().at(0);
        const auto id = qint64(note.id());
        const auto entries = logOf(session, [&] {
            note.setLyric(QStringLiteral("i"));
            note.setIntensity(std::nullopt);
            note.portamento().at(0).setType(PortamentoPoint::J);
        });
        const auto point = qint64(note.portamento().at(0).id());
        QCOMPARE(entries, QList<QJsonObject>({
                              QJsonObject{{QStringLiteral("node"), id},
                                          {QStringLiteral("shape"), QStringLiteral("set")},
                                          {QStringLiteral("slot"), QStringLiteral("lyric")},
                                          {QStringLiteral("before"), QString::fromUtf8("あ")},
                                          {QStringLiteral("after"), QStringLiteral("i")}},
                              QJsonObject{{QStringLiteral("node"), id},
                                          {QStringLiteral("shape"), QStringLiteral("set")},
                                          {QStringLiteral("slot"), QStringLiteral("intensity")},
                                          {QStringLiteral("before"), 0},
                                          {QStringLiteral("after"), QJsonValue::Null}   },
                              QJsonObject{{QStringLiteral("node"), point},
                                          {QStringLiteral("shape"), QStringLiteral("set")},
                                          {QStringLiteral("slot"), QStringLiteral("type")},
                                          {QStringLiteral("before"), QStringLiteral("S")},
                                          {QStringLiteral("after"), QStringLiteral("J")}},
        }));
    }

    // Undo reports the changes in the reverse direction.
    void an_undone_value_is_logged_in_reverse() {
        ProjectSession session(richProject());
        const auto note = ProjectRef(&session).tracks().at(0).notes().at(0);
        logOf(session, [&] { note.setLyric(QStringLiteral("i")); });
        const auto entries = logOf(session, [&] { session.undo(); }, false);
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().value(QStringLiteral("before")), QJsonValue(QStringLiteral("i")));
        QCOMPARE(entries.first().value(QStringLiteral("after")),
                 QJsonValue(QString::fromUtf8("あ")));
    }

    // A whole value is logged as its JSON in .usth.
    void a_whole_value_is_logged_as_its_json() {
        const auto project = richProject();
        ProjectSession session(project);
        const auto note = ProjectRef(&session).tracks().at(0).notes().at(0);
        auto vibrato = *project.tracks.first().notes.first().vibrato;
        vibrato.period = 200;
        const auto entries = logOf(session, [&] { note.setVibrato(vibrato); });
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().value(QStringLiteral("before")),
                 QJsonValue(project.tracks.first().notes.first().vibrato->toJson()));
        QCOMPARE(entries.first().value(QStringLiteral("after")), QJsonValue(vibrato.toJson()));
    }

    // The previous record is no longer in the tree, therefore only the record after the change is
    // logged.
    void a_replaced_record_is_logged_as_the_record_after_the_change() {
        ProjectSession session(richProject());
        const auto note = ProjectRef(&session).tracks().at(0).notes().at(0);
        const PitchBend bend{
            std::nullopt, {1, 2}
        };
        auto entries = logOf(session, [&] { note.setPitchBend(bend); });
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().value(QStringLiteral("slot")),
                 QJsonValue(QStringLiteral("pitchBend")));
        QVERIFY(!entries.first().contains(QStringLiteral("before")));
        QCOMPARE(entries.first().value(QStringLiteral("after")), QJsonValue(bend.toJson()));

        entries = logOf(session, [&] { note.setPitchBend(std::nullopt); });
        QCOMPARE(entries.first().value(QStringLiteral("after")), QJsonValue(QJsonValue::Null));
    }

    // An absent entry is omitted rather than written as null, because null is a value of an
    // unknown field.
    void an_entry_is_logged_with_its_key() {
        ProjectSession session(richProject());
        const auto userData = ProjectRef(&session).tracks().at(0).notes().at(0).userData();
        auto entries = logOf(session, [&] {
            userData.setValue(QStringLiteral("$new"), QStringLiteral("v"));
            userData.remove(QStringLiteral("$custom"));
        });
        const auto id = qint64(userData.id());
        QCOMPARE(entries, QList<QJsonObject>({
                              QJsonObject{{QStringLiteral("node"), id},
                                          {QStringLiteral("shape"), QStringLiteral("entry")},
                                          {QStringLiteral("key"), QStringLiteral("$new")},
                                          {QStringLiteral("after"), QStringLiteral("v")}    },
                              QJsonObject{{QStringLiteral("node"), id},
                                          {QStringLiteral("shape"), QStringLiteral("entry")},
                                          {QStringLiteral("key"), QStringLiteral("$custom")},
                                          {QStringLiteral("before"), QStringLiteral("kept")}},
        }));

        const auto unknownFields = ProjectRef(&session).unknownFields();
        entries = logOf(
            session, [&] { unknownFields.setValue(QStringLiteral("number"), QJsonValue::Null); });
        QCOMPARE(entries.first().value(QStringLiteral("before")), QJsonValue(2.5));
        QCOMPARE(entries.first().value(QStringLiteral("after")), QJsonValue(QJsonValue::Null));
    }

    // A removal is logged once, after it is applied.
    void list_and_array_changes_are_logged_by_position() {
        ProjectSession session(richProject());
        const auto notes = ProjectRef(&session).tracks().at(0).notes();
        const auto id = qint64(notes.id());
        const auto bend = notes.at(0).pitchBend();
        auto entries = logOf(session, [&] {
            notes.move(0, 1, 1);
            notes.remove(0, 1);
            notes.insert(1, {session.snapshot().tracks.first().notes.first()});
            bend.replaceValues(0, {5});
        });
        // The array node has no handle, therefore its entry is compared without the node.
        QCOMPARE(entries.size(), 4);
        QCOMPARE(entries.takeLast().value(QStringLiteral("shape")),
                 QJsonValue(QStringLiteral("array")));
        QCOMPARE(entries, QList<QJsonObject>({
                              QJsonObject{{QStringLiteral("node"), id},
                                          {QStringLiteral("shape"), QStringLiteral("move")},
                                          {QStringLiteral("index"), 0},
                                          {QStringLiteral("count"), 1},
                                          {QStringLiteral("destination"), 1}},
                              QJsonObject{{QStringLiteral("node"), id},
                                          {QStringLiteral("shape"), QStringLiteral("remove")},
                                          {QStringLiteral("index"), 0},
                                          {QStringLiteral("count"), 1}},
                              QJsonObject{{QStringLiteral("node"), id},
                                          {QStringLiteral("shape"), QStringLiteral("insert")},
                                          {QStringLiteral("index"), 1},
                                          {QStringLiteral("count"), 1}},
        }));
    }
};

QTEST_APPLESS_MAIN(test_ProjectSession)

#include "test_ProjectSession.moc"
