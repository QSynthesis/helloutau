#include <filesystem>
#include <fstream>
#include <string>

#include <QtCore/QByteArray>
#include <QtTest/QTest>

#include <hellokit/Document/DocumentConstants.h>
#include <hellokit/Document/UstDocument.h>
#include <hellokit/Support/TextCodec.h>

using namespace hello::kit;
namespace fs = std::filesystem;

namespace {

    QString u(const char *utf8Text) {
        return QString::fromUtf8(utf8Text);
    }

    // A file of its own per case, removed afterwards, since what is being tested is what lands
    // on disk and what comes back off it.
    class TempUst {
    public:
        explicit TempUst(const std::string &name) {
            _path = fs::temp_directory_path() / ("hellokit_" + name + ".ust");
        }

        ~TempUst() {
            std::error_code ignored;
            fs::remove(_path, ignored);
        }

        void writeBytes(const QByteArray &bytes) const {
            std::ofstream out(_path, std::ios::binary | std::ios::trunc);
            out.write(bytes.constData(), bytes.size());
        }

        QByteArray readBytes() const {
            std::ifstream in(_path, std::ios::binary);
            const std::string all((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
            return QByteArray(all.data(), qsizetype(all.size()));
        }

        const fs::path &path() const {
            return _path;
        }

    private:
        fs::path _path;
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

    // A UST as UTAU writes one, with no control note and nothing said about the encoding.
    QByteArray plainUst(const QByteArray &lyricBytes) {
        return QByteArray("[#VERSION]\r\nUST Version1.2\r\n[#SETTING]\r\nTempo=120.00\r\n"
                          "Tracks=1\r\nMode2=True\r\n[#0000]\r\nLength=480\r\nLyric=") +
               lyricBytes + "\r\nNoteNum=60\r\n[#TRACKEND]\r\n";
    }

    // Builds and writes in the two steps the real caller takes.
    bool writeTo(const Project &project, const TempUst &file,
                 const UstDocument::ExportOptions &options, DiagnosticList &diagnostics) {
        const auto ust = UstDocument::fromProject(project, options, diagnostics);
        return ust && ust->save(file.path(), diagnostics);
    }

    // Opens and converts in the two steps the real caller takes, with one parse between them.
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
        note.velocity = 0; // zero, which has to stay a value rather than become absent
        note.flags = QStringLiteral("g-5");
        note.envelope = Envelope{
            {{0, 0}, {5, 100}, {35, 100}, {0, 0}}
        };
        note.vibrato = Vibrato{65, 180, 35, 20, 20, 0, 0, 7};
        note.portamento = {
            {-40, 0,  PortamentoType::S     },
            {50,  10, PortamentoType::Linear}
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
        QCOMPARE(back.first().envelope->anchors.size(), 4);
        QVERIFY(back.first().vibrato.has_value());
        QCOMPARE(back.first().vibrato->period, 180.0);
        QCOMPARE(back.first().portamento.size(), 2);
        QVERIFY(back.first().portamento.at(1).type == PortamentoType::Linear);
        QCOMPARE(back.first().label, QStringLiteral("verse"));
        QCOMPARE(back.first().userData.value(QStringLiteral("$mine")), QStringLiteral("kept"));
        QCOMPARE(again->tracks.first().voiceDir, QStringLiteral("%VOICE%uta"));
    }

    // The eighth value of VBR is one UTAU never uses and has no field for, and it still has to
    // come back, or a file loses something every time it passes through.
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

    // One goes out, exactly one comes in. Getting this wrong grows a run of half second leaders
    // across repeated round trips, which is the kind of thing nobody notices until it is bad.
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

    // The payload is what the Charset line cannot say, so it has to survive being written in an
    // encoding that is not UTF-8 and be findable again.
    void the_encoding_is_recorded_and_read_back() {
        TempUst file("charset");
        const auto project = oneNote(u("あ"));

        DiagnosticList diagnostics;
        QVERIFY(writeTo(project, file, {QStringLiteral("Shift_JIS"), {}, {}}, diagnostics));

        // UST can only declare UTF-8, so it says nothing here.
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

    // A lyric with no Shift_JIS spelling still has to come back, which is what escaping is for.
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

    // A UST from UTAU knows nothing of the escaping, so its backslashes are its own. Unescaping
    // one would turn a Windows path into something else.
    void a_foreign_ust_is_not_unescaped() {
        TempUst file("foreign");
        file.writeBytes(plainUst("C:\\utau\\voice"));

        const auto project = readAs(file, QStringLiteral("Shift_JIS"));
        QVERIFY(project.has_value());
        QCOMPARE(project->tracks.first().notes.first().lyric, QStringLiteral("C:\\utau\\voice"));
    }

    // Nothing in the file says what encoding it is, so the caller has to ask.
    void a_file_that_says_nothing_settles_nothing() {
        TempUst file("unknown");
        file.writeBytes(plainUst("a"));

        DiagnosticList diagnostics;
        const auto ust = UstDocument::open(file.path(), diagnostics);
        QVERIFY(ust.has_value());
        QVERIFY(!ust->recordedCharset().has_value());
        QVERIFY(!ust->declaresUtf8());
        QVERIFY(!ust->settledCharset().has_value());
        QVERIFY(!ust->rawLyrics().isEmpty()); // still enough to show a preview
    }

    // Reading with the wrong encoding has to fail rather than produce a page of replacement
    // characters that the user then saves over their project.
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

    // The engine paths are the project's own where it has them, and the local ones only fill a
    // gap.
    void the_engines_come_from_the_project_first() {
        TempUst file("engines");
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

    // Project does not carry everything a UST holds, so the parse stays reachable. What comes
    // out of it is bytes in the file's own encoding, which is the point.
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

        // stdutau reads it as a file with no notes rather than refusing, which is a project with
        // nothing in it. What matters is that nothing pretends to have been read.
        const auto project = readAs(file, QStringLiteral("UTF-8"));
        if (project) {
            QVERIFY(project->tracks.first().notes.isEmpty());
        }
    }
};

QTEST_APPLESS_MAIN(test_UstDocument)

#include "test_UstDocument.moc"
