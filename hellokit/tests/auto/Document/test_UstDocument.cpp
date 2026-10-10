#include <filesystem>
#include <fstream>
#include <string>

#include <QtCore/QByteArray>
#include <QtTest/QTest>

#include <hellokit/Document/DocumentConstants.h>
#include <hellokit/Document/PayloadCodec.h>
#include <hellokit/Document/UstDocument.h>
#include <hellokit/Support/TextCodec.h>

using namespace hello::kit;
namespace fs = std::filesystem;

namespace {

    QString u(const char *utf8Text) {
        return QString::fromUtf8(utf8Text);
    }

    // A separate file per test case, removed afterward, because the tests verify the data
    // written to disk and read back from it.
    class TempUst {
    public:
        explicit TempUst(const std::string &name) {
            m_path = fs::temp_directory_path() / ("hellokit_" + name + ".ust");
        }

        ~TempUst() {
            std::error_code ignored;
            fs::remove(m_path, ignored);
        }

        void writeBytes(const QByteArray &bytes) const {
            std::ofstream out(m_path, std::ios::binary | std::ios::trunc);
            out.write(bytes.constData(), bytes.size());
        }

        QByteArray readBytes() const {
            std::ifstream in(m_path, std::ios::binary);
            const std::string all((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
            return QByteArray(all.data(), qsizetype(all.size()));
        }

        const fs::path &path() const {
            return m_path;
        }

    private:
        fs::path m_path;
    };

    Project oneNote(const QString &lyric = QStringLiteral("la")) {
        Note note;
        note.lyric = lyric;
        note.length = 480;
        note.noteNum = 60;

        Track track;
        track.voiceDir = QStringLiteral("%VOICE%uta");
        track.notes.push_back(note);

        Project project;
        project.settings.name = QStringLiteral("test");
        project.tracks.push_back(track);
        return project;
    }

    // A UST as written by UTAU, without a control note or an encoding declaration.
    QByteArray plainUst(const QByteArray &lyricBytes) {
        return QByteArray("[#VERSION]\r\nUST Version1.2\r\n[#SETTING]\r\nTempo=120.00\r\n"
                          "Tracks=1\r\nMode2=True\r\n[#0000]\r\nLength=480\r\nLyric=") +
               lyricBytes + "\r\nNoteNum=60\r\n[#TRACKEND]\r\n";
    }

    // Builds and writes in the same two steps as a real caller.
    bool writeTo(const Project &project, const TempUst &file,
                 const UstDocument::ExportOptions &options, DiagnosticList &diagnostics) {
        const auto ust = UstDocument::fromProject(project, options, diagnostics);
        return ust && ust->save(file.path(), diagnostics);
    }

    // Opens and converts in the same two steps as a real caller, with a single parse.
    std::optional<Project> readAs(const TempUst &file, const QString &charset,
                                  DiagnosticList *sink = nullptr) {
        DiagnosticList ignored;
        DiagnosticList &diagnostics = sink ? *sink : ignored;

        const auto ust = UstDocument::open(file.path(), diagnostics);
        if (!ust) {
            return std::nullopt;
        }
        return ust->toProject(charset, diagnostics);
    }

}

class test_UstDocument : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void a_ust_written_here_reads_back_the_same() {
        TempUst file("roundtrip");
        auto project = oneNote(u("あ"));
        auto &note = project.tracks[0].notes[0];
        note.intensity = 80;
        note.velocity = 0; // zero, which must remain a value rather than become absent
        note.flags = QStringLiteral("g-5");
        // Five anchors, so that the middle anchor survives the round trip in its position.
        note.envelope = Envelope::fromTimeOrder({
            {0,  0  },
            {5,  100},
            {20, 80 },
            {35, 100},
            {0,  0  }
        });
        note.vibrato = Vibrato{65, 180, 35, 20, 20, 0, 0, 7};
        note.portamento = {
            {-40, 0,  PortamentoPoint::S     },
            {50,  10, PortamentoPoint::Linear}
        };
        note.label = QStringLiteral("verse");
        note.userData.insert(QStringLiteral("$mine"), QStringLiteral("kept"));

