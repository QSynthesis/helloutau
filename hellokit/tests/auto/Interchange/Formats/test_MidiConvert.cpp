#include <filesystem>
#include <fstream>
#include <string>

#include <QtTest/QTest>

#include <wolf-midi/MidiFile.h>

#include <hellokit/Interchange/Formats/MidiConvert.h>
#include <hellokit/Interchange/InterchangeSelector.h>

using namespace hello::kit;
namespace fs = std::filesystem;

namespace {

    // The files are built here rather than checked in, so that what each case relies on is
    // written next to what it asserts instead of hidden in a blob.
    constexpr int Resolution = 96; // not 480, so that the scaling is exercised

    class TempMidi {
    public:
        explicit TempMidi(const std::string &name) {
            _path = fs::temp_directory_path() / ("hellokit_" + name + ".mid");
            midi.setFileFormat(1);
            midi.setDivisionType(Midi::MidiFile::PPQ);
            midi.setResolution(Resolution);
            track = midi.createTrack();
        }

        ~TempMidi() {
            std::error_code ignored;
            fs::remove(_path, ignored);
        }

        void note(int startTick, int endTick, int pitch, int voice = 0) {
            midi.createNote(track, startTick, endTick, voice, pitch, 100, 64);
        }

        void lyric(int tick, const std::string &text) {
            midi.createLyricEvent(track, tick, std::vector<char>(text.begin(), text.end()));
        }

        void tempo(int tick, float bpm) {
            midi.createTempoEvent(track, tick, bpm);
        }

        const fs::path &save() {
            midi.save(_path);
            return _path;
        }

        Midi::MidiFile midi;
        int track = 0;

    private:
        fs::path _path;
    };

    ImportResult importOf(const fs::path &path) {
        MidiReader reader;
        return reader.read(path, nullptr);
    }

    // Stands in for the chooser, so that a case can say which encoding to write in without
    // reaching past the public interface.
    class FixedEncodingSelector : public InterchangeSelector {
    public:
        explicit FixedEncodingSelector(QString encoding) : _encoding(std::move(encoding)) {
        }

        std::optional<ImportRequest> selectImport(const InterchangeReader &,
                                                  const InterchangeSource &, const ImportLimits &,
                                                  DiagnosticList &) override {
            return std::nullopt;
        }

        std::optional<ExportRequest> selectExport(const InterchangeWriter &, const Project &,
                                                  DiagnosticList &) override {
            ExportRequest request;
            request.driverOptions.insert(QStringLiteral("encoding"), _encoding);
            return request;
        }

    private:
        QString _encoding;
    };

    bool warned(const DiagnosticList &diagnostics) {
        return std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic &d) {
            return d.severity == DiagnosticSeverity::Warning;
        });
    }

}

