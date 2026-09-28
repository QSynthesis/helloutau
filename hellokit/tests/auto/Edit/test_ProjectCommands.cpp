#include <QtCore/QDebug>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtTest/QTest>

#include <hellokit/Edit/ProjectCommands.h>
#include <hellokit/Edit/ProjectEdits.h>
#include <hellokit/Edit/ProjectRefs.h>

#include "ProjectSamples.h"

using namespace hello::kit;

// The command lines are ordinary string literals rather than raw string literals, because moc
// does not recognize the class of a file whose raw strings contain unpaired quotes.
class test_ProjectCommands : public QObject {
    Q_OBJECT

private:
    static bool run(ProjectSession &session, const QString &line) {
        DiagnosticList diagnostics;
        const auto executed = ProjectCommands::execute(session, line, diagnostics);
        if (!executed) {
            qDebug().noquote() << line
                               << (diagnostics.isEmpty() ? QString() : diagnostics.first().message);
        }
        return executed;
    }

    // Verifies that line is refused with an error and leaves the project and the history
    // unchanged. The first message must contain reason, which distinguishes checks that refuse
    // the same command with different messages.
    static void verifyRefused(ProjectSession &session, const QString &line,
                              const QString &reason = QString()) {
        const auto before = session.snapshot().toJson();
        const auto step = session.currentStep();
        DiagnosticList diagnostics;
        QVERIFY2(!ProjectCommands::execute(session, line, diagnostics), qPrintable(line));
        QVERIFY2(hasError(diagnostics), qPrintable(line));
        QVERIFY2(diagnostics.first().message.contains(reason),
                 qPrintable(diagnostics.first().message));
        QCOMPARE(session.snapshot().toJson(), before);
        QCOMPARE(session.currentStep(), step);
    }

    // The names of the invokable functions declared in the class of meta, sorted.
    static QStringList invokableNames(const QMetaObject &meta) {
        QStringList names;
        for (int i = meta.methodOffset(); i < meta.methodCount(); ++i) {
            names.push_back(QString::fromLatin1(meta.method(i).name()));
        }
        names.sort();
        return names;
    }

    static Note noteAt(const ProjectSession &session, int index) {
        return session.snapshot().tracks.first().notes.at(index);
    }

private Q_SLOTS:
    // Each command is one undo step with the command as its message.
    void a_command_is_one_undo_step() {
        ProjectSession session(richProject());
        const auto line = QStringLiteral("set /tracks/0/notes/0/lyric i");
        QVERIFY(run(session, line));
        QCOMPARE(noteAt(session, 0).lyric, QStringLiteral("i"));
        QCOMPARE(session.currentStep(), 1);
        QCOMPARE(session.undoMessage(), line);

        session.undo();
        QCOMPARE(noteAt(session, 0).lyric, QString::fromUtf8("あ"));
    }

    void a_line_without_a_command_creates_no_step() {
        ProjectSession session(richProject());
        DiagnosticList diagnostics;
        QVERIFY(ProjectCommands::execute(session, QStringLiteral("  # comment"), diagnostics));
        QVERIFY(ProjectCommands::execute(session, QString(), diagnostics));
        QVERIFY(diagnostics.isEmpty());
        QCOMPARE(session.currentStep(), 0);
    }

    void a_malformed_or_unknown_command_is_refused() {
        ProjectSession session(richProject());
        verifyRefused(session, QStringLiteral("set /tracks/0/notes/0/lyric \"open"));
        verifyRefused(session, QStringLiteral("rename /tracks/0/notes/0/lyric a"));
        verifyRefused(session, QStringLiteral("\"set\" /tracks/0/notes/0/lyric a"));
        verifyRefused(session, QStringLiteral("note"));
        verifyRefused(session, QStringLiteral("note merge /tracks/0/notes 0"));
    }