        DiagnosticList diagnostics;
        QVERIFY(writeTo(project, file, {}, diagnostics));

        const auto again = readAs(file, QStringLiteral("UTF-8"));
        QVERIFY(again.has_value());
        QCOMPARE(again->tracks.size(), 1);

        const auto &back = again->tracks.first().notes;
        QCOMPARE(back.size(), 1);
        QCOMPARE(back.first().lyric, u("あ"));
        QCOMPARE(back.first().length, 480);
        QCOMPARE(back.first().noteNum, 60);
        QVERIFY(back.first().intensity.has_value());
        QCOMPARE(*back.first().intensity, 80.0);
        QVERIFY(back.first().velocity.has_value());
        QCOMPARE(*back.first().velocity, 0.0);
        QVERIFY(!back.first().modulation.has_value());
        QCOMPARE(back.first().flags, QStringLiteral("g-5"));
        QVERIFY(back.first().envelope.has_value());
        QVERIFY(back.first().envelope->hasMiddle);
        QVERIFY(*back.first().envelope == *note.envelope);
        QVERIFY(back.first().vibrato.has_value());
        QCOMPARE(back.first().vibrato->period, 180.0);
        QCOMPARE(back.first().portamento.size(), 2);
        QVERIFY(back.first().portamento.at(1).type == PortamentoPoint::Linear);
        QCOMPARE(back.first().label, QStringLiteral("verse"));
        QCOMPARE(back.first().userData.value(QStringLiteral("$mine")), QStringLiteral("kept"));
        QCOMPARE(again->tracks.first().voiceDir, QStringLiteral("%VOICE%uta"));
    }

    // The Mode1 values are written as UTAU writes those it draws, every value a number, zeros
    // at the end included: the last value holds for an interval (docs/Synth.md, "Mode1 的音高"),
    // so that dropping the zeros after it would change the curve.
    void mode1_values_are_written_each_with_the_zeros_at_the_end() {
        TempUst file("mode1");
        auto project = oneNote();
        project.tracks[0].notes[0].pitchBend = PitchBend{
            -8.938, {0, 17, 0, -5.5, 100, 0, 0}
        };

        DiagnosticList diagnostics;
        QVERIFY(writeTo(project, file, {}, diagnostics));
        const auto bytes = file.readBytes();
        QVERIFY2(bytes.contains("\r\nPitchBend=0,17,0,-5.5,100,0,0\r\n"), bytes.constData());
        QVERIFY2(bytes.contains("\r\nPBStart=-8.938\r\n"), bytes.constData());

        const auto again = readAs(file, QStringLiteral("UTF-8"));
        QVERIFY(again.has_value());
        QCOMPARE(again->tracks.first().notes.first().pitchBend,
                 project.tracks[0].notes[0].pitchBend);
    }

    // UTAU ignores the eighth value of VBR and exposes no field for it, but it must still be
    // preserved. Otherwise a file would lose data on every round trip.
    void the_vibrato_value_utau_ignores_still_comes_back() {
        TempUst file("vbr8");
        auto project = oneNote();
        project.tracks[0].notes[0].vibrato = Vibrato{65, 180, 35, 20, 20, 0, 0, 42};

        DiagnosticList diagnostics;
        QVERIFY(writeTo(project, file, {}, diagnostics));

        const auto again = readAs(file, QStringLiteral("UTF-8"));
        QVERIFY(again.has_value());
        const auto &vibrato = again->tracks.first().notes.first().vibrato;
        QVERIFY(vibrato.has_value());
        QCOMPARE(vibrato->intensity, 42.0);
    }

    // Measured in UTAU: saving writes CacheDir from the file name, whatever the project says.
    // See docs/claude/utau-voicedir-cachedir.md.
    void the_cache_dir_is_written_from_the_file_name_when_the_file_is_given() {
        TempUst file("cachedir");
        auto project = oneNote();
        project.settings.cacheDir = QStringLiteral("old.cache");

        DiagnosticList diagnostics;
        UstDocument::ExportOptions options;
        options.file = file.path();
        QVERIFY(writeTo(project, file, options, diagnostics));
        QVERIFY(file.readBytes().contains("CacheDir=hellokit_cachedir.cache\r\n"));

        // Without the file, as for a round trip that must not change the settings
        QVERIFY(writeTo(project, file, {}, diagnostics));
        QVERIFY(file.readBytes().contains("CacheDir=old.cache\r\n"));
    }

