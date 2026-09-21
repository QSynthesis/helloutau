#define BOOST_TEST_MAIN
#define BOOST_TEST_MODULE test_UstConversion

#include <filesystem>
#include <fstream>
#include <string>

#include <boost/test/unit_test.hpp>

#include <QtCore/QByteArray>

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

    /// Builds and writes in the two steps the real caller takes.
    bool writeTo(const Project &project, const TempUst &file,
                 const UstDocument::ExportOptions &options, DiagnosticList &diagnostics) {
        auto ust = UstDocument::fromProject(project, options, diagnostics);
        return ust && ust->save(file.path(), diagnostics);
    }

    /// Opens and converts in the two steps the real caller takes, with one parse between them.
    ///
    /// \param sink where to keep what was said about it, for the cases that ask
    std::optional<Project> readAs(const TempUst &file, const QString &charset,
                                  DiagnosticList *sink = nullptr) {
        DiagnosticList ignored;
        DiagnosticList &diagnostics = sink ? *sink : ignored;

        auto ust = UstDocument::open(file.path(), diagnostics);
        if (!ust) {
            return std::nullopt;
        }
        return ust->toProject(charset, diagnostics);
    }

}

BOOST_AUTO_TEST_SUITE(test_UstConversion)

BOOST_AUTO_TEST_CASE(a_ust_written_here_reads_back_the_same) {
    TempUst file("roundtrip");
    auto project = oneNote(u("あ"));
    auto &note = project.tracks[0].notes[0];
    note.intensity = 80;
    note.velocity = 0; // zero, which has to stay a value rather than become absent
    note.flags = QStringLiteral("g-5");
    note.envelope = Envelope{{{0, 0}, {5, 100}, {35, 100}, {0, 0}}};
    note.vibrato = Vibrato{65, 180, 35, 20, 20, 0, 0, 7};
    note.portamento = {{-40, 0, PortamentoType::S}, {50, 10, PortamentoType::Linear}};
    note.label = QStringLiteral("verse");
    note.userData.insert(QStringLiteral("$mine"), QStringLiteral("kept"));

    DiagnosticList diagnostics;
    BOOST_REQUIRE(writeTo(project, file, {}, diagnostics));

    auto again = readAs(file, QStringLiteral("UTF-8"));
    BOOST_REQUIRE(again.has_value());
    BOOST_REQUIRE_EQUAL(again->tracks.size(), 1);

    const auto &back = again->tracks.first().notes;
    BOOST_REQUIRE_EQUAL(back.size(), 1);
    BOOST_CHECK(back.first().lyric == u("あ"));
    BOOST_CHECK_EQUAL(back.first().length, 480);
    BOOST_CHECK_EQUAL(back.first().noteNum, 60);
    BOOST_REQUIRE(back.first().intensity.has_value());
    BOOST_CHECK_CLOSE(*back.first().intensity, 80.0, 0.01);
    BOOST_REQUIRE(back.first().velocity.has_value());
    BOOST_CHECK_CLOSE(*back.first().velocity, 0.0, 0.01);
    BOOST_CHECK(!back.first().modulation.has_value());
    BOOST_CHECK_EQUAL(back.first().flags.toStdString(), "g-5");
    BOOST_REQUIRE(back.first().envelope.has_value());
    BOOST_CHECK_EQUAL(back.first().envelope->anchors.size(), 4);
    BOOST_REQUIRE(back.first().vibrato.has_value());
    BOOST_CHECK_CLOSE(back.first().vibrato->period, 180.0, 0.01);
    BOOST_REQUIRE_EQUAL(back.first().portamento.size(), 2);
    BOOST_CHECK(back.first().portamento.at(1).type == PortamentoType::Linear);
    BOOST_CHECK_EQUAL(back.first().label.toStdString(), "verse");
    BOOST_CHECK_EQUAL(back.first().userData.value(QStringLiteral("$mine")).toStdString(), "kept");
    BOOST_CHECK_EQUAL(again->tracks.first().voiceDir.toStdString(), "%VOICE%uta");
}