    // An empty optional field is written as null, which differs from zero.
    void null_empties_an_optional_field() {
        ProjectSession session(richProject());
        QVERIFY(run(session, QStringLiteral("set /tracks/0/notes/0/intensity null")));
        QVERIFY(!noteAt(session, 0).intensity);
        QVERIFY(run(session, QStringLiteral("set /tracks/0/notes/0/intensity 0")));
        QCOMPARE(noteAt(session, 0).intensity, std::optional<double>(0));
        verifyRefused(session, QStringLiteral("set /tracks/0/notes/0/length null"));
        verifyRefused(session, QStringLiteral("set /tracks/0/notes/0/length 1.5"));
    }

    // Setting a member replaces the whole value with the member changed.
    void a_member_of_a_whole_value_is_set_by_path() {
        ProjectSession session(richProject());
        auto expected = *noteAt(session, 0).vibrato;
        expected.period = 200;
        QVERIFY(run(session, QStringLiteral("set /tracks/0/notes/0/vibrato/period 200")));
        QVERIFY(noteAt(session, 0).vibrato == expected);

        QVERIFY(run(session, QStringLiteral("set /tracks/0/notes/0/envelope/anchors/1/y 50")));
        QCOMPARE(noteAt(session, 0).envelope->anchors[1].y, 50.0);

        verifyRefused(session, QStringLiteral("set /tracks/0/notes/0/vibrato/speed 1"),
                      QStringLiteral("has no member speed"));
        verifyRefused(session, QStringLiteral("set /tracks/0/notes/0/vibrato/period fast"),
                      QStringLiteral("must be a number"));
        verifyRefused(session, QStringLiteral("set /tracks/0/notes/0/envelope/anchors/4/y 1"),
                      QStringLiteral("has no member anchors/4."));
        // The second note has no vibrato, therefore it is set as a whole.
        verifyRefused(session, QStringLiteral("set /tracks/0/notes/1/vibrato/period 200"));
    }

    // A whole value is read as from a file, which ignores unknown members and reads a member of
    // another type as zero. A command refuses both.
    void a_whole_value_is_read_strictly() {
        ProjectSession session(richProject());
        QVERIFY(run(session, QStringLiteral("set /tracks/0/notes/1/vibrato {\"period\": 180}")));
        QCOMPARE(noteAt(session, 1).vibrato->period, 180.0);
        QCOMPARE(noteAt(session, 1).vibrato->length, 0.0);

        verifyRefused(session, QStringLiteral("set /tracks/0/notes/1/vibrato {\"period\": \"a\"}"));
        verifyRefused(session, QStringLiteral("set /tracks/0/notes/1/vibrato {\"periods\": 1}"));
        verifyRefused(session, QStringLiteral("set /tracks/0/notes/1/envelope {\"anchors\": []}"));
    }

    void an_optional_record_is_set_as_a_whole_or_removed() {
        ProjectSession session(richProject());
        QVERIFY(run(session, QStringLiteral("set /tracks/0/notes/0/pitchBend null")));
        QVERIFY(!noteAt(session, 0).pitchBend);
        QVERIFY(run(session,
                    QStringLiteral("set /tracks/0/notes/0/pitchBend {\"start\": 1, \"values\": "
                                   "[1, 2]}")));
        PitchBend expected;
        expected.start = 1;
        expected.values = {1, 2};
        QVERIFY(noteAt(session, 0).pitchBend == expected);
        QCOMPARE(session.currentStep(), 2);

        // An equal value creates no change.
        QVERIFY(run(session,
                    QStringLiteral("set /tracks/0/notes/0/pitchBend {\"start\": 1, \"values\": "
                                   "[1, 2]}")));
        QCOMPARE(session.currentStep(), 2);

        // A value without the start of the current value differs from it.
        QVERIFY(
            run(session, QStringLiteral("set /tracks/0/notes/0/pitchBend {\"values\": [1, 2]}")));
        QVERIFY(!noteAt(session, 0).pitchBend->start);
        QCOMPARE(session.currentStep(), 3);

        verifyRefused(session,
                      QStringLiteral("set /tracks/0/notes/0/pitchBend {\"values\": [\"a\"]}"));
        verifyRefused(session, QStringLiteral("set /tracks/0/notes/0/pitchBend 1"));
        verifyRefused(session, QStringLiteral("set /settings {\"tempo\": 120}"),
                      QStringLiteral("is not set as a whole"));
        verifyRefused(session, QStringLiteral("set /tracks/0/notes/0/pitchBend/values/0 5"));
    }