    // UST writes the intervals between the points and the heights in tenths of a semitone,
    // whereas a project keeps times from the start of the note and cents. A decimal height
    // converts in both directions without changing its text.
    void a_pitch_curve_is_read_as_times_and_cents_and_written_back_as_ust() {
        TempUst file("pitchcurve");
        file.writeBytes("[#VERSION]\r\nUST Version1.2\r\n[#SETTING]\r\nTempo=120.00\r\n"
                        "Tracks=1\r\nMode2=True\r\n[#0000]\r\nLength=480\r\nLyric=a\r\n"
                        "NoteNum=60\r\nPBS=-40;2.88\r\nPBW=50,30\r\nPBY=2.3,-0.7\r\nPBM=s,r\r\n"
                        "[#TRACKEND]\r\n");

        const auto project = readAs(file, QStringLiteral("UTF-8"));
        QVERIFY(project.has_value());
        const auto &points = project->tracks.first().notes.first().portamento;
        QCOMPARE(points.size(), 3);
        // Multiplied by ten without rounding, 2.88 gives 28.799999999999997
        QVERIFY(points[0].x == -40 && points[0].y == 28.8);
        QVERIFY(points[1].x == 10 && points[1].y == 23);
        QVERIFY(points[2].x == 40 && points[2].y == -7);
        QCOMPARE(points[1].type, PortamentoPoint::Linear);
        QCOMPARE(points[2].type, PortamentoPoint::R);

        DiagnosticList diagnostics;
        QVERIFY(writeTo(*project, file, {}, diagnostics));
        const auto bytes = file.readBytes();
        for (const char *line :
             {"PBS=-40;2.88\r\n", "PBW=50,30\r\n", "PBY=2.3,-0.7\r\n", "PBM=s,r\r\n"}) {
            QVERIFY2(bytes.contains(line), line);
        }
    }

    // One control note is written and exactly one is read. An error here would accumulate
    // leading half-second notes over repeated round trips, which would go unnoticed until
    // severe.
    void the_control_note_is_added_once_and_eaten_once() {
        TempUst file("control");
        const auto project = oneNote();

        DiagnosticList diagnostics;
        QVERIFY(writeTo(project, file, {}, diagnostics));

        const QByteArray written = file.readBytes();
        QVERIFY(written.contains(controlNoteLyric));
        QVERIFY(written.contains(controlNoteEntry));

        for (int pass = 0; pass < 3; ++pass) {
            const auto again = readAs(file, QStringLiteral("UTF-8"));
            QVERIFY2(again.has_value(), qPrintable(QStringLiteral("pass %1").arg(pass)));
            QCOMPARE(again->tracks.first().notes.size(), 1);
            QVERIFY(!again->tracks.first().notes.first().isRest());
            QVERIFY(writeTo(*again, file, {}, diagnostics));
        }
    }

    // The payload records what the Charset line cannot declare, so it must survive being
    // written in an encoding other than UTF-8 and remain locatable.
    void the_encoding_is_recorded_and_read_back() {
        TempUst file("charset");
        const auto project = oneNote(u("あ"));

        DiagnosticList diagnostics;
        QVERIFY(writeTo(project, file, {QStringLiteral("Shift_JIS"), {}, {}}, diagnostics));

        // UST can declare only UTF-8, so no declaration is written here.
        QVERIFY(!file.readBytes().contains("Charset="));

        const auto ust = UstDocument::open(file.path(), diagnostics);
        QVERIFY(ust.has_value());
        QVERIFY(ust->recordedCharset().has_value());
        QCOMPARE(*ust->recordedCharset(), QStringLiteral("Shift_JIS"));
        QVERIFY(ust->settledCharset().has_value());

        const auto again = readAs(file, *ust->settledCharset());
        QVERIFY(again.has_value());
        QCOMPARE(again->tracks.first().notes.first().lyric, u("あ"));
    }