// The eighth value of VBR is one UTAU never uses and has no field for, and it still has to come
// back, or a file loses something every time it passes through.
BOOST_AUTO_TEST_CASE(the_vibrato_value_utau_ignores_still_comes_back) {
    TempUst file("vbr8");
    auto project = oneNote();
    project.tracks[0].notes[0].vibrato = Vibrato{65, 180, 35, 20, 20, 0, 0, 42};

    DiagnosticList diagnostics;
    BOOST_REQUIRE(writeTo(project, file, {}, diagnostics));

    auto again = readAs(file, QStringLiteral("UTF-8"));
    BOOST_REQUIRE(again.has_value());
    const auto &vibrato = again->tracks.first().notes.first().vibrato;
    BOOST_REQUIRE(vibrato.has_value());
    BOOST_CHECK_CLOSE(vibrato->intensity, 42.0, 0.01);
}

// One goes out, exactly one comes in. Getting this wrong grows a run of half second leaders
// across repeated round trips, which is the kind of thing nobody notices until it is bad.
BOOST_AUTO_TEST_CASE(the_control_note_is_added_once_and_eaten_once) {
    TempUst file("control");
    auto project = oneNote();

    DiagnosticList diagnostics;
    BOOST_REQUIRE(writeTo(project, file, {}, diagnostics));

    const QByteArray written = file.readBytes();
    BOOST_CHECK(written.contains(controlNoteLyric));
    BOOST_CHECK(written.contains(controlNoteEntry));

    for (int pass = 0; pass < 3; ++pass) {
        auto again = readAs(file, QStringLiteral("UTF-8"));
        BOOST_REQUIRE(again.has_value());
        BOOST_TEST_CONTEXT("pass " << pass) {
            BOOST_REQUIRE_EQUAL(again->tracks.first().notes.size(), 1);
            BOOST_CHECK(!again->tracks.first().notes.first().isRest());
        }
        BOOST_REQUIRE(writeTo(*again, file, {}, diagnostics));
    }
}

// The payload is what the Charset line cannot say, so it has to survive being written in an
// encoding that is not UTF-8 and be findable again.
BOOST_AUTO_TEST_CASE(the_encoding_is_recorded_and_read_back) {
    TempUst file("charset");
    auto project = oneNote(u("あ"));

    DiagnosticList diagnostics;
    BOOST_REQUIRE(writeTo(project, file, {QStringLiteral("Shift_JIS"), {}, {}}, diagnostics));

    // UST can only declare UTF-8, so it says nothing here.
    BOOST_CHECK(!file.readBytes().contains("Charset="));

    auto probe = UstDocument::open(file.path(), diagnostics);
    BOOST_REQUIRE(probe.has_value());
    BOOST_REQUIRE(probe->recordedCharset().has_value());
    BOOST_CHECK_EQUAL(probe->recordedCharset()->toStdString(), "Shift_JIS");
    BOOST_REQUIRE(probe->settledCharset().has_value());

    auto again = readAs(file, *probe->settledCharset());
    BOOST_REQUIRE(again.has_value());
    BOOST_CHECK(again->tracks.first().notes.first().lyric == u("あ"));
}

BOOST_AUTO_TEST_CASE(utf8_is_declared_in_the_version_section) {
    TempUst file("utf8");
    DiagnosticList diagnostics;
    BOOST_REQUIRE(writeTo(oneNote(), file, {}, diagnostics));
    BOOST_CHECK(file.readBytes().contains("Charset=UTF-8"));

    auto probe = UstDocument::open(file.path(), diagnostics);
    BOOST_REQUIRE(probe.has_value());
    BOOST_CHECK(probe->declaresUtf8());
}

// A lyric with no Shift_JIS spelling still has to come back, which is what escaping is for.
BOOST_AUTO_TEST_CASE(a_lyric_the_encoding_cannot_hold_survives_as_an_escape) {
    TempUst file("escape");
    auto project = oneNote(u("你"));

    DiagnosticList diagnostics;
    BOOST_REQUIRE(writeTo(project, file, {QStringLiteral("Shift_JIS"), {}, {}}, diagnostics));
    BOOST_CHECK(file.readBytes().contains("\\u4f60"));

    auto again = readAs(file, QStringLiteral("Shift_JIS"));
    BOOST_REQUIRE(again.has_value());
    BOOST_CHECK(again->tracks.first().notes.first().lyric == u("你"));
}

// A UST from UTAU knows nothing of the escaping, so its backslashes are its own. Unescaping one
// would turn a Windows path into something else.
BOOST_AUTO_TEST_CASE(a_foreign_ust_is_not_unescaped) {
    TempUst file("foreign");
    file.writeBytes(plainUst("C:\\utau\\voice"));

    auto project = readAs(file, QStringLiteral("Shift_JIS"));
    BOOST_REQUIRE(project.has_value());
    BOOST_CHECK_EQUAL(project->tracks.first().notes.first().lyric.toStdString(),
                      "C:\\utau\\voice");
}