    void the_values_of_an_array_are_edited_by_index() {
        ProjectSession session(richProject());
        QVERIFY(run(session, QStringLiteral("replace /tracks/0/notes/0/pitchBend/values 1 7")));
        QCOMPARE(noteAt(session, 0).pitchBend->values, QList<double>({0, 7, -20, 0}));
    }

    void a_portamento_point_is_inserted_with_the_name_of_its_type() {
        ProjectSession session(richProject());
        QVERIFY(run(session, QStringLiteral("insert /tracks/0/notes/0/portamento 4 "
                                            "{\"x\": 50, \"y\": 1, \"type\": \"R\"}")));
        const auto point = noteAt(session, 0).portamento.last();
        QCOMPARE(point.x, 50.0);
        QCOMPARE(point.type, PortamentoPoint::R);

        QVERIFY(run(session, QStringLiteral("set /tracks/0/notes/0/portamento/0/type J")));
        QCOMPARE(noteAt(session, 0).portamento.first().type, PortamentoPoint::J);

        verifyRefused(session, QStringLiteral("insert /tracks/0/notes/0/portamento 0 "
                                              "{\"x\": -50, \"y\": 0, \"type\": \"s\"}"));
    }

    void the_entries_of_the_mappings_are_set_by_key() {
        ProjectSession session(richProject());
        QVERIFY(run(session, QStringLiteral("set /tracks/0/notes/0/userData $Custom @\"a\\b\"")));
        QCOMPARE(noteAt(session, 0).userData.value(QStringLiteral("$Custom")),
                 QStringLiteral("a\\b"));
        QVERIFY(run(session, QStringLiteral("set /unknownFields extra {\"a\": [1]}")));
        QCOMPARE(session.snapshot().unknownFields.value(QStringLiteral("extra")),
                 QJsonValue(QJsonObject{
                     {QStringLiteral("a"), QJsonArray{1}}
        }));
        verifyRefused(session, QStringLiteral("set /tracks/0/notes/0/userData $Custom 1"));
    }

    // The commit validates the result, therefore a command that introduces a violation is
    // rolled back.
    void a_command_that_violates_a_constraint_is_rolled_back() {
        ProjectSession session(richProject());
        verifyRefused(session, QStringLiteral("set /tracks/0/notes/0/length 0"));
        verifyRefused(session, QStringLiteral("set /settings/tempo -1"));
        verifyRefused(session, QStringLiteral("remove /tracks 0"));
    }

    void note_transpose_moves_the_given_notes() {
        ProjectSession session(richProject());
        QVERIFY(
            run(session, QStringLiteral("note transpose -2 /tracks/0/notes/0 /tracks/0/notes/1")));
        QCOMPARE(noteAt(session, 0).noteNum, 58);
        QCOMPARE(noteAt(session, 1).noteNum, 58);
        QCOMPARE(session.currentStep(), 1);

        verifyRefused(session, QStringLiteral("note transpose 1.5 /tracks/0/notes/0"));
        verifyRefused(session, QStringLiteral("note transpose 1 /tracks/0/notes"));
        verifyRefused(session, QStringLiteral("note transpose 1"));
    }

    void note_split_divides_a_note() {
        ProjectSession session(richProject());
        QVERIFY(run(session, QStringLiteral("note split /tracks/0/notes 0 100")));
        QCOMPARE(noteAt(session, 0).length, 100);
        QCOMPARE(noteAt(session, 1).length, 380);
        QCOMPARE(session.undoMessage(), QStringLiteral("note split /tracks/0/notes 0 100"));

        verifyRefused(session, QStringLiteral("note split /tracks/0/notes 3 10"));
        verifyRefused(session, QStringLiteral("note split /tracks/0/notes -1 10"));
        verifyRefused(session, QStringLiteral("note split /tracks/0/notes 0 100"));
        verifyRefused(session, QStringLiteral("note split /tracks/0/notes/0 0 10"));
        verifyRefused(session, QStringLiteral("note split /tracks/0/notes/0/portamento 0 10"));
        verifyRefused(session, QStringLiteral("note split /tracks/0/notes/0/lyric 0 10"));
    }