    void utf8_is_declared_in_the_version_section() {
        TempUst file("utf8");
        DiagnosticList diagnostics;
        QVERIFY(writeTo(oneNote(), file, {}, diagnostics));
        QVERIFY(file.readBytes().contains("Charset=UTF-8"));

        const auto ust = UstDocument::open(file.path(), diagnostics);
        QVERIFY(ust.has_value());
        QVERIFY(ust->declaresUtf8());
    }

    // A lyric without a Shift_JIS representation must still round-trip, which is the purpose
    // of escaping.
    void a_lyric_the_encoding_cannot_hold_survives_as_an_escape() {
        TempUst file("escape");
        const auto project = oneNote(u("你"));

        DiagnosticList diagnostics;
        QVERIFY(writeTo(project, file, {QStringLiteral("Shift_JIS"), {}, {}}, diagnostics));
        QVERIFY(file.readBytes().contains("\\u4f60"));

        const auto again = readAs(file, QStringLiteral("Shift_JIS"));
        QVERIFY(again.has_value());
        QCOMPARE(again->tracks.first().notes.first().lyric, u("你"));
    }

    // A UST from UTAU does not use escaping, so its backslashes are literal. Unescaping it would
    // corrupt a Windows path.
    void a_foreign_ust_is_not_unescaped() {
        TempUst file("foreign");
        file.writeBytes(plainUst("C:\\utau\\voice"));

        const auto project = readAs(file, QStringLiteral("Shift_JIS"));
        QVERIFY(project.has_value());
        QCOMPARE(project->tracks.first().notes.first().lyric, QStringLiteral("C:\\utau\\voice"));
    }

    // The file does not declare its encoding, so the caller must ask the user.
    void a_file_without_an_encoding_declaration_settles_no_encoding() {
        TempUst file("unknown");
        file.writeBytes(plainUst("a"));

        DiagnosticList diagnostics;
        const auto ust = UstDocument::open(file.path(), diagnostics);
        QVERIFY(ust.has_value());
        QVERIFY(!ust->recordedCharset().has_value());
        QVERIFY(!ust->declaresUtf8());
        QVERIFY(!ust->settledCharset().has_value());
        QVERIFY(!ust->rawLyrics().isEmpty()); // sufficient for a preview
    }

    // Decoding with the wrong encoding must fail rather than produce replacement characters
    // that the user might then save over the project.
    void the_wrong_encoding_is_refused() {
        TempUst file("wrong");
        const TextCodec sjis(QStringLiteral("Shift_JIS"));
        const QByteArray lyric = sjis.encode(u("あ"));
        file.writeBytes(QByteArray("[#VERSION]\r\nUST Version1.2\r\n[#SETTING]\r\nProjectName=") +
                        lyric +
                        "\r\nTempo=120.00\r\n[#0000]\r\nLength=480\r\nLyric=a\r\n"
                        "NoteNum=60\r\n[#TRACKEND]\r\n");

        DiagnosticList diagnostics;
        QVERIFY(!readAs(file, QStringLiteral("UTF-8"), &diagnostics).has_value());
        QVERIFY(hasError(diagnostics));
    }

    // Mode2 is on where the setting is present, as UTAU writes it, and off without it or where it
    // is False, which UTAU reads as off (docs/Synth.md).
    void mode2_is_off_without_the_setting_or_where_it_is_false() {
        for (const auto &[line, on] : std::initializer_list<std::pair<QByteArray, bool>>{
                 {"Mode2=True\r\n",  true },
                 {"Mode2=False\r\n", false},
                 {"",                false}
        }) {
            TempUst file("mode2");
            file.writeBytes("[#VERSION]\r\nUST Version1.2\r\n[#SETTING]\r\nTempo=120.00\r\n" +
                            line +
                            "[#0000]\r\nLength=480\r\nLyric=a\r\nNoteNum=60\r\n[#TRACKEND]\r\n");
            const auto project = readAs(file, QStringLiteral("UTF-8"));
            QVERIFY(project.has_value());
            QCOMPARE(project->settings.mode2, on);
        }
    }

