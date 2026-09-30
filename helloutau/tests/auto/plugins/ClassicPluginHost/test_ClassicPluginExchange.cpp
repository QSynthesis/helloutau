#include <filesystem>
#include <string>
#include <string_view>

#include <QtTest/QTest>

#include <stdutau/pluginfile.h>

#include <hellokit/Document/Project.h>
#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Edit/ProjectSession.h>

#include <ClassicPluginHost/ClassicPlugin.h>
#include <ClassicPluginHost/ClassicPluginExchange.h>

using namespace hello;
using namespace hello::daw;

namespace fs = std::filesystem;

namespace {

    kit::Note noteOf(const QString &lyric, int length, int noteNum) {
        kit::Note note;
        note.lyric = lyric;
        note.length = length;
        note.noteNum = noteNum;
        return note;
    }

    // The project of the probe in docs/claude/utau-plugin-protocol.md: R a i R u e o, with
    // pitch, vibrato, flags, velocity, a label and a user entry
    kit::Project probe() {
        auto a = noteOf(QStringLiteral("a"), 480, 60);
        a.intensity = 100;
        a.modulation = 0;
        a.portamento = {
            {-40, 0, kit::PortamentoPoint::S},
            {40,  0, kit::PortamentoPoint::S}
        };
        auto i = noteOf(QStringLiteral("i"), 480, 62);
        i.intensity = 100;
        i.modulation = 0;
        i.vibrato = kit::Vibrato();
        auto u = noteOf(QStringLiteral("u"), 480, 64);
        u.intensity = 100;
        u.modulation = 0;
        u.flags = QStringLiteral("g-5");
        u.velocity = 150;
        auto e = noteOf(QStringLiteral("e"), 480, 65);
        e.intensity = 100;
        e.modulation = 0;
        e.label = QStringLiteral("L5");
        e.userData.insert(QStringLiteral("$probe"), QStringLiteral("x"));

        kit::Track track;
        track.notes = {noteOf(QStringLiteral("R"), 480, 60), a, i,
                       noteOf(QStringLiteral("R"), 240, 60), u, e,
                       noteOf(QStringLiteral("o"), 960, 67)};
        kit::Project project;
        project.settings.mode2 = true;
        project.tracks = {track};
        return project;
    }

    ClassicPlugin pluginOf(const QString &charset) {
        ClassicPlugin plugin;
        plugin.name = QStringLiteral("Probe");
        plugin.charset = charset;
        return plugin;
    }

    QStringList lyricsOf(const kit::Project &project) {
        QStringList lyrics;
        for (const auto &note : project.tracks.first().notes) {
            lyrics.push_back(note.lyric);
        }
        return lyrics;
    }

    utau::PluginInput read(const QByteArray &bytes) {
        utau::PluginInput file;
        file.read(std::string_view(bytes.data(), size_t(bytes.size())));
        return file;
    }

}