    void note_insert_adds_a_note_from_its_json() {
        ProjectSession session(richProject());
        QVERIFY(
            run(session, QStringLiteral("note insert /tracks/0/notes 1 {\"lyric\": \"ka\", "
                                        "\"length\": 240, \"noteNum\": 62, \"flags\": \"g-2\"}")));
        const auto note = noteAt(session, 1);
        QCOMPARE(note.lyric, QStringLiteral("ka"));
        QCOMPARE(note.noteNum, 62);
        QCOMPARE(note.flags, QStringLiteral("g-2"));

        // An absent optional record may be written as null, as in a .usth file.
        QVERIFY(run(session, QStringLiteral("note insert /tracks/0/notes 0 {\"lyric\": \"ka\", "
                                            "\"length\": 240, \"noteNum\": 62, \"pitchBend\": "
                                            "null}")));
        QVERIFY(!noteAt(session, 0).pitchBend);

        verifyRefused(session, QStringLiteral("note insert /tracks/0/notes 0 {\"lyric\": \"ka\"}"));
        verifyRefused(session, QStringLiteral("note insert /tracks/0/notes 0 {\"lyric\": \"ka\", "
                                              "\"length\": 240, \"noteNum\": 62, \"flag\": \"\"}"));
        verifyRefused(session, QStringLiteral("note insert /tracks/0/notes 9 {\"lyric\": \"ka\", "
                                              "\"length\": 240, \"noteNum\": 62}"));
        verifyRefused(session, QStringLiteral("note insert /tracks/0/notes 0 ka"));
    }

    void note_tempo_sets_the_tempo_of_a_note() {
        ProjectSession session(richProject());
        QVERIFY(run(session, QStringLiteral("note tempo /tracks/0/notes/1 150.5")));
        QCOMPARE(noteAt(session, 1).tempo, std::optional<double>(150.5));
        verifyRefused(session, QStringLiteral("note tempo /tracks/0/notes 150"));
        verifyRefused(session, QStringLiteral("note tempo /tracks/0 150"));
        verifyRefused(session, QStringLiteral("note tempo /tracks/0/notes/1 fast"));
    }

    void note_remove_deletes_the_given_notes() {
        ProjectSession session(richProject());
        QVERIFY(run(session, QStringLiteral("note remove /tracks/0/notes 0")));
        QCOMPARE(ProjectRef(&session).tracks().at(0).notes().size(), 1);
        QCOMPARE(noteAt(session, 0).lyric, QStringLiteral("R"));

        verifyRefused(session, QStringLiteral("note remove /tracks/0/notes 1"));
        verifyRefused(session, QStringLiteral("note remove /tracks/0/notes x"));
        verifyRefused(session, QStringLiteral("note remove /tracks/0/notes"));
    }

    void note_length_sets_the_length_of_a_note() {
        ProjectSession session(richProject());
        QVERIFY(run(session, QStringLiteral("note length /tracks/0/notes/0 120")));
        QCOMPARE(noteAt(session, 0).length, 120);
        verifyRefused(session, QStringLiteral("note length /tracks/0/notes/0 0"));
        verifyRefused(session, QStringLiteral("note length /tracks/0/notes 120"));
    }