    // The synth tool paths of the project take precedence, and the local ones are used only if the
    // project specifies none.
    void the_synth_tools_come_from_the_project_first() {
        TempUst file("synthTools");
        auto project = oneNote();
        project.settings.resampler = QStringLiteral("own_resampler.exe");

        UstDocument::ExportOptions options;
        options.wavtool = QStringLiteral("local_wavtool.exe");
        options.resampler = QStringLiteral("local_resampler.exe");

        DiagnosticList diagnostics;
        QVERIFY(writeTo(project, file, options, diagnostics));

        const auto again = readAs(file, QStringLiteral("UTF-8"));
        QVERIFY(again.has_value());
        QCOMPARE(again->settings.resampler, QStringLiteral("own_resampler.exe"));
        QCOMPARE(again->settings.wavtool, QStringLiteral("local_wavtool.exe"));
    }

    // Project cannot represent all data of a UST, so the parse remains accessible. It yields
    // bytes in the encoding of the file, as intended.
    void the_parse_underneath_is_reachable_and_undecoded() {
        TempUst file("raw");
        const TextCodec sjis(QStringLiteral("Shift_JIS"));
        file.writeBytes(plainUst(sjis.encode(u("あ"))));

        DiagnosticList diagnostics;
        const auto ust = UstDocument::open(file.path(), diagnostics);
        QVERIFY(ust.has_value());

        QCOMPARE(ust->file().notes.size(), size_t(1));
        const std::string &raw = ust->file().notes.front().lyric;
        QCOMPARE(raw.size(), size_t(2)); // Shift_JIS bytes, not UTF-8 and not decoded
        QVERIFY(raw != u("あ").toStdString());

        const auto project = ust->toProject(QStringLiteral("Shift_JIS"), diagnostics);
        QVERIFY(project.has_value());
        QCOMPARE(project->tracks.first().notes.first().lyric, u("あ"));
    }

    void something_that_is_not_a_ust_is_an_error() {
        TempUst file("notaust");
        file.writeBytes("hello, this is not a UST at all");

        // stdutau reads it as a file without notes rather than rejecting it, which yields an
        // empty project. The requirement is that no content is fabricated.
        const auto project = readAs(file, QStringLiteral("UTF-8"));
        if (project) {
            QVERIFY(project->tracks.first().notes.isEmpty());
        }
    }

    // The path fields of an exported UST use the separators of UTAU, as in a saved project, and
    // a voice directory with .. comes back unchanged.
    void the_path_fields_are_exported_with_the_separators_of_utau() {
        TempUst file("paths");
        auto project = oneNote();
        project.settings.outputFile = QStringLiteral("out/song.wav");
        project.settings.cacheDir = QStringLiteral("song.cache/sub");
        project.settings.wavtool = QStringLiteral("tools/wavtool.exe");
        project.settings.resampler = QStringLiteral("/opt/tools\\resampler");
        project.tracks[0].voiceDir = QStringLiteral("%VOICE%uta/../other");

        DiagnosticList diagnostics;
        QVERIFY(writeTo(project, file, {}, diagnostics));
        const auto written = file.readBytes();
        QVERIFY(written.contains("\r\nOutFile=out\\song.wav\r\n"));
        QVERIFY(written.contains("\r\nCacheDir=song.cache\\sub\r\n"));
        QVERIFY(written.contains("\r\nTool1=tools\\wavtool.exe\r\n"));
        QVERIFY(written.contains("\r\nTool2=/opt/tools/resampler\r\n"));
        QVERIFY(written.contains("\r\nVoiceDir=%VOICE%uta\\..\\other\r\n"));

        const auto again = readAs(file, QStringLiteral("UTF-8"));
        QVERIFY(again.has_value());
        QCOMPARE(again->tracks[0].voiceDir, QStringLiteral("%VOICE%uta\\..\\other"));
    }

