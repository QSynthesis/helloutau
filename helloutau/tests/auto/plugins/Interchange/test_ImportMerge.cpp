#include <QtTest/QTest>

#include <hellokit/Document/Project.h>
#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Edit/ProjectSession.h>

#include <Interchange/ImportMerge.h>

using namespace hello;
using namespace hello::daw;

namespace {

    kit::Note note(const char *lyric, std::optional<double> tempo = std::nullopt) {
        kit::Note note;
        note.lyric = QString::fromUtf8(lyric);
        note.length = 480;
        note.noteNum = 60;
        note.tempo = tempo;
        return note;
    }

    kit::Project projectOf(const QList<kit::Note> &notes, double tempo = 120) {
        kit::Project project;
        project.settings.tempo = tempo;
        kit::Track track;
        track.notes = notes;
        project.tracks.push_back(track);
        return project;
    }

    QList<kit::Note> notesOf(const kit::ProjectSession &session) {
        return session.snapshot().tracks.first().notes;
    }

    QStringList lyricsOf(const kit::ProjectSession &session) {
        QStringList lyrics;
        for (const auto &note : notesOf(session)) {
            lyrics.push_back(note.lyric);
        }
        return lyrics;
    }

    ImportMerge::Options at(ImportMerge::Position position, int first = 0, int count = 0) {
        ImportMerge::Options options;
        options.position = position;
        options.first = first;
        options.count = count;
        return options;
    }

    bool has(const kit::DiagnosticList &diagnostics, kit::DiagnosticSeverity severity) {
        return std::any_of(diagnostics.begin(), diagnostics.end(),
                           [severity](const auto &d) { return d.severity == severity; });
    }

}