    void note_move_reorders_the_notes() {
        ProjectSession session(richProject());
        QVERIFY(run(session, QStringLiteral("note insert /tracks/0/notes 2 {\"lyric\": \"ka\", "
                                            "\"length\": 240, \"noteNum\": 62}")));
        QVERIFY(run(session, QStringLiteral("note move /tracks/0/notes 0 1 2")));
        QCOMPARE(noteAt(session, 0).lyric, QStringLiteral("R"));
        QCOMPARE(noteAt(session, 1).lyric, QStringLiteral("ka"));
        QCOMPARE(noteAt(session, 2).lyric, QString::fromUtf8("あ"));
        QVERIFY(run(session, QStringLiteral("note move /tracks/0/notes 0 1 1")));
        QCOMPARE(noteAt(session, 0).lyric, QStringLiteral("ka"));
        verifyRefused(session, QStringLiteral("note move /tracks/0/notes 0 2 2"));
        verifyRefused(session, QStringLiteral("note move /tracks/0/notes 0 1"));
    }

    void note_portamento_replaces_the_points() {
        ProjectSession session(richProject());
        QVERIFY(run(session, QStringLiteral("note portamento /tracks/0/notes/1 "
                                            "[{\"x\": -20, \"y\": 0}, "
                                            "{\"x\": 20, \"y\": -50, \"type\": \"R\"}]")));
        const auto points = noteAt(session, 1).portamento;
        QCOMPARE(points.size(), 2);
        QCOMPARE(points[1].x, 20.0);
        QCOMPARE(points[1].y, -50.0);
        QCOMPARE(points[1].type, PortamentoPoint::R);
        verifyRefused(session, QStringLiteral("note portamento /tracks/0/notes/1 {}"));
        verifyRefused(session, QStringLiteral("note portamento /tracks/0/notes/1 [1]"));
        verifyRefused(session, QStringLiteral("note portamento /tracks/0/notes/1 "
                                              "[{\"x\": 0}, {\"x\": -10}]"));
    }

    void note_envelope_sets_and_removes_the_envelope() {
        ProjectSession session(richProject());
        QVERIFY(run(session, QStringLiteral("note envelope {\"anchors\": [{\"x\": 0, \"y\": 0}, "
                                            "{\"x\": 5, \"y\": 100}, {\"x\": 35, \"y\": 100}, "
                                            "{\"x\": 0, \"y\": 0}]} /tracks/0/notes/1")));
        QCOMPARE(noteAt(session, 1).envelope->anchors[3].x, 35.0);
        QVERIFY(run(session, QStringLiteral("note envelope null /tracks/0/notes/1")));
        QVERIFY(!noteAt(session, 1).envelope);
        verifyRefused(session, QStringLiteral("note envelope {\"anchors\": []} /tracks/0/notes/1"));
        verifyRefused(session, QStringLiteral("note envelope 3 /tracks/0/notes/1"));
    }

    void note_vibrato_sets_and_removes_the_vibrato() {
        ProjectSession session(richProject());
        QVERIFY(run(session, QStringLiteral("note vibrato {\"length\": 65, \"period\": 180} "
                                            "/tracks/0/notes/0 /tracks/0/notes/1")));
        QCOMPARE(noteAt(session, 0).vibrato->period, 180.0);
        QCOMPARE(noteAt(session, 1).vibrato->length, 65.0);
        QVERIFY(run(session, QStringLiteral("note vibrato null /tracks/0/notes/1")));
        QVERIFY(!noteAt(session, 1).vibrato);
        verifyRefused(session, QStringLiteral("note vibrato 3 /tracks/0/notes/1"));
        verifyRefused(session, QStringLiteral("note vibrato null"));
    }