    // UST joins the names of the regions that start or end at a note with |, which cannot be part
    // of a name. Empty names are dropped.
    void region_names_are_split_and_joined_at_the_bar() {
        const auto read = [](const QByteArray &region, const QByteArray &regionEnd) {
            TempUst file("regions");
            file.writeBytes("[#VERSION]\r\nUST Version1.2\r\nCharset=UTF-8\r\n[#SETTING]\r\n"
                            "Tempo=120.00\r\nTracks=1\r\nMode2=True\r\n[#0000]\r\nLength=480\r\n"
                            "Lyric=a\r\nNoteNum=60\r\n$region=" +
                            region + "\r\n$region_end=" + regionEnd + "\r\n[#TRACKEND]\r\n");
            const auto project = readAs(file, QStringLiteral("UTF-8"));
            return project ? project->tracks[0].notes[0] : Note();
        };
        const auto exported = [](const Note &note) {
            TempUst file("regions_out");
            auto project = oneNote();
            project.tracks[0].notes[0] = note;
            DiagnosticList diagnostics;
            writeTo(project, file, {}, diagnostics);
            return file.readBytes();
        };
        const auto a = QStringLiteral("A");
        const auto b = QStringLiteral("B");

        const auto both = read("A|B", "A|B");
        QCOMPARE(both.regions, (QStringList{a, b}));
        QCOMPARE(both.regionEnds, (QStringList{a, b}));
        const auto written = exported(both);
        QVERIFY(written.contains("\r\n$region=A|B\r\n"));
        QVERIFY(written.contains("\r\n$region_end=A|B\r\n"));

        const auto doubled = read("A||B", "|A");
        QCOMPARE(doubled.regions, (QStringList{a, b}));
        QCOMPARE(doubled.regionEnds, (QStringList{a}));
        QVERIFY(exported(doubled).contains("\r\n$region=A|B\r\n"));
        QVERIFY(exported(doubled).contains("\r\n$region_end=A\r\n"));

        const auto trailing = read("A|", "B");
        QCOMPARE(trailing.regions, (QStringList{a}));
        QVERIFY(exported(trailing).contains("\r\n$region=A\r\n"));
    }

    // UST has no time signature, so the control note carries it.
    void the_time_signature_travels_in_the_control_note() {
        TempUst file("timesignature");
        auto project = oneNote();
        project.settings.timeSignature = {3, 4};
        DiagnosticList diagnostics;
        QVERIFY(writeTo(project, file, {}, diagnostics));
        const auto again = readAs(file, QStringLiteral("UTF-8"));
        QVERIFY(again.has_value());
        QCOMPARE(again->settings.timeSignature, (TimeSignature{3, 4}));
    }

    void a_control_note_without_a_valid_time_signature_gives_four_four() {
        const auto readWithPayload = [](const QByteArray &payload, DiagnosticList &diagnostics) {
            TempUst file("payload");
            file.writeBytes(
                "[#VERSION]\r\nUST Version1.2\r\nCharset=UTF-8\r\n[#SETTING]\r\n"
                "Tempo=120.00\r\nTracks=1\r\nMode2=True\r\n[#0000]\r\nLength=480\r\n"
                "Lyric=_USTH_\r\nNoteNum=60\r\n$usth=" +
                PayloadCodec::encode(payload) +
                "\r\n[#0001]\r\nLength=480\r\nLyric=a\r\nNoteNum=60\r\n[#TRACKEND]\r\n");
            return readAs(file, QStringLiteral("UTF-8"), &diagnostics);
        };

        DiagnosticList diagnostics;
        const auto absent = readWithPayload(R"({"version":1,"ustCharset":"UTF-8"})", diagnostics);
        QVERIFY(absent.has_value());
        QCOMPARE(absent->tracks[0].notes.size(), 1);
        QCOMPARE(absent->settings.timeSignature, (TimeSignature{4, 4}));
        QVERIFY(diagnostics.isEmpty());

        const auto invalid = readWithPayload(
            R"({"version":1,"ustCharset":"UTF-8","timeSignature":{"numerator":3,"denominator":5}})",
            diagnostics);
        QVERIFY(invalid.has_value());
        QCOMPARE(invalid->settings.timeSignature, (TimeSignature{4, 4}));
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(diagnostics.at(0).severity, DiagnosticSeverity::Warning);
    }
};

QTEST_APPLESS_MAIN(test_UstDocument)

#include "test_UstDocument.moc"
