#include <filesystem>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QTemporaryDir>
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
        const auto path = track.voiceDirectory(VoiceLocations::ofUtau(utauDirectory));
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

    // Per-project synth tool paths are a commonly used setting, so discarding them would delete
    // user data in the name of safety. Not executing them is a separate matter.
    void the_synth_tool_paths_are_kept() {
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
        // A voice folder that does not exist is skipped, so this one must exist.
        QTemporaryDir dir;
        const auto utau = fs::path(dir.path().toStdU16String()) / u"utau";
        fs::create_directories(utau / u"voice");
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
        QCOMPARE(Project::cacheDirTextOf(dir / u"song.ust"), QStringLiteral("song.cache"));
        QCOMPARE(Project::cacheDirTextOf(dir / u"a.b.usth"), QStringLiteral("a.b.cache"));
        QCOMPARE(Project::cacheDirTextOf(dir / u"歌.ust"), QString::fromUtf8("歌.cache"));
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
        QTemporaryDir dir;
        const auto utau = fs::path(dir.path().toStdU16String()) / u"utau";
        const auto voice = utau / u"voice";
        fs::create_directories(voice);
        const auto bank = fs::temp_directory_path() / u"bank";
        const QString separator = QDir::separator();
        const auto locations = VoiceLocations::ofUtau(utau);

        QCOMPARE(Track::voiceDirOf(voice / u"hp_abs", locations), QStringLiteral("%VOICE%hp_abs"));
        QCOMPARE(Track::voiceDirOf(voice / u"hp_abs" / u"", locations),
                 QStringLiteral("%VOICE%hp_abs"));
        QCOMPARE(Track::voiceDirOf(voice / u"sub" / u"uta", locations),
                 QStringLiteral("%VOICE%sub") + separator + QStringLiteral("uta"));
        QCOMPARE(Track::voiceDirOf(bank, locations), nativeOf(bank));
        QCOMPARE(Track::voiceDirOf(voice, locations), nativeOf(voice));
        QCOMPARE(Track::voiceDirOf(utau / u"voices" / u"uta", locations),
                 nativeOf(utau / u"voices" / u"uta"));
        QCOMPARE(Track::voiceDirOf(voice / u"hp_abs", {}), nativeOf(voice / u"hp_abs"));
    }

    // A saved project writes backslashes, as UTAU does, except in an absolute Unix path. Nothing
    // else changes, so that .. and %VOICE% keep their meaning.
    void a_saved_path_uses_the_separators_of_utau() {
        QCOMPARE(Project::savedPathText(QStringLiteral("C:/x/y")), QStringLiteral("C:\\x\\y"));
        QCOMPARE(Project::savedPathText(QStringLiteral("C:\\x/y")), QStringLiteral("C:\\x\\y"));
        QCOMPARE(Project::savedPathText(QStringLiteral("/Users/x\\y")),
                 QStringLiteral("/Users/x/y"));
        QCOMPARE(Project::savedPathText(QStringLiteral("%VOICE%uta/sub")),
                 QStringLiteral("%VOICE%uta\\sub"));
        QCOMPARE(Project::savedPathText(QStringLiteral("%VOICE%uta\\..\\other")),
                 QStringLiteral("%VOICE%uta\\..\\other"));
        QCOMPARE(Project::savedPathText(QString()), QString());
    }

    // The five path fields are saved by the same rule, and a voice directory with .. still names
    // the same voice bank after a round trip.
    void the_path_fields_are_saved_with_the_separators_of_utau() {
        auto project = oneNote();
        project.settings.outputFile = QStringLiteral("out/song.wav");
        project.settings.cacheDir = QStringLiteral("song.cache/sub");
        project.settings.wavtool = QStringLiteral("tools/wavtool.exe");
        project.settings.resampler = QStringLiteral("/opt/tools\\resampler");
        project.tracks[0].voiceDir = QStringLiteral("%VOICE%uta/../other");

        const auto again = parsed(project.toJson());
        QVERIFY(again.has_value());
        QCOMPARE(again->settings.outputFile, QStringLiteral("out\\song.wav"));
        QCOMPARE(again->settings.cacheDir, QStringLiteral("song.cache\\sub"));
        QCOMPARE(again->settings.wavtool, QStringLiteral("tools\\wavtool.exe"));
        QCOMPARE(again->settings.resampler, QStringLiteral("/opt/tools/resampler"));
        QCOMPARE(again->tracks[0].voiceDir, QStringLiteral("%VOICE%uta\\..\\other"));

        QTemporaryDir dir;
        const auto utau = fs::path(dir.path().toStdU16String()) / u"utau";
        fs::create_directories(utau / u"voice" / u"other");
        const auto locations = VoiceLocations::ofUtau(utau);
        QCOMPARE(textOf(again->tracks[0].voiceDirectory(locations)),
                 textOf(project.tracks[0].voiceDirectory(locations)));
        QCOMPARE(textOf(again->tracks[0].voiceDirectory(locations)),
                 textOf(utau / u"voice" / u"other"));
    }

    // UTAU writes backslashes, which std::filesystem reads as separators only on Windows.
    void a_path_text_reads_backslashes_on_every_platform() {
        const auto tool = Project::pathOf(QStringLiteral("tools\\resampler.exe"));
        QCOMPARE(std::distance(tool.begin(), tool.end()), 2);
        QCOMPARE(QString::fromStdU16String(tool.filename().u16string()),
                 QStringLiteral("resampler.exe"));

        const auto output = Project::pathOf(QStringLiteral("out\\song.wav"));
        QCOMPARE(std::distance(output.begin(), output.end()), 2);
        QVERIFY(output.is_relative());
#ifdef Q_OS_WINDOWS
        QVERIFY(Project::pathOf(QStringLiteral("C:\\x")).is_absolute());
#endif
    }

    // %VOICE% denotes the first voice folder that contains the voice bank, a missing voice folder
    // is skipped, and a relative path is relative to relativeBase.
    void a_voice_dir_resolves_against_several_voice_folders() {
        QTemporaryDir dir;
        const auto root = fs::path(dir.path().toStdU16String());
        const auto first = root / u"first";
        const auto second = root / u"second";
        const auto missing = root / u"missing";
        fs::create_directories(first / u"bank");
        fs::create_directories(second / u"bank");
        fs::create_directories(second / u"only");
        const auto resolvedIn = [](QStringView voiceDir, const VoiceLocations &locations) {
            Track track;
            track.voiceDir = voiceDir.toString();
            const auto path = track.voiceDirectory(locations);
            return path.empty() ? QString() : textOf(path);
        };

        const VoiceLocations both{
            {first, second},
            root
        };
        QCOMPARE(resolvedIn(u"%VOICE%bank", both), textOf(first / u"bank"));
        QCOMPARE(resolvedIn(u"%VOICE%only", both), textOf(second / u"only"));
        // In no voice folder: the path in the first existing one
        QCOMPARE(resolvedIn(u"%VOICE%none", both), textOf(first / u"none"));

        const VoiceLocations missingFirst{
            {missing, second},
            root
        };
        QCOMPARE(resolvedIn(u"%VOICE%none", missingFirst), textOf(second / u"none"));
        const VoiceLocations noneExists{{missing}, root};
        QCOMPARE(resolvedIn(u"%VOICE%bank", noneExists), QString());

        QCOMPARE(resolvedIn(u"rel", both), textOf(root / u"rel"));
        const VoiceLocations noBase{
            {first, second},
            {}
        };
        QCOMPARE(resolvedIn(u"rel", noBase), QString());

        // A folder of the same name in the first voice folder hides that of the second, which is
        // therefore written as an absolute path.
        QCOMPARE(Track::voiceDirOf(second / u"bank", both), nativeOf(second / u"bank"));
        QCOMPARE(Track::voiceDirOf(second / u"only", both), QStringLiteral("%VOICE%only"));
        QCOMPARE(Track::voiceDirOf(first / u"bank", both), QStringLiteral("%VOICE%bank"));
    }

    // Each start is paired with the first end of the same name from its note on, or with the last
    // note if none follows.
    void regions_pair_each_start_with_the_next_end() {
        const auto regionsOf = [](const QList<QStringList> &starts,
                                  const QList<QStringList> &ends) {
            return Region::of(starts, ends);
        };
        const QStringList none;
        const auto a = QStringLiteral("A");
        const auto b = QStringLiteral("B");

        // Two regions from one note, in the order of its names, with a shared end
        QCOMPARE(regionsOf(
                     {
                         {a, b},
                         none, none
        },
                     {none, none, {a, b}}),
                 (QList<Region>{{a, 0, 2}, {b, 0, 2}}));
        // Nested
        QCOMPARE(regionsOf(
                     {
                         {a},
                         {b},
                         none, none
        },
                     {none, none, {b}, {a}}),
                 (QList<Region>{{a, 0, 3}, {b, 1, 2}}));
        // Not ended: to the last note
        QCOMPARE(regionsOf(
                     {
                         none, {a},
                          none
        },
                     {none, none, none}),
                 (QList<Region>{{a, 1, 2}}));
        // A name used twice is paired twice
        QCOMPARE(regionsOf(
                     {
                         {a},
                         none, {a},
                         none
        },
                     {none, {a}, none, {a}}),
                 (QList<Region>{{a, 0, 1}, {a, 2, 3}}));
        // An end before the start does not count
        QCOMPARE(regionsOf(
                     {
                         none, {a},
                          none
        },
                     {{a}, none, none}),
                 (QList<Region>{{a, 1, 2}}));
        // Lists of different lengths: the shorter one counts
        QCOMPARE(regionsOf(
                     {
                         {a},
                         none, {b}
        },
                     {none, none}),
                 (QList<Region>{{a, 0, 1}}));
    }

    void a_time_signature_is_valid_within_its_bounds() {
        QVERIFY(TimeSignature::isValid(1, 4));
        QVERIFY(TimeSignature::isValid(TimeSignature::maximumNumerator, 4));
        QVERIFY(!TimeSignature::isValid(TimeSignature::maximumNumerator + 1, 4));
        QVERIFY(!TimeSignature::isValid(0, 4));
        for (const int denominator : {2, 4, 8, 16, 32}) {
            QVERIFY2(TimeSignature::isValid(3, denominator),
                     qPrintable(QString::number(denominator)));
        }
        for (const int denominator : {0, 1, 3, 64}) {
            QVERIFY2(!TimeSignature::isValid(3, denominator),
                     qPrintable(QString::number(denominator)));
        }
    }

    void a_time_signature_reads_only_valid_integers() {
        const auto objectOf = [](const QJsonValue &numerator, const QJsonValue &denominator) {
            QJsonObject object;
            if (!numerator.isUndefined()) {
                object.insert(QStringLiteral("numerator"), numerator);
            }
            if (!denominator.isUndefined()) {
                object.insert(QStringLiteral("denominator"), denominator);
            }
            return object;
        };
        const auto read = TimeSignature::fromJson(objectOf(6, 8));
        QVERIFY(read.has_value());
        QCOMPARE(*read, (TimeSignature{6, 8}));
        QCOMPARE(TimeSignature::fromJson(read->toJson()), read);

        QVERIFY(!TimeSignature::fromJson(objectOf(3.5, 4)).has_value());
        QVERIFY(!TimeSignature::fromJson(objectOf(QStringLiteral("3"), 4)).has_value());
        QVERIFY(!TimeSignature::fromJson(objectOf(3, QJsonValue::Undefined)).has_value());
        QVERIFY(!TimeSignature::fromJson(objectOf(QJsonValue::Undefined, 4)).has_value());
        QVERIFY(!TimeSignature::fromJson(objectOf(3, 6)).has_value());
    }

    // The time signature is written in settings.timeSignature, 4/4 if the file records none, and
    // 4/4 with a warning if the file records an invalid one.
    void the_time_signature_is_saved_with_the_project() {
        auto project = oneNote();
        project.settings.timeSignature = {3, 4};
        const auto json = project.toJson();
        const auto settings =
            QJsonDocument::fromJson(json).object().value(QStringLiteral("settings"));
        QCOMPARE(settings.toObject().value(QStringLiteral("timeSignature")).toObject(),
                 (TimeSignature{3, 4}).toJson());
        const auto again = parsed(json);
        QVERIFY(again.has_value());
        QCOMPARE(again->settings.timeSignature, (TimeSignature{3, 4}));

        DiagnosticList diagnostics;
        const auto absent = Project::fromJson(minimal(), diagnostics);
        QVERIFY(absent.has_value());
        QVERIFY(diagnostics.isEmpty());
        QCOMPARE(absent->settings.timeSignature, (TimeSignature{4, 4}));

        const QByteArray invalid =
            R"({"$format":"usth","version":1,"settings":{"timeSignature":{"numerator":0,)"
            R"("denominator":4}},"tracks":[{"notes":[]}]})";
        const auto reset = Project::fromJson(invalid, diagnostics);
        QVERIFY(reset.has_value());
        QCOMPARE(reset->settings.timeSignature, (TimeSignature{4, 4}));
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(diagnostics.at(0).severity, DiagnosticSeverity::Warning);
    }
};

QTEST_APPLESS_MAIN(test_Project)

#include "test_Project.moc"
