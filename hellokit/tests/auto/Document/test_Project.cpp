#include <filesystem>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtTest/QTest>

#include <hellokit/Document/Project.h>

using namespace hello::kit;
namespace fs = std::filesystem;

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

    static QString textOf(const fs::path &path) {
        return QString::fromStdU16String(path.lexically_normal().generic_u16string());
    }

    static QString nativeOf(const fs::path &path) {
        return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
    }

    static QString resolved(QStringView voiceDir, const fs::path &utauDirectory) {
        Track track;
        track.voiceDir = voiceDir.toString();
        const auto path = track.voiceDirectory(utauDirectory);
        return path.empty() ? QString() : textOf(path);
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
    void an_absent_field_and_a_null_field_are_equivalent() {
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

    // An envelope has four or five anchors. Any other number is reported, and the note is read
    // without the envelope rather than rejected.
    void an_envelope_with_another_number_of_anchors_is_reported() {
        DiagnosticList diagnostics;
        const auto json = QByteArray(
            R"({"$format":"usth","version":1,"settings":{},"tracks":[{"notes":[)"
            R"({"lyric":"a","length":480,"noteNum":60,)"
            R"("envelope":{"anchors":[{"x":0,"y":0},{"x":5,"y":100},{"x":0,"y":0}]}}]}]})");
        const auto project = Project::fromJson(json, diagnostics);
        QVERIFY(project.has_value());
        QVERIFY(!project->tracks.first().notes.first().envelope.has_value());
        QVERIFY(!hasError(diagnostics));
        QCOMPARE(diagnostics.size(), 1);
    }

    void a_diagnostic_of_a_note_records_the_index_of_the_note() {
        DiagnosticList diagnostics;
        const auto json =
            QByteArray(R"({"$format":"usth","version":1,"settings":{},"tracks":[{"notes":[)"
                       R"({"lyric":"a","length":480,"noteNum":60},)"
                       R"({"lyric":"a","length":480,"noteNum":60,"intensity":"loud"}]}]})");
        QVERIFY(Project::fromJson(json, diagnostics).has_value());
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(diagnostics.at(0).noteIndex, std::optional<int>(1));

        diagnostics.clear();
        const auto missing =
            QByteArray(R"({"$format":"usth","version":1,"settings":{},"tracks":[{"notes":[)"
                       R"({"lyric":"a","length":480,"noteNum":60},{"lyric":"a"}]}]})");
        QVERIFY(!Project::fromJson(missing, diagnostics).has_value());
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(diagnostics.at(0).noteIndex, std::optional<int>(1));
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
        note.envelope = Envelope::fromTimeOrder({
            {0,  0  },
            {5,  100},
            {35, 100},
            {0,  0  }
        });
        note.vibrato = Vibrato{65, 180, 35, 20, 20, 0, 0, 0};
        note.portamento = {
            {-40, 0,  PortamentoPoint::S     },
            {50,  10, PortamentoPoint::Linear}
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
        QVERIFY(!back.envelope->hasMiddle);
        QVERIFY(*back.envelope == *note.envelope);
        QVERIFY(back.vibrato.has_value());
        QCOMPARE(back.vibrato->period, 180.0);
        QCOMPARE(back.portamento.size(), 2);
        QVERIFY(back.portamento.at(0).type == PortamentoPoint::S);
        QVERIFY(back.portamento.at(1).type == PortamentoPoint::Linear);
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
        QCOMPARE(again->settings.wavtool, QStringLiteral("C:\\evil\\wavtool.exe"));
        QCOMPARE(again->settings.resampler, QStringLiteral("C:\\evil\\resampler.exe"));
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

    void the_file_is_utf8_without_a_bom() {
        auto project = oneNote();
        project.tracks[0].notes[0].lyric = QString::fromUtf8("あ");
        project.settings.name = QString::fromUtf8("中文工程");

        const auto written = project.toJson();
        QVERIFY(!written.startsWith("\xEF\xBB\xBF"));
        QVERIFY(written.contains(QString::fromUtf8("中文工程").toUtf8()));

        const auto again = parsed(written);
        QVERIFY(again.has_value());
        QCOMPARE(again->tracks.first().notes.first().lyric, QString::fromUtf8("あ"));
    }

    // Compact by default, since indentation makes a project several times larger. The indented
    // form breaks lines with LF only, also on Windows.
    void the_file_is_compact_unless_indentation_is_requested() {
        const auto dir = fs::temp_directory_path();
        const auto compact = dir / u"hellokit_compact.usth";
        const auto indented = dir / u"hellokit_indented.usth";
        DiagnosticList diagnostics;
        QVERIFY(oneNote().save(compact, diagnostics));
        QVERIFY(oneNote().save(indented, diagnostics, QJsonDocument::Indented));

        const auto read = [](const fs::path &path) {
            QFile file(QString::fromStdU16String(path.u16string()));
            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
        };
        const auto compactBytes = read(compact);
        const auto indentedBytes = read(indented);
        fs::remove(compact);
        fs::remove(indented);

        QVERIFY(!compactBytes.contains('\n'));
        QVERIFY(!compactBytes.contains("  "));
        QVERIFY(indentedBytes.contains("\n    \""));
        QVERIFY(!indentedBytes.contains('\r'));
        QVERIFY(compactBytes.size() < indentedBytes.size());
    }

    // Measured in UTAU: a relative path is relative to the directory of utau.exe, not to the
    // project file or the voice directory. See docs/claude/utau-voicedir-cachedir.md.
    void a_voice_dir_resolves_as_utau_resolves_it() {
        const auto utau = fs::temp_directory_path() / u"utau";
        const auto bank = fs::temp_directory_path() / u"bank";

        QCOMPARE(resolved(u"%VOICE%uta", utau), textOf(utau / u"voice" / u"uta"));
        QCOMPARE(resolved(u"%VOICE%\\uta", utau), textOf(utau / u"voice" / u"uta"));
        QCOMPARE(resolved(u"%VOICE%sub\\uta", utau), textOf(utau / u"voice" / u"sub" / u"uta"));
        QCOMPARE(resolved(u"hp_rel", utau), textOf(utau / u"hp_rel"));
        QCOMPARE(resolved(nativeOf(bank), utau), textOf(bank));
        QCOMPARE(resolved(u"", utau), QString());

        // Without the UTAU directory, only an absolute path can be resolved.
        QCOMPARE(resolved(u"%VOICE%uta", {}), QString());
        QCOMPARE(resolved(u"hp_rel", {}), QString());
        QCOMPARE(resolved(nativeOf(bank), {}), textOf(bank));
    }

    // Measured in UTAU: the cache is named after the project file, and saving writes that name
    // whatever the file said before.
    void the_cache_dir_follows_the_file_name() {
        const auto dir = fs::temp_directory_path();
        QCOMPARE(Project::cacheDirOf(dir / u"song.ust"), QStringLiteral("song.cache"));
        QCOMPARE(Project::cacheDirOf(dir / u"a.b.usth"), QStringLiteral("a.b.cache"));
        QCOMPARE(Project::cacheDirOf(dir / u"歌.ust"), QString::fromUtf8("歌.cache"));
        QCOMPARE(textOf(Project::cacheDirectoryOf(dir / u"song.ust")), textOf(dir / u"song.cache"));

        auto project = oneNote();
        project.settings.cacheDir = QStringLiteral("old.cache");
        const auto path = dir / u"hellokit_cachedir.usth";
        DiagnosticList diagnostics;
        QVERIFY(project.save(path, diagnostics));
        const auto again = Project::open(path, diagnostics);
        fs::remove(path);
        QVERIFY(again.has_value());
        QCOMPARE(again->settings.cacheDir, QStringLiteral("hellokit_cachedir.cache"));
        QCOMPARE(project.settings.cacheDir, QStringLiteral("old.cache"));
    }

    // Measured in UTAU: an absolute path inside the voice directory is saved with the prefix.
    void a_voice_dir_is_written_as_utau_writes_it() {
        const auto utau = fs::temp_directory_path() / u"utau";
        const auto voice = utau / u"voice";
        const auto bank = fs::temp_directory_path() / u"bank";
        const QString separator = QDir::separator();

        QCOMPARE(Track::voiceDirOf(voice / u"hp_abs", utau), QStringLiteral("%VOICE%hp_abs"));
        QCOMPARE(Track::voiceDirOf(voice / u"hp_abs" / u"", utau), QStringLiteral("%VOICE%hp_abs"));
        QCOMPARE(Track::voiceDirOf(voice / u"sub" / u"uta", utau),
                 QStringLiteral("%VOICE%sub") + separator + QStringLiteral("uta"));
        QCOMPARE(Track::voiceDirOf(bank, utau), nativeOf(bank));
        QCOMPARE(Track::voiceDirOf(voice, utau), nativeOf(voice));
        QCOMPARE(Track::voiceDirOf(utau / u"voices" / u"uta", utau),
                 nativeOf(utau / u"voices" / u"uta"));
        QCOMPARE(Track::voiceDirOf(voice / u"hp_abs", {}), nativeOf(voice / u"hp_abs"));
    }
};

QTEST_APPLESS_MAIN(test_Project)

#include "test_Project.moc"