    // Acceptance criteria 3 and 4 of docs/Editing.md: undoing every command restores the
    // project, redoing every command restores the edited project, and the same commands produce
    // the same changes in another session.
    void a_script_is_undone_redone_and_repeated_exactly() {
        const QStringList script{
            QStringLiteral("set /tracks/0/notes/0/lyric ka"),
            QStringLiteral("set /tracks/0/notes/0/flags @\"g-3\\B40\""),
            QStringLiteral("note transpose 2 /tracks/0/notes/0 /tracks/0/notes/1"),
            QStringLiteral("note split /tracks/0/notes 0 120"),
            QStringLiteral("set /tracks/0/notes/1/vibrato {\"period\": 180, \"length\": 65}"),
            QStringLiteral("set /tracks/0/notes/0/vibrato/period 200"),
            QStringLiteral("set /tracks/0/notes/0/pitchBend null"),
            QStringLiteral("note insert /tracks/0/notes 2 {\"lyric\": \"sa\", \"length\": 240, "
                           "\"noteNum\": 64}"),
            QStringLiteral("note tempo /tracks/0/notes/2 140"),
            QStringLiteral("insert /tracks/0/notes/0/portamento 4 {\"x\": 50, \"y\": 1, \"type\": "
                           "\"R\"}"),
            QStringLiteral("set /tracks/0/notes/0/userData $Custom \"x y\""),
            QStringLiteral("remove /tracks/0/notes/0/userData $custom"),
            QStringLiteral("set /unknownFields extra [1, 2]"),
            QStringLiteral("move /tracks/0/notes 0 1 2"),
            QStringLiteral("remove /tracks/0/notes 3"),
        };
        const auto run = [&script](ProjectSession &session) {
            QList<QJsonObject> entries;
            const auto connection =
                QObject::connect(&session, &edit::EditSession::changed, &session,
                                 [&](const edit::ChangePtr &change) {
                                     if (const auto entry = session.logEntry(*change)) {
                                         entries.push_back(*entry);
                                     }
                                 });
            for (const auto &line : script) {
                DiagnosticList diagnostics;
                if (!ProjectCommands::execute(session, line, diagnostics)) {
                    qDebug().noquote() << line << diagnostics.first().message;
                    entries.clear();
                    break;
                }
            }
            QObject::disconnect(connection);
            return entries;
        };

        const auto project = richProject();
        ProjectSession first(project);
        const auto entries = run(first);
        QCOMPARE(first.currentStep(), int(script.size()));
        const auto edited = first.snapshot().toJson();
        QVERIFY(edited != project.toJson());

        ProjectSession second(project);
        QCOMPARE(run(second), entries);

        while (first.canUndo()) {
            first.undo();
        }
        QCOMPARE(first.snapshot().toJson(), project.toJson());
        while (first.canRedo()) {
            first.redo();
        }
        QCOMPARE(first.snapshot().toJson(), edited);
    }

    // Acceptance criterion 7 of docs/Editing.md: every domain function has a command, and every
    // domain command calls a domain function. The functions are those that the meta-object of
    // ProjectEdits lists.
    void every_domain_function_has_a_command() {
        const auto functions = ProjectCommands::domainFunctions();
        QCOMPARE(invokableNames(ProjectEdits::staticMetaObject), functions.keys());
        const auto names = ProjectCommands::names();
        for (const auto &command : functions) {
            QVERIFY2(names.contains(command), qPrintable(command));
        }
        for (const auto &name : names) {
            QVERIFY2(!name.contains(QLatin1Char(' ')) || functions.values().contains(name),
                     qPrintable(name));
        }
    }

