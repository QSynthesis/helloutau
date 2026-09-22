#include <QtCore/QByteArray>
#include <QtTest/QTest>

#include <hellokit/Document/Project.h>

using namespace hello::kit;

class test_Project : public QObject {
    Q_OBJECT

private:
    static QByteArray minimal(const char *extra = "") {
        return QByteArray(R"({"$format":"usth","version":1,)") + extra +
               R"("settings":{},"tracks":[{"notes":[]}]})";
    }

    static std::optional<Project> parsed(const QByteArray &json) {
        DiagnosticList diagnostics;
        return Project::fromJson(json, diagnostics);
    }

    static Project oneNote() {
        Note note;
        note.lyric = QStringLiteral("la");
        note.length = 480;
        note.noteNum = 60;

        Track track;
        track.notes.push_back(note);

        Project project;
        project.tracks.push_back(track);
        return project;
    }

private Q_SLOTS:
    void a_minimal_project_reads() {
        const auto project = parsed(minimal());
        QVERIFY(project.has_value());
        QCOMPARE(project->tracks.size(), 1);
        QCOMPARE(project->settings.tempo, 120.0);
        QVERIFY(project->settings.mode2);
    }

    // Valid JSON is not necessarily a project. Without this check every field would be read as
    // missing, and the user would receive an empty project instead of an error message.
    void json_that_is_not_a_project_is_refused() {
        DiagnosticList diagnostics;
        QVERIFY(!Project::fromJson(R"({"hello":1})", diagnostics).has_value());
        QVERIFY(hasError(diagnostics));

        diagnostics.clear();
        QVERIFY(!Project::fromJson(R"({"$format":"ustx","version":1})", diagnostics).has_value());
        QVERIFY(hasError(diagnostics));
    }

    void broken_json_is_refused() {
        DiagnosticList diagnostics;
        QVERIFY(!Project::fromJson(R"({"$format":"usth",)", diagnostics).has_value());
        QVERIFY(hasError(diagnostics));
    }

    void a_newer_version_is_refused_rather_than_guessed_at() {
        DiagnosticList diagnostics;
        const auto json =
            QByteArray(R"({"$format":"usth","version":99,"settings":{},"tracks":[{}]})");
        QVERIFY(!Project::fromJson(json, diagnostics).has_value());
        QVERIFY(hasError(diagnostics));
    }

    // The array exists to allow multiple tracks later. A build that supports one track must
    // reject additional tracks rather than open the file with parts missing.
    void more_than_one_track_is_refused_not_trimmed() {
        DiagnosticList diagnostics;
        auto json = QByteArray(
            R"({"$format":"usth","version":1,"settings":{},"tracks":[{"notes":[]},{"notes":[]}]})");
        QVERIFY(!Project::fromJson(json, diagnostics).has_value());
        QVERIFY(hasError(diagnostics));

        diagnostics.clear();
        json = QByteArray(R"({"$format":"usth","version":1,"settings":{},"tracks":[]})");
        QVERIFY(!Project::fromJson(json, diagnostics).has_value());
        QVERIFY(hasError(diagnostics));
    }

    // An absent field and a null field are equivalent and differ from zero, which is the
    // reason these fields are optional.
    void absent_and_null_both_mean_the_file_did_not_say() {
        const auto json =
            QByteArray(R"({"$format":"usth","version":1,"settings":{},"tracks":[{"notes":[)"
                       R"({"lyric":"a","length":480,"noteNum":60,"intensity":null},)"
                       R"({"lyric":"a","length":480,"noteNum":60},)"
                       R"({"lyric":"a","length":480,"noteNum":60,"intensity":0}]}]})");

        const auto project = parsed(json);
        QVERIFY(project.has_value());

        const auto &notes = project->tracks.first().notes;
        QCOMPARE(notes.size(), 3);
        QVERIFY(!notes.at(0).intensity.has_value());
        QVERIFY(!notes.at(1).intensity.has_value());
        QVERIFY(notes.at(2).intensity.has_value());
        QCOMPARE(*notes.at(2).intensity, 0.0);
    }

    void a_note_missing_a_required_field_is_an_error() {
        DiagnosticList diagnostics;
        const auto json = QByteArray(
            R"({"$format":"usth","version":1,"settings":{},"tracks":[{"notes":[{"lyric":"a"}]}]})");
        QVERIFY(!Project::fromJson(json, diagnostics).has_value());
        QVERIFY(hasError(diagnostics));
    }