class test_ClassicPluginExchange : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The selection numbered by its position, with the notes around it, the settings with
    // their absolute paths, and the computed values of the synthesis, zero without a voice bank
    void the_input_is_written_as_utau_writes_it() {
        ClassicPluginExchange::Paths paths;
        paths.project = fs::path(u"/songs/song.ust");
        paths.voiceDirectory = fs::path(u"/voice/bank");
        paths.cacheDirectory = fs::path(u"/songs/song.cache");
        const auto bytes = ClassicPluginExchange::input(pluginOf(QStringLiteral("UTF-8")), probe(),
                                                        2, 3, paths, nullptr);
        QVERIFY(bytes.startsWith("[#VERSION]\r\nUST Version 1.20\r\n[#SETTING]\r\n"));

        const auto file = read(bytes);
        QCOMPARE(QString::fromStdString(file.settings.project),
                 QString::fromStdU16String(paths.project.u16string()));
        QCOMPARE(QString::fromStdString(file.settings.voiceDir),
                 QString::fromStdU16String(paths.voiceDirectory.u16string()));
        QVERIFY(file.settings.isMode2);
        QCOMPARE(file.startIndex, 2);
        QVERIFY(bytes.contains("[#0002]\r\n"));
        QCOMPARE(file.notes.size(), size_t(3));
        QCOMPARE(file.notes[0].lyric, std::string("i"));
        QVERIFY(file.notes[0].vibrato);
        QCOMPARE(file.notes[2].flags, std::string("g-5"));
        QCOMPARE(file.notes[1].preUttrRO.value_or(-1), 0.0);
        QVERIFY(file.notes[0].filenameRO.empty());
        QVERIFY(file.prevNote);
        QCOMPARE(file.prevNote->lyric, std::string("a"));
        QVERIFY(file.nextNote);
        QCOMPARE(file.nextNote->lyric, std::string("e"));
    }

    // A plugin that receives the whole track gets every note, and no note around them.
    void the_whole_track_has_no_notes_around_it() {
        auto plugin = pluginOf(QStringLiteral("UTF-8"));
        plugin.wholeTrack = true;
        const auto file = read(ClassicPluginExchange::input(plugin, probe(), 2, 3, {}, nullptr));
        QCOMPARE(file.startIndex, 0);
        QCOMPARE(file.notes.size(), size_t(7));
        QVERIFY(!file.prevNote);
        QVERIFY(!file.nextNote);
    }

    // The result of the probe R2 gives the track that UTAU saved: the notes around the
    // selection edited, an insertion with the length and note number of the next note, a note
    // unchanged as a bare header, a deletion, an empty entry removed and an omitted one kept.
    // It is one undo step.
    void the_result_applies_as_utau_applies_it() {
        kit::ProjectSession session(probe());
        const auto notes = kit::ProjectRef(&session).tracks().at(0).notes();
        kit::DiagnosticList diagnostics;
        const auto outcome = ClassicPluginExchange::apply(
            pluginOf(QStringLiteral("UTF-8")), notes, 2, 3,
            "[#PREV]\r\nLyric=P\r\n[#INSERT]\r\nLyric=N\r\n[#0002]\r\n[#DELETE]\r\n"
            "[#0004]\r\nVelocity=\r\nLength=240\r\n[#NEXT]\r\nLyric=Q\r\n",
            diagnostics);
        QCOMPARE(outcome, ClassicPluginExchange::Applied);
        QVERIFY(diagnostics.isEmpty());

        const auto project = session.snapshot();
        QCOMPARE(lyricsOf(project), QStringList({"R", "P", "N", "i", "u", "Q", "o"}));
        const auto &result = project.tracks.first().notes;
        QCOMPARE(result[1].portamento.size(), 2);
        QCOMPARE(result[2].length, 480);
        QCOMPARE(result[2].noteNum, 62);
        QVERIFY(!result[2].intensity);
        QVERIFY(result[3].vibrato);
        QCOMPARE(result[4].length, 240);
        QVERIFY(!result[4].velocity);
        QCOMPARE(result[4].flags, QStringLiteral("g-5"));
        QCOMPARE(result[5].label, QStringLiteral("L5"));
        QCOMPARE(result[5].userData.value(QStringLiteral("$probe")), QStringLiteral("x"));

        QVERIFY(session.canUndo());
        session.undo();
        QCOMPARE(lyricsOf(session.snapshot()), lyricsOf(probe()));
        QVERIFY(!session.canUndo());
    }

    // The probe R1: the sections apply in their order, whatever their numbers.
    void the_numbers_do_not_matter() {
        kit::ProjectSession session(probe());
        const auto notes = kit::ProjectRef(&session).tracks().at(0).notes();
        kit::DiagnosticList diagnostics;
        ClassicPluginExchange::apply(pluginOf(QStringLiteral("UTF-8")), notes, 2, 3,
                                     "[#0004]\r\nLyric=X\r\n[#0003]\r\n[#0002]\r\nLyric=Z\r\n",
                                     diagnostics);
        QCOMPARE(lyricsOf(session.snapshot()), QStringList({"R", "a", "X", "R", "Z", "e", "o"}));
    }

    // The probe R3: a file without a note cancels, and makes no undo step.
    void an_empty_result_cancels() {
        kit::ProjectSession session(probe());
        const auto notes = kit::ProjectRef(&session).tracks().at(0).notes();
        kit::DiagnosticList diagnostics;
        QCOMPARE(ClassicPluginExchange::apply(pluginOf(QStringLiteral("UTF-8")), notes, 2, 3, "",
                                              diagnostics),
                 ClassicPluginExchange::Cancelled);
        QVERIFY(!session.canUndo());
    }

    // A section past the selection is ignored with a warning, rather than applied to the note
    // after it.
    void sections_beyond_the_selection_are_ignored() {
        kit::ProjectSession session(probe());
        const auto notes = kit::ProjectRef(&session).tracks().at(0).notes();
        kit::DiagnosticList diagnostics;
        ClassicPluginExchange::apply(
            pluginOf(QStringLiteral("UTF-8")), notes, 2, 3,
            "[#0002]\r\n[#0003]\r\n[#0004]\r\n[#0005]\r\nLyric=W\r\n[#DELETE]\r\n", diagnostics);
        QCOMPARE(diagnostics.size(), 2);
        QCOMPARE(lyricsOf(session.snapshot()), lyricsOf(probe()));
    }

    // An empty entry removes the property, a user entry included.
    void empty_entries_remove() {
        kit::ProjectSession session(probe());
        const auto notes = kit::ProjectRef(&session).tracks().at(0).notes();
        kit::DiagnosticList diagnostics;
        ClassicPluginExchange::apply(pluginOf(QStringLiteral("UTF-8")), notes, 5, 1,
                                     "[#0005]\r\n$probe=\r\nLabel=\r\nIntensity=\r\n", diagnostics);
        const auto e = session.snapshot().tracks.first().notes[5];
        QVERIFY(e.userData.isEmpty());
        QVERIFY(e.label.isEmpty());
        QVERIFY(!e.intensity);
        QCOMPARE(e.modulation.value_or(-1), 0.0);
    }

    // At the end of the track, an insertion has no note after it and takes the length, the
    // note number and the lyric of a new note where it gives none.
    void an_insertion_at_the_end() {
        kit::ProjectSession session(probe());
        const auto notes = kit::ProjectRef(&session).tracks().at(0).notes();
        kit::DiagnosticList diagnostics;
        ClassicPluginExchange::apply(pluginOf(QStringLiteral("UTF-8")), notes, 6, 1,
                                     "[#0006]\r\n[#INSERT]\r\nNoteNum=70\r\n", diagnostics);
        const auto result = session.snapshot().tracks.first().notes;
        QCOMPARE(result.size(), 8);
        QCOMPARE(result[7].noteNum, 70);
        QCOMPARE(result[7].length, 480);
        QCOMPARE(result[7].lyric, QStringLiteral("a"));
    }

    // In an encoding other than UTF-8, what it cannot represent is escaped in the input, and
    // the escapes of the result are read back.
    void text_is_escaped_outside_utf8() {
        auto project = probe();
        project.tracks.first().notes[2].lyric = QString(QChar(0x4f60));
        const auto plugin = pluginOf(QStringLiteral("Shift_JIS"));
        const auto bytes = ClassicPluginExchange::input(plugin, project, 2, 1, {}, nullptr);
        QVERIFY(bytes.contains("Lyric=\\"
                               "u4f60\r\n"));

        kit::ProjectSession session(project);
        const auto notes = kit::ProjectRef(&session).tracks().at(0).notes();
        kit::DiagnosticList diagnostics;
        ClassicPluginExchange::apply(plugin, notes, 2, 1,
                                     "[#0002]\r\nLyric=\\"
                                     "u597d\\\\\r\n",
                                     diagnostics);
        QCOMPARE(session.snapshot().tracks.first().notes[2].lyric,
                 QString(QChar(0x597d)) + QLatin1Char('\\'));
    }
};

QTEST_GUILESS_MAIN(test_ClassicPluginExchange)

#include "test_ClassicPluginExchange.moc"
