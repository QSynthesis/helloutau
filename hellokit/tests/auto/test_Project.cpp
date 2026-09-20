#define BOOST_TEST_MAIN
#define BOOST_TEST_MODULE test_Project

#include <string>

#include <boost/test/unit_test.hpp>

#include <QtCore/QByteArray>

#include <hellokit/Document/Project.h>

using namespace hello::kit;

namespace {

    QByteArray minimal(const char *extra = "") {
        return QByteArray(R"({"$format":"usth","version":1,)") + extra +
               R"("settings":{},"tracks":[{"notes":[]}]})";
    }

    std::optional<Project> parsed(const QByteArray &json) {
        DiagnosticList diagnostics;
        return Project::parse(json, diagnostics);
    }

    Project oneNote() {
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

}

BOOST_AUTO_TEST_SUITE(test_Project)

BOOST_AUTO_TEST_CASE(a_minimal_project_reads) {
    auto project = parsed(minimal());
    BOOST_REQUIRE(project.has_value());
    BOOST_CHECK_EQUAL(project->tracks.size(), 1);
    BOOST_CHECK_CLOSE(project->settings.tempo, 120.0, 0.01);
    BOOST_CHECK(project->settings.mode2);
}

// A file that happens to be JSON is not thereby a project. Without this check every field would
// be read as missing and the user would get an empty project instead of a message.
BOOST_AUTO_TEST_CASE(json_that_is_not_a_project_is_refused) {
    DiagnosticList diagnostics;
    BOOST_CHECK(!Project::parse(R"({"hello":1})", diagnostics).has_value());
    BOOST_CHECK(hasError(diagnostics));

    diagnostics.clear();
    BOOST_CHECK(!Project::parse(R"({"$format":"ustx","version":1})", diagnostics).has_value());
    BOOST_CHECK(hasError(diagnostics));
}

BOOST_AUTO_TEST_CASE(broken_json_is_refused) {
    DiagnosticList diagnostics;
    BOOST_CHECK(!Project::parse(R"({"$format":"usth",)", diagnostics).has_value());
    BOOST_CHECK(hasError(diagnostics));
}

BOOST_AUTO_TEST_CASE(a_newer_version_is_refused_rather_than_guessed_at) {
    DiagnosticList diagnostics;
    auto json = QByteArray(R"({"$format":"usth","version":99,"settings":{},"tracks":[{}]})");
    BOOST_CHECK(!Project::parse(json, diagnostics).has_value());
    BOOST_CHECK(hasError(diagnostics));
}

// The array is there so that several tracks become possible later. A build that holds one has to
// refuse the rest rather than open the file with the other parts missing.
BOOST_AUTO_TEST_CASE(more_than_one_track_is_refused_not_trimmed) {
    DiagnosticList diagnostics;
    auto json = QByteArray(
        R"({"$format":"usth","version":1,"settings":{},"tracks":[{"notes":[]},{"notes":[]}]})");
    BOOST_CHECK(!Project::parse(json, diagnostics).has_value());
    BOOST_CHECK(hasError(diagnostics));

    diagnostics.clear();
    json = QByteArray(R"({"$format":"usth","version":1,"settings":{},"tracks":[]})");
    BOOST_CHECK(!Project::parse(json, diagnostics).has_value());
    BOOST_CHECK(hasError(diagnostics));
}

// Absent and null are the same thing and neither is zero, which is the whole point of holding
// these in an optional.
BOOST_AUTO_TEST_CASE(absent_and_null_both_mean_the_file_did_not_say) {
    auto json = QByteArray(
        R"({"$format":"usth","version":1,"settings":{},"tracks":[{"notes":[)"
        R"({"lyric":"a","length":480,"noteNum":60,"intensity":null},)"
        R"({"lyric":"a","length":480,"noteNum":60},)"
        R"({"lyric":"a","length":480,"noteNum":60,"intensity":0}]}]})");

    auto project = parsed(json);
    BOOST_REQUIRE(project.has_value());

    const auto &notes = project->tracks.first().notes;
    BOOST_REQUIRE_EQUAL(notes.size(), 3);
    BOOST_CHECK(!notes.at(0).intensity.has_value());
    BOOST_CHECK(!notes.at(1).intensity.has_value());
    BOOST_REQUIRE(notes.at(2).intensity.has_value());
    BOOST_CHECK_CLOSE(*notes.at(2).intensity, 0.0, 0.01);
}

BOOST_AUTO_TEST_CASE(a_note_missing_a_required_field_is_an_error) {
    DiagnosticList diagnostics;
    auto json = QByteArray(
        R"({"$format":"usth","version":1,"settings":{},"tracks":[{"notes":[{"lyric":"a"}]}]})");
    BOOST_CHECK(!Project::parse(json, diagnostics).has_value());
    BOOST_CHECK(hasError(diagnostics));
}

// An older build must not eat what a newer one wrote, or the user loses it the next time they
// press save.
BOOST_AUTO_TEST_CASE(unknown_top_level_fields_come_back) {
    auto project = parsed(minimal(R"("somethingNew":{"a":1},)"));
    BOOST_REQUIRE(project.has_value());

    const auto written = project->serialize();
    BOOST_CHECK(written.contains("somethingNew"));

    auto again = parsed(written);
    BOOST_REQUIRE(again.has_value());
    BOOST_CHECK(again->unknownFields.contains(QStringLiteral("somethingNew")));
}

BOOST_AUTO_TEST_CASE(everything_a_note_carries_survives_a_round_trip) {
    auto project = oneNote();
    auto &note = project.tracks[0].notes[0];

    note.intensity = 80;
    note.velocity = 0; // zero, which has to stay a value rather than become absent
    note.tempo = 128.5;
    note.flags = QStringLiteral("g-5");
    note.envelope = Envelope{{{0, 0}, {5, 100}, {35, 100}, {0, 0}}};
    note.vibrato = Vibrato{65, 180, 35, 20, 20, 0, 0};
    note.portamento = {{-40, 0, PortamentoType::S}, {50, 10, PortamentoType::Linear}};
    note.label = QStringLiteral("verse");
    note.patch = QStringLiteral("resampler.exe");
    note.userData.insert(QStringLiteral("$whatever"), QStringLiteral("kept"));

    auto again = parsed(project.serialize());
    BOOST_REQUIRE(again.has_value());

    const auto &back = again->tracks.first().notes.first();
    BOOST_CHECK_EQUAL(back.lyric.toStdString(), "la");
    BOOST_CHECK_EQUAL(back.length, 480);
    BOOST_CHECK_EQUAL(back.noteNum, 60);
    BOOST_REQUIRE(back.intensity.has_value());
    BOOST_CHECK_CLOSE(*back.intensity, 80.0, 0.01);
    BOOST_REQUIRE(back.velocity.has_value());
    BOOST_CHECK_CLOSE(*back.velocity, 0.0, 0.01);
    BOOST_CHECK(!back.modulation.has_value());
    BOOST_CHECK_EQUAL(back.flags.toStdString(), "g-5");
    BOOST_REQUIRE(back.envelope.has_value());
    BOOST_CHECK_EQUAL(back.envelope->anchors.size(), 4);
    BOOST_REQUIRE(back.vibrato.has_value());
    BOOST_CHECK_CLOSE(back.vibrato->period, 180.0, 0.01);
    BOOST_REQUIRE_EQUAL(back.portamento.size(), 2);
    BOOST_CHECK(back.portamento.at(0).type == PortamentoType::S);
    BOOST_CHECK(back.portamento.at(1).type == PortamentoType::Linear);
    BOOST_CHECK_EQUAL(back.label.toStdString(), "verse");
    BOOST_CHECK_EQUAL(back.patch.toStdString(), "resampler.exe");
    BOOST_CHECK_EQUAL(back.userData.value(QStringLiteral("$whatever")).toStdString(), "kept");
}

// The engine paths are a per project setting people really use, so throwing them away would be
// deleting the user's work under cover of safety. Not running them is a separate matter.
BOOST_AUTO_TEST_CASE(the_engine_paths_are_kept) {
    auto project = oneNote();
    project.settings.wavtool = QStringLiteral("C:/evil/wavtool.exe");
    project.settings.resampler = QStringLiteral("C:/evil/resampler.exe");

    auto again = parsed(project.serialize());
    BOOST_REQUIRE(again.has_value());
    BOOST_CHECK_EQUAL(again->settings.wavtool.toStdString(), "C:/evil/wavtool.exe");
    BOOST_CHECK_EQUAL(again->settings.resampler.toStdString(), "C:/evil/resampler.exe");
}

// The sentinel stands for a reading the curve does not have. Writing it out as a number would
// put a real pitch of -32768 in the file.
BOOST_AUTO_TEST_CASE(an_empty_pitch_sample_is_written_as_null) {
    auto project = oneNote();
    project.tracks[0].notes[0].pitchBend = PitchBend{-20.0, {0, PitchBend::noValue, 20}};

    const auto written = project.serialize();
    BOOST_CHECK(!written.contains("-32768"));

    auto again = parsed(written);
    BOOST_REQUIRE(again.has_value());
    const auto &bend = again->tracks.first().notes.first().pitchBend;
    BOOST_REQUIRE(bend.has_value());
    BOOST_REQUIRE_EQUAL(bend->values.size(), 3);
    BOOST_CHECK_EQUAL(bend->values.at(1), PitchBend::noValue);
}

BOOST_AUTO_TEST_CASE(the_file_is_utf8_without_a_bom_and_uses_newlines) {
    auto project = oneNote();
    project.tracks[0].notes[0].lyric = QString::fromUtf8("あ");
    project.settings.name = QString::fromUtf8("中文工程");

    const auto written = project.serialize();
    BOOST_CHECK(!written.startsWith("\xEF\xBB\xBF"));
    BOOST_CHECK(!written.contains("\r\n"));
    BOOST_CHECK(written.contains(QString::fromUtf8("中文工程").toUtf8()));

    auto again = parsed(written);
    BOOST_REQUIRE(again.has_value());
    BOOST_CHECK(again->tracks.first().notes.first().lyric == QString::fromUtf8("あ"));
}

BOOST_AUTO_TEST_SUITE_END()