    // A query returns the content at a path in the notation of the values of the commands, so
    // that its result can be given to a command again.
    void get_returns_the_content_at_a_path() {
        ProjectSession session(richProject());
        const auto get = [&session](const char *line) {
            DiagnosticList diagnostics;
            const auto result =
                ProjectCommands::query(session, QString::fromUtf8(line), diagnostics);
            if (!result) {
                qDebug().noquote() << line << diagnostics.first().message;
            }
            return result.value_or(QJsonValue(QJsonValue::Undefined));
        };
        const auto note = noteAt(session, 0);
        QCOMPARE(get("get /tracks/0/notes/0/lyric"), QJsonValue(QString::fromUtf8("あ")));
        QCOMPARE(get("get /tracks/0/notes/0/vibrato/period"), QJsonValue(note.vibrato->period));
        QCOMPARE(get("get /tracks/0/notes/0/envelope/anchors/1"),
                 get("get /tracks/0/notes/0/envelope").toObject().value("anchors").toArray().at(1));
        QCOMPARE(get("get /tracks/0/notes/0"), QJsonValue(note.toJson()));
        QCOMPARE(get("get /tracks/0/notes").toArray().size(), 2);
        QCOMPARE(get("get /tracks/0/notes").toArray().at(1),
                 QJsonValue(noteAt(session, 1).toJson()));
        QCOMPARE(get("get /tracks/0/notes/1/intensity"), QJsonValue(QJsonValue::Null));
        QCOMPARE(get("get /tracks/0/notes/0/pitchBend/values"),
                 QJsonValue(QJsonArray{0, 10.5, -20, 0}));
        QCOMPARE(get("get /tracks/0/notes/0/userData"),
                 QJsonValue(QJsonObject{
                     {QStringLiteral("$custom"), QStringLiteral("kept")},
                     {QStringLiteral("Unknown"), QString()             }
        }));
        QCOMPARE(get("get /unknownFields").toObject().value("object"),
                 QJsonValue(QJsonObject{
                     {QStringLiteral("a"), 1}
        }));

        // A record that its document does not write as JSON is an object of its fields.
        const auto settings = get("get /settings").toObject();
        QCOMPARE(settings.value("tempo"), QJsonValue(134.5));
        QCOMPARE(settings.value("mode2"), QJsonValue(false));
        QCOMPARE(get("get /").toObject().value("settings"), QJsonValue(settings));
        QCOMPARE(get("get /").toObject().value("tracks").toArray().at(0).toObject().value("name"),
                 QJsonValue(QStringLiteral("vocal")));

        QVERIFY(run(session, QStringLiteral("set /tracks/0/notes/0/pitchBend null")));
        QCOMPARE(get("get /tracks/0/notes/0/pitchBend"), QJsonValue(QJsonValue::Null));
    }

    void get_modifies_nothing() {
        ProjectSession session(richProject());
        const auto before = session.snapshot().toJson();
        DiagnosticList diagnostics;
        QVERIFY(
            ProjectCommands::query(session, QStringLiteral("get /tracks/0/notes"), diagnostics));
        QCOMPARE(session.currentStep(), 0);
        QVERIFY(!session.canUndo());
        QCOMPARE(session.snapshot().toJson(), before);
    }

    void a_malformed_query_is_refused() {
        ProjectSession session(richProject());
        for (const auto line : {"get", "get /tracks/0/notes/2", "get /tracks/0/lyric",
                                "get /tracks/0/notes/0/vibrato/depth/x",
                                "get /tracks/0/notes/0/lyric more", "find /", "", "\"get\" /"}) {
            DiagnosticList diagnostics;
            QVERIFY2(!ProjectCommands::query(session, QString::fromUtf8(line), diagnostics), line);
            QVERIFY2(hasError(diagnostics), line);
        }
        // A query is not a command, and a command is not a query.
        verifyRefused(session, QStringLiteral("get /tracks/0/notes/0/lyric"),
                      QStringLiteral("query"));
        DiagnosticList diagnostics;
        QVERIFY(!ProjectCommands::query(session, QStringLiteral("set /tracks/0/notes/0/lyric a"),
                                        diagnostics));
        QCOMPARE(noteAt(session, 0).lyric, QString::fromUtf8("あ"));
        QCOMPARE(ProjectCommands::queryNames(), QStringList{QStringLiteral("get")});
    }

    void names_lists_every_command() {
        QCOMPARE(
            ProjectCommands::names(),
            QStringList({QStringLiteral("set"), QStringLiteral("insert"), QStringLiteral("remove"),
                         QStringLiteral("move"), QStringLiteral("replace"),
                         QStringLiteral("note transpose"), QStringLiteral("note split"),
                         QStringLiteral("note insert"), QStringLiteral("note tempo"),
                         QStringLiteral("note remove"), QStringLiteral("note length"),
                         QStringLiteral("note move"), QStringLiteral("note portamento"),
                         QStringLiteral("note vibrato"), QStringLiteral("note envelope")}));
    }
};

QTEST_APPLESS_MAIN(test_ProjectCommands)

#include "test_ProjectCommands.moc"
