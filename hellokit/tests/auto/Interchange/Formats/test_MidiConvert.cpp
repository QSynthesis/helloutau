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

    // The files are generated here rather than checked in, so that the input of each case is
    // defined next to its assertions instead of in a binary file.
    constexpr int Resolution = 96; // not 480, so that scaling is tested

    class TempMidi {
    public:
        explicit TempMidi(const std::string &name) {
            m_path = fs::temp_directory_path() / ("hellokit_" + name + ".mid");
            midi.setFileFormat(1);
            midi.setDivisionType(Midi::MidiFile::PPQ);
            midi.setResolution(Resolution);
            track = midi.createTrack();
        }

        ~TempMidi() {
            std::error_code ignored;
            fs::remove(m_path, ignored);
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
            midi.save(m_path);
            return m_path;
        }

        Midi::MidiFile midi;
        int track = 0;

    private:
        fs::path m_path;
    };

    ImportResult importOf(const fs::path &path) {
        MidiReader reader;
        return reader.read(path, nullptr);
    }

    // A substitute for the selector, so that a test case can specify the output encoding
    // through the public interface only.
    class FixedEncodingSelector : public InterchangeSelector {
    public:
        explicit FixedEncodingSelector(QString encoding) : m_encoding(std::move(encoding)) {
        }

        std::optional<ImportRequest> selectImport(const InterchangeReader &,
                                                  const InterchangeSource &, const ImportLimits &,
                                                  DiagnosticList &) override {
            return std::nullopt;
        }

        std::optional<ExportRequest> selectExport(const InterchangeWriter &, const Project &,
                                                  DiagnosticList &) override {
            ExportRequest request;
            request.driverOptions.insert(QStringLiteral("encoding"), m_encoding);
            return request;
        }

    private:
        QString m_encoding;
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
    // A quarter note at this resolution is 96 ticks and must become 480. Ticks are scaled from
    // absolute positions rather than per length, so that a long track cannot drift away from
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

    // The silence before the first note exists in the file, so it is translated rather than
    // trimmed. The caller that inserts the notes decides whether to keep it.
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

    // A track is monophonic, so an overlap must be resolved. Shortening the note already
    // sounding preserves both notes, whereas dropping the later note loses it entirely.
    void an_overlap_shortens_the_note_already_sounding() {
        TempMidi file("overlap");
        file.note(0, 192, 60);
        file.note(96, 288, 62);

        const auto result = importOf(file.save());
        QVERIFY(result.project.has_value());

        const auto &notes = result.project->tracks.first().notes;
        QCOMPARE(notes.size(), 2);
        QCOMPARE(notes.at(0).noteNum, 60);
        QCOMPARE(notes.at(0).length, 480); // 96 ticks, cut at the start of the next note
        QCOMPARE(notes.at(1).noteNum, 62);
        QCOMPARE(notes.at(1).length, 960);
        QVERIFY(warned(result.diagnostics));
    }

    // Notes that start together cannot all be kept, and the highest note carries the melody.
    void a_chord_keeps_its_highest_note_with_a_warning() {
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

    // A lyric is assigned to the note sounding at its time. Matching only equal ticks would
    // silently lose a lyric that a sequencer placed one tick early.
    void a_lyric_placed_late_still_lands_on_its_note() {
        TempMidi file("lyric");
        file.note(0, 96, 60);
        file.note(96, 192, 62);
        file.lyric(2, "ka");  // shortly after the start of the first note
        file.lyric(96, "sa"); // exactly at the start of the second note

        const auto result = importOf(file.save());
        QVERIFY(result.project.has_value());

        const auto &notes = result.project->tracks.first().notes;
        QCOMPARE(notes.size(), 2);
        QCOMPARE(notes.at(0).lyric, QStringLiteral("ka"));
        QCOMPARE(notes.at(1).lyric, QStringLiteral("sa"));
    }

    // A note without a lyric still requires one, because a UST note with an empty lyric is a
    // rest.
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

        // Inexact by necessity. MIDI stores tempo as whole microseconds per quarter note, so
        // 143 BPM is stored as 419580 and read back as 143.0001.
        QVERIFY(qAbs(result.project->settings.tempo - 143.0) < 0.001);
    }

    // The UTAU keyboard ranges from C1 to B7, so notes outside that range cannot be placed.
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

    // Exported data must be imported unchanged, which is the only meaningful guarantee for a
    // format with such limited content.
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

    // MIDI represents only notes and lyrics among the data of this project, so every export
    // loses the remainder. Reporting this on every export is intentional.
    void writing_reports_the_data_midi_cannot_represent() {
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

    // A lyric that the selected encoding cannot represent is not escaped. MIDI cannot record
    // that the file was written by HelloUtau, so an escape sequence would be read back as
    // literal text.
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
        QVERIFY(std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                            [](const Diagnostic &d) {
                                return d.message.contains(QStringLiteral("cannot be represented"));
                            }));

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