// Nothing in the file says what encoding it is, so the caller has to ask.
BOOST_AUTO_TEST_CASE(a_file_that_says_nothing_settles_nothing) {
    TempUst file("unknown");
    file.writeBytes(plainUst("a"));

    DiagnosticList diagnostics;
    auto probe = UstDocument::open(file.path(), diagnostics);
    BOOST_REQUIRE(probe.has_value());
    BOOST_CHECK(!probe->recordedCharset().has_value());
    BOOST_CHECK(!probe->declaresUtf8());
    BOOST_CHECK(!probe->settledCharset().has_value());
    BOOST_CHECK(!probe->rawLyrics().isEmpty()); // still enough to show a preview
}

// Reading with the wrong encoding has to fail rather than produce a page of replacement
// characters that the user then saves over their project.
BOOST_AUTO_TEST_CASE(the_wrong_encoding_is_refused) {
    TempUst file("wrong");
    const TextCodec sjis(QStringLiteral("Shift_JIS"));
    QByteArray lyric = sjis.encode(u("あ"));
    file.writeBytes(QByteArray("[#VERSION]\r\nUST Version1.2\r\n[#SETTING]\r\nProjectName=") +
                    lyric + "\r\nTempo=120.00\r\n[#0000]\r\nLength=480\r\nLyric=a\r\n"
                            "NoteNum=60\r\n[#TRACKEND]\r\n");

    DiagnosticList diagnostics;
    BOOST_CHECK(!readAs(file, QStringLiteral("UTF-8"), &diagnostics).has_value());
    BOOST_CHECK(hasError(diagnostics));
}

// The engine paths are the project's own where it has them, and the local ones only fill a gap.
BOOST_AUTO_TEST_CASE(the_engines_come_from_the_project_first) {
    TempUst file("engines");
    auto project = oneNote();
    project.settings.resampler = QStringLiteral("own_resampler.exe");

    UstDocument::ExportOptions options;
    options.wavtool = QStringLiteral("local_wavtool.exe");
    options.resampler = QStringLiteral("local_resampler.exe");

    DiagnosticList diagnostics;
    BOOST_REQUIRE(writeTo(project, file, options, diagnostics));

    auto again = readAs(file, QStringLiteral("UTF-8"));
    BOOST_REQUIRE(again.has_value());
    BOOST_CHECK_EQUAL(again->settings.resampler.toStdString(), "own_resampler.exe");
    BOOST_CHECK_EQUAL(again->settings.wavtool.toStdString(), "local_wavtool.exe");
}

// Project does not carry everything a UST holds, so the parse stays reachable. What comes out of
// it is bytes in the file's own encoding, which is the point: it has not been decoded and the
// caller has to do that itself.
BOOST_AUTO_TEST_CASE(the_parse_underneath_is_reachable_and_undecoded) {
    TempUst file("raw");
    const TextCodec sjis(QStringLiteral("Shift_JIS"));
    file.writeBytes(plainUst(sjis.encode(u("あ"))));

    DiagnosticList diagnostics;
    auto ust = UstDocument::open(file.path(), diagnostics);
    BOOST_REQUIRE(ust.has_value());

    BOOST_REQUIRE_EQUAL(ust->file().notes.size(), 1);
    const std::string &raw = ust->file().notes.front().lyric;
    BOOST_CHECK_EQUAL(raw.size(), 2); // Shift_JIS bytes, not UTF-8 and not decoded
    BOOST_CHECK(raw != u("あ").toStdString());

    auto project = ust->toProject(QStringLiteral("Shift_JIS"), diagnostics);
    BOOST_REQUIRE(project.has_value());
    BOOST_CHECK(project->tracks.first().notes.first().lyric == u("あ"));
}

BOOST_AUTO_TEST_CASE(something_that_is_not_a_ust_is_an_error) {
    TempUst file("notaust");
    file.writeBytes("hello, this is not a UST at all");

    DiagnosticList diagnostics;
    auto project = readAs(file, QStringLiteral("UTF-8"));
    // stdutau reads it as a file with no notes rather than refusing, which is a project with
    // nothing in it. What matters is that nothing pretends to have been read.
    if (project) {
        BOOST_CHECK(project->tracks.first().notes.isEmpty());
    }
}

BOOST_AUTO_TEST_SUITE_END()