class test_ImportMerge : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Each position inserts the notes at the expected index, in one undo step.
    void the_notes_go_where_the_options_say() {
        const auto imported = projectOf({note("x"), note("y")});
        const auto existing = projectOf({note("a"), note("b"), note("c"), note("d")});
        const struct {
            ImportMerge::Position position;
            QStringList lyrics;
            int first;
        } cases[] = {
            {ImportMerge::After,   {"a", "b", "c", "x", "y", "d"}, 3},
            {ImportMerge::Before,  {"a", "x", "y", "b", "c", "d"}, 1},
            {ImportMerge::Replace, {"a", "x", "y", "d"},           1},
            {ImportMerge::AtEnd,   {"a", "b", "c", "d", "x", "y"}, 4},
        };
        for (const auto &c : cases) {
            kit::ProjectSession session(existing);
            kit::DiagnosticList diagnostics;
            const auto range = ImportMerge::merge(imported, kit::ProjectRef(&session),
                                                  at(c.position, 1, 2), diagnostics);
            QVERIFY(range);
            QCOMPARE(range->first, c.first);
            QCOMPARE(range->count, 2);
            QCOMPARE(lyricsOf(session), c.lyrics);
            QVERIFY(!has(diagnostics, kit::DiagnosticSeverity::Warning));

            session.undo();
            QCOMPARE(lyricsOf(session), QStringList({"a", "b", "c", "d"}));
        }
    }

    // Leading rests are removed unless keepLeadingRest is set. A file of rests only results in an
    // empty range, a warning and no undo step.
    void the_leading_rests_go_unless_kept() {
        const auto imported = projectOf({note("R"), note("r"), note("x"), note("R")});
        {
            kit::ProjectSession session(projectOf({}));
            kit::DiagnosticList diagnostics;
            ImportMerge::merge(imported, kit::ProjectRef(&session), at(ImportMerge::AtEnd),
                               diagnostics);
            QCOMPARE(lyricsOf(session), QStringList({"x", "R"}));
        }
        {
            kit::ProjectSession session(projectOf({}));
            kit::DiagnosticList diagnostics;
            auto options = at(ImportMerge::AtEnd);
            options.keepLeadingRest = true;
            ImportMerge::merge(imported, kit::ProjectRef(&session), options, diagnostics);
            QCOMPARE(lyricsOf(session), QStringList({"R", "r", "x", "R"}));
        }
        {
            kit::ProjectSession session(projectOf({note("a")}));
            kit::DiagnosticList diagnostics;
            const auto range = ImportMerge::merge(projectOf({note("R")}), kit::ProjectRef(&session),
                                                  at(ImportMerge::AtEnd), diagnostics);
            QVERIFY(range);
            QCOMPARE(range->count, 0);
            QVERIFY(has(diagnostics, kit::DiagnosticSeverity::Warning));
            QVERIFY(!session.canUndo());
        }
    }

    // With keepTempo, the first inserted note records the imported tempo if it differs from the
    // tempo at the insertion position. The following note retains its tempo in either case.
    void the_tempo_is_kept_or_taken() {
        const auto imported = projectOf({note("x"), note("y", 90)}, 150);
        {
            kit::ProjectSession session(projectOf({note("a"), note("b")}));
            kit::DiagnosticList diagnostics;
            ImportMerge::merge(imported, kit::ProjectRef(&session), at(ImportMerge::Before, 1, 1),
                               diagnostics);
            const auto notes = notesOf(session);
            QCOMPARE(notes[1].tempo, 150.0);
            QCOMPARE(notes[2].tempo, 90.0);
            QCOMPARE(notes[3].tempo, 120.0); // b retains the project tempo
        }
        {
            // An imported tempo equal to the tempo in effect is not recorded.
            kit::ProjectSession session(projectOf({note("a")}, 150));
            kit::DiagnosticList diagnostics;
            ImportMerge::merge(projectOf({note("x")}, 150), kit::ProjectRef(&session),
                               at(ImportMerge::AtEnd), diagnostics);
            QCOMPARE(notesOf(session)[1].tempo, std::nullopt);
        }
        {
            auto options = at(ImportMerge::Before, 1, 1);
            options.keepTempo = false;
            kit::ProjectSession session(projectOf({note("a"), note("b")}));
            kit::DiagnosticList diagnostics;
            ImportMerge::merge(imported, kit::ProjectRef(&session), options, diagnostics);
            const auto notes = notesOf(session);
            QCOMPARE(notes[1].tempo, std::nullopt);
            QCOMPARE(notes[2].tempo, std::nullopt);
            QCOMPARE(notes[3].tempo, std::nullopt); // the tempo in effect for b is still 120
        }
        {
            // A following note with an explicit tempo keeps that tempo.
            kit::ProjectSession session(projectOf({note("a"), note("b", 100)}));
            kit::DiagnosticList diagnostics;
            ImportMerge::merge(imported, kit::ProjectRef(&session), at(ImportMerge::Before, 1, 1),
                               diagnostics);
            QCOMPARE(notesOf(session)[3].tempo, 100.0);
        }
        {
            // The tempo of a replaced note is recorded on the following note.
            auto options = at(ImportMerge::Replace, 1, 1);
            options.keepTempo = false;
            kit::ProjectSession session(projectOf({note("a"), note("b", 100), note("c")}));
            kit::DiagnosticList diagnostics;
            ImportMerge::merge(projectOf({note("x")}), kit::ProjectRef(&session), options,
                               diagnostics);
            const auto notes = notesOf(session);
            QCOMPARE(notes[1].lyric, QStringLiteral("x"));
            QCOMPARE(notes[2].tempo, 100.0);
        }
    }

    // Discarded settings are reported as a note, and removed pitch data of the other mode as a
    // warning.
    void what_is_dropped_is_said() {
        auto imported = projectOf({note("x")});
        imported.settings.name = QStringLiteral("song");
        imported.settings.mode2 = false;
        imported.tracks.first().notes.first().pitchBend = kit::PitchBend();
        {
            kit::ProjectSession session(projectOf({}));
            kit::DiagnosticList diagnostics;
            ImportMerge::merge(imported, kit::ProjectRef(&session), at(ImportMerge::AtEnd),
                               diagnostics);
            QVERIFY(has(diagnostics, kit::DiagnosticSeverity::Note));
            QVERIFY(has(diagnostics, kit::DiagnosticSeverity::Warning));
            QVERIFY(!notesOf(session).first().pitchBend);
            QCOMPARE(session.snapshot().settings.name, QString());
        }
        {
            auto mode1 = projectOf({});
            mode1.settings.mode2 = false;
            kit::ProjectSession session(mode1);
            kit::DiagnosticList diagnostics;
            ImportMerge::merge(imported, kit::ProjectRef(&session), at(ImportMerge::AtEnd),
                               diagnostics);
            QVERIFY(!has(diagnostics, kit::DiagnosticSeverity::Warning));
            QVERIFY(notesOf(session).first().pitchBend);
        }
    }

    void a_selection_outside_the_track_fails() {
        kit::ProjectSession session(projectOf({note("a")}));
        kit::DiagnosticList diagnostics;
        QVERIFY(!ImportMerge::merge(projectOf({note("x")}), kit::ProjectRef(&session),
                                    at(ImportMerge::Replace, 0, 2), diagnostics));
        QVERIFY(kit::hasError(diagnostics));
        QCOMPARE(lyricsOf(session), QStringList({"a"}));
    }
};

QTEST_APPLESS_MAIN(test_ImportMerge)

#include "test_ImportMerge.moc"