class test_MidiConvert : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // A quarter note at this resolution is 96 ticks and has to come out as 480. Ticks are scaled
    // from absolute positions rather than one length at a time, so a long track cannot drift off
    // the bar lines.
    void ticks_are_scaled_to_480_per_quarter() {
        TempMidi file("scale");
        file.note(0, 96, 60);
        file.note(96, 288, 62);

        const auto result = importOf(file.save());
        QVERIFY(result.project.has_value());

        const auto &notes = result.project->tracks.first().notes;
        QCOMPARE(notes.size(), 2);
        QCOMPARE(notes.at(0).length, 480);
        QCOMPARE(notes.at(1).length, 960);
        QCOMPARE(notes.at(0).noteNum, 60);
    }

    // Silence in front of the first note is really there, so it is translated rather than
    // trimmed. Whoever is inserting these notes somewhere decides whether to keep it.
    void silence_before_the_first_note_becomes_a_rest() {
        TempMidi file("lead");
        file.note(192, 288, 64);

        const auto result = importOf(file.save());
        QVERIFY(result.project.has_value());

        const auto &notes = result.project->tracks.first().notes;
        QCOMPARE(notes.size(), 2);
        QVERIFY(notes.at(0).isRest());
        QCOMPARE(notes.at(0).length, 960);
        QVERIFY(!notes.at(1).isRest());
    }

    void a_gap_between_notes_becomes_a_rest() {
        TempMidi file("gap");
        file.note(0, 96, 60);
        file.note(192, 288, 62);

        const auto result = importOf(file.save());
        QVERIFY(result.project.has_value());

        const auto &notes = result.project->tracks.first().notes;
        QCOMPARE(notes.size(), 3);
        QVERIFY(notes.at(1).isRest());
        QCOMPARE(notes.at(1).length, 480);
    }

    // A track holds one voice, so an overlap has to go. Shortening the note already sounding
    // keeps both of them, where dropping the later one loses a note outright.
    void an_overlap_shortens_the_note_already_sounding() {
        TempMidi file("overlap");
        file.note(0, 192, 60);
        file.note(96, 288, 62);

        const auto result = importOf(file.save());
        QVERIFY(result.project.has_value());

        const auto &notes = result.project->tracks.first().notes;
        QCOMPARE(notes.size(), 2);
        QCOMPARE(notes.at(0).noteNum, 60);
        QCOMPARE(notes.at(0).length, 480); // 96 ticks, cut where the next one starts
        QCOMPARE(notes.at(1).noteNum, 62);
        QCOMPARE(notes.at(1).length, 960);
        QVERIFY(warned(result.diagnostics));
    }

    // Notes that begin together cannot all be kept, and the top one is the melody.
    void a_chord_keeps_its_highest_note_and_says_so() {
        TempMidi file("chord");
        file.note(0, 96, 60);
        file.note(0, 96, 64);
        file.note(0, 96, 67);

        const auto result = importOf(file.save());
        QVERIFY(result.project.has_value());

        const auto &notes = result.project->tracks.first().notes;
        QCOMPARE(notes.size(), 1);
        QCOMPARE(notes.at(0).noteNum, 67);
        QVERIFY(warned(result.diagnostics));
    }

    // The earlier implementation matched a lyric to a note only when the ticks were equal, so a
    // sequencer that placed one a tick early lost it without a word.
    void a_lyric_placed_late_still_lands_on_its_note() {
        TempMidi file("lyric");
        file.note(0, 96, 60);
        file.note(96, 192, 62);
        file.lyric(2, "ka");  // a little after the first note began
        file.lyric(96, "sa"); // exactly on the second

        const auto result = importOf(file.save());
        QVERIFY(result.project.has_value());

        const auto &notes = result.project->tracks.first().notes;
        QCOMPARE(notes.size(), 2);
        QCOMPARE(notes.at(0).lyric, QStringLiteral("ka"));
        QCOMPARE(notes.at(1).lyric, QStringLiteral("sa"));
    }

    // A note with no lyric of its own still has to say something, because a UST note with an
    // empty lyric is a rest.
    void a_note_with_no_lyric_gets_the_default_one() {
        TempMidi file("nolyric");
        file.note(0, 96, 60);

        const auto result = importOf(file.save());
        QVERIFY(result.project.has_value());

        const auto &notes = result.project->tracks.first().notes;
        QCOMPARE(notes.size(), 1);
        QVERIFY(!notes.at(0).isRest());
        QCOMPARE(notes.at(0).lyric, QStringLiteral("la"));
    }

    void the_first_tempo_becomes_the_project_tempo() {
        TempMidi file("tempo");
        file.tempo(0, 143.0f);
        file.note(0, 96, 60);

        const auto result = importOf(file.save());
        QVERIFY(result.project.has_value());

        // Not exact, and cannot be. MIDI keeps tempo as whole microseconds per quarter note, so
        // 143 BPM is stored as 419580 and reads back as 143.0001.
        QVERIFY(qAbs(result.project->settings.tempo - 143.0) < 0.001);
    }

    // UTAU's keyboard stops at C1 and B7, so anything further out has nowhere to go.
    void pitches_outside_the_keyboard_are_pulled_back_in() {
        TempMidi file("range");
        file.note(0, 96, 12);
        file.note(96, 192, 120);

        const auto result = importOf(file.save());
        QVERIFY(result.project.has_value());

        const auto &notes = result.project->tracks.first().notes;
        QCOMPARE(notes.size(), 2);
        QCOMPARE(notes.at(0).noteNum, 24);
        QCOMPARE(notes.at(1).noteNum, 107);
        QVERIFY(warned(result.diagnostics));
    }

    void inspect_reports_what_the_file_holds() {
        TempMidi file("inspect");
        file.note(0, 96, 60);
        file.note(96, 192, 72);

        MidiReader reader;
        DiagnosticList diagnostics;
        const auto source = reader.inspect(file.save(), diagnostics);

        QVERIFY(source.has_value());
        QVERIFY(!source->entries.isEmpty());

        const auto &entry = source->entries.first();
        QCOMPARE(entry.noteCount, 2);
        QVERIFY(entry.lowestNote.has_value());
        QCOMPARE(*entry.lowestNote, 60);
        QVERIFY(entry.highestNote.has_value());
        QCOMPARE(*entry.highestNote, 72);
    }

    // What goes out has to come back, which is the only claim worth making about a format that
    // holds so little.
    void what_midi_can_hold_survives_a_round_trip() {
        const fs::path path = fs::temp_directory_path() / "hellokit_written.mid";

        Project project;
        project.settings.tempo = 132.0;
        Track track;
        track.notes.push_back(Note{QStringLiteral("ka"), 480, 60});
        track.notes.push_back(Note{QStringLiteral("R"), 240, 60});
        track.notes.push_back(Note{QStringLiteral("sa"), 480, 64});
        project.tracks.push_back(track);

        MidiWriter writer;
        const auto written = writer.write(project, path, nullptr);
        QVERIFY(written.written);

        const auto result = importOf(path);
        QVERIFY(result.project.has_value());

        const auto &notes = result.project->tracks.first().notes;
        QCOMPARE(notes.size(), 3);
        QCOMPARE(notes.at(0).lyric, QStringLiteral("ka"));
        QCOMPARE(notes.at(0).length, 480);
        QCOMPARE(notes.at(0).noteNum, 60);
        QVERIFY(notes.at(1).isRest());
        QCOMPARE(notes.at(1).length, 240);
        QCOMPARE(notes.at(2).lyric, QStringLiteral("sa"));
        QCOMPARE(notes.at(2).noteNum, 64);
        QVERIFY(qAbs(result.project->settings.tempo - 132.0) < 0.001);

        std::error_code ignored;
        fs::remove(path, ignored);
    }

    // MIDI holds notes and lyrics and nothing else this project works with, so every export
    // loses the rest. Saying so every time is the point, not a nuisance.
    void writing_says_what_midi_cannot_hold() {
        const fs::path path = fs::temp_directory_path() / "hellokit_lossy.mid";

        Project project;
        Track track;
        Note note{QStringLiteral("a"), 480, 60};
        note.vibrato = Vibrato{65, 180, 35, 20, 20, 0, 0, 0};
        track.notes.push_back(note);
        project.tracks.push_back(track);

        MidiWriter writer;
        const auto result = writer.write(project, path, nullptr);
        QVERIFY(result.written);
        QVERIFY(warned(result.diagnostics));

        std::error_code ignored;
        fs::remove(path, ignored);
    }

    // A lyric the chosen encoding cannot spell is not escaped, because MIDI has nowhere to say
    // that the file was written here, so an escape would read back as its own literal text.
    void a_lyric_the_encoding_cannot_hold_is_reported_not_escaped() {
        const fs::path path = fs::temp_directory_path() / "hellokit_lossyname.mid";

        Project project;
        Track track;
        track.notes.push_back(Note{QString::fromUtf8("你"), 480, 60});
        project.tracks.push_back(track);

        MidiWriter writer;
        FixedEncodingSelector selector{QStringLiteral("Shift_JIS")};
        const auto result = writer.write(project, path, &selector);

        QVERIFY(result.written);
        QVERIFY(std::any_of(
            result.diagnostics.begin(), result.diagnostics.end(),
            [](const Diagnostic &d) { return d.message.contains(QStringLiteral("no spelling")); }));

        std::error_code ignored;
        fs::remove(path, ignored);
    }

    void something_that_is_not_a_midi_file_is_an_error() {
        const auto path = fs::temp_directory_path() / "hellokit_notmidi.mid";
        {
            std::ofstream out(path, std::ios::binary);
            out << "this is not a MIDI file";
        }

        const auto result = importOf(path);
        QVERIFY(!result.project.has_value());
        QVERIFY(!result.cancelled);
        QVERIFY(hasError(result.diagnostics));

        std::error_code ignored;
        fs::remove(path, ignored);
    }
};

QTEST_APPLESS_MAIN(test_MidiConvert)

#include "test_MidiConvert.moc"