    // An older build must not discard data written by a newer one. Otherwise the user loses it
    // on the next save.
    void unknown_top_level_fields_come_back() {
        const auto project = parsed(minimal(R"("somethingNew":{"a":1},)"));
        QVERIFY(project.has_value());

        const auto written = project->toJson();
        QVERIFY(written.contains("somethingNew"));

        const auto again = parsed(written);
        QVERIFY(again.has_value());
        QVERIFY(again->unknownFields.contains(QStringLiteral("somethingNew")));
    }

    void everything_a_note_carries_survives_a_round_trip() {
        auto project = oneNote();
        auto &note = project.tracks[0].notes[0];

        note.intensity = 80;
        note.velocity = 0; // zero, which must remain a value rather than become absent
        note.tempo = 128.5;
        note.flags = QStringLiteral("g-5");
        note.envelope = Envelope{
            {{0, 0}, {5, 100}, {35, 100}, {0, 0}}
        };
        note.vibrato = Vibrato{65, 180, 35, 20, 20, 0, 0, 0};
        note.portamento = {
            {-40, 0,  PortamentoType::S     },
            {50,  10, PortamentoType::Linear}
        };
        note.label = QStringLiteral("verse");
        note.patch = QStringLiteral("resampler.exe");
        note.userData.insert(QStringLiteral("$whatever"), QStringLiteral("kept"));

        const auto again = parsed(project.toJson());
        QVERIFY(again.has_value());

        const auto &back = again->tracks.first().notes.first();
        QCOMPARE(back.lyric, QStringLiteral("la"));
        QCOMPARE(back.length, 480);
        QCOMPARE(back.noteNum, 60);
        QVERIFY(back.intensity.has_value());
        QCOMPARE(*back.intensity, 80.0);
        QVERIFY(back.velocity.has_value());
        QCOMPARE(*back.velocity, 0.0);
        QVERIFY(!back.modulation.has_value());
        QCOMPARE(back.flags, QStringLiteral("g-5"));
        QVERIFY(back.envelope.has_value());
        QCOMPARE(back.envelope->anchors.size(), 4);
        QVERIFY(back.vibrato.has_value());
        QCOMPARE(back.vibrato->period, 180.0);
        QCOMPARE(back.portamento.size(), 2);
        QVERIFY(back.portamento.at(0).type == PortamentoType::S);
        QVERIFY(back.portamento.at(1).type == PortamentoType::Linear);
        QCOMPARE(back.label, QStringLiteral("verse"));
        QCOMPARE(back.patch, QStringLiteral("resampler.exe"));
        QCOMPARE(back.userData.value(QStringLiteral("$whatever")), QStringLiteral("kept"));
    }

    // Per-project engine paths are a commonly used setting, so discarding them would delete
    // user data in the name of safety. Not executing them is a separate matter.
    void the_engine_paths_are_kept() {
        auto project = oneNote();
        project.settings.wavtool = QStringLiteral("C:/evil/wavtool.exe");
        project.settings.resampler = QStringLiteral("C:/evil/resampler.exe");

        const auto again = parsed(project.toJson());
        QVERIFY(again.has_value());
        QCOMPARE(again->settings.wavtool, QStringLiteral("C:/evil/wavtool.exe"));
        QCOMPARE(again->settings.resampler, QStringLiteral("C:/evil/resampler.exe"));
    }

    void the_mode1_pitch_curve_survives_a_round_trip() {
        auto project = oneNote();
        project.tracks[0].notes[0].pitchBend = PitchBend{
            -20.0, {0, 10.5, -20}
        };

        const auto again = parsed(project.toJson());
        QVERIFY(again.has_value());
        const auto &bend = again->tracks.first().notes.first().pitchBend;
        QVERIFY(bend.has_value());
        QVERIFY(bend->start.has_value());
        QCOMPARE(*bend->start, -20.0);
        QCOMPARE(bend->values.size(), 3);
        QCOMPARE(bend->values.at(1), 10.5);
    }

    void the_file_is_utf8_without_a_bom_and_uses_newlines() {
        auto project = oneNote();
        project.tracks[0].notes[0].lyric = QString::fromUtf8("あ");
        project.settings.name = QString::fromUtf8("中文工程");

        const auto written = project.toJson();
        QVERIFY(!written.startsWith("\xEF\xBB\xBF"));
        QVERIFY(!written.contains("\r\n"));
        QVERIFY(written.contains(QString::fromUtf8("中文工程").toUtf8()));

        const auto again = parsed(written);
        QVERIFY(again.has_value());
        QCOMPARE(again->tracks.first().notes.first().lyric, QString::fromUtf8("あ"));
    }
};

QTEST_APPLESS_MAIN(test_Project)

#include "test_Project.moc"
