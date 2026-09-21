#define BOOST_TEST_MAIN
#define BOOST_TEST_MODULE test_MidiReader

#include <filesystem>
#include <fstream>
#include <string>

#include <boost/test/unit_test.hpp>

#include <wolf-midi/MidiFile.h>

#include <hellokit/Interchange/Formats/MidiReader.h>

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
            BOOST_REQUIRE(midi.save(_path));
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

    bool warned(const DiagnosticList &diagnostics) {
        return std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic &d) {
            return d.severity == DiagnosticSeverity::Warning;
        });
    }

}

BOOST_AUTO_TEST_SUITE(test_MidiReader)

// A quarter note at this resolution is 96 ticks and has to come out as 480. Ticks are scaled
// from absolute positions rather than one length at a time, so a long track cannot drift off the
// bar lines.
BOOST_AUTO_TEST_CASE(ticks_are_scaled_to_480_per_quarter) {
    TempMidi file("scale");
    file.note(0, 96, 60);
    file.note(96, 288, 62);

    auto result = importOf(file.save());
    BOOST_REQUIRE(result.project.has_value());

    const auto &notes = result.project->tracks.first().notes;
    BOOST_REQUIRE_EQUAL(notes.size(), 2);
    BOOST_CHECK_EQUAL(notes.at(0).length, 480);
    BOOST_CHECK_EQUAL(notes.at(1).length, 960);
    BOOST_CHECK_EQUAL(notes.at(0).noteNum, 60);
}

// Silence in front of the first note is really there, so it is translated rather than trimmed.
// Whoever is inserting these notes somewhere decides whether to keep it.
BOOST_AUTO_TEST_CASE(silence_before_the_first_note_becomes_a_rest) {
    TempMidi file("lead");
    file.note(192, 288, 64);

    auto result = importOf(file.save());
    BOOST_REQUIRE(result.project.has_value());

    const auto &notes = result.project->tracks.first().notes;
    BOOST_REQUIRE_EQUAL(notes.size(), 2);
    BOOST_CHECK(notes.at(0).isRest());
    BOOST_CHECK_EQUAL(notes.at(0).length, 960);
    BOOST_CHECK(!notes.at(1).isRest());
}

BOOST_AUTO_TEST_CASE(a_gap_between_notes_becomes_a_rest) {
    TempMidi file("gap");
    file.note(0, 96, 60);
    file.note(192, 288, 62);

    auto result = importOf(file.save());
    BOOST_REQUIRE(result.project.has_value());

    const auto &notes = result.project->tracks.first().notes;
    BOOST_REQUIRE_EQUAL(notes.size(), 3);
    BOOST_CHECK(notes.at(1).isRest());
    BOOST_CHECK_EQUAL(notes.at(1).length, 480);
}

// A track holds one voice, so an overlap has to go. Shortening the note already sounding keeps
// both of them, where dropping the later one loses a note outright.
BOOST_AUTO_TEST_CASE(an_overlap_shortens_the_note_already_sounding) {
    TempMidi file("overlap");
    file.note(0, 192, 60);
    file.note(96, 288, 62);

    auto result = importOf(file.save());
    BOOST_REQUIRE(result.project.has_value());

    const auto &notes = result.project->tracks.first().notes;
    BOOST_REQUIRE_EQUAL(notes.size(), 2);
    BOOST_CHECK_EQUAL(notes.at(0).noteNum, 60);
    BOOST_CHECK_EQUAL(notes.at(0).length, 480); // 96 ticks, cut where the next one starts
    BOOST_CHECK_EQUAL(notes.at(1).noteNum, 62);
    BOOST_CHECK_EQUAL(notes.at(1).length, 960);
    BOOST_CHECK(warned(result.diagnostics));
}

// Notes that begin together cannot all be kept, and the top one is the melody.
BOOST_AUTO_TEST_CASE(a_chord_keeps_its_highest_note_and_says_so) {
    TempMidi file("chord");
    file.note(0, 96, 60);
    file.note(0, 96, 64);
    file.note(0, 96, 67);

    auto result = importOf(file.save());
    BOOST_REQUIRE(result.project.has_value());

    const auto &notes = result.project->tracks.first().notes;
    BOOST_REQUIRE_EQUAL(notes.size(), 1);
    BOOST_CHECK_EQUAL(notes.at(0).noteNum, 67);
    BOOST_CHECK(warned(result.diagnostics));
}

// The earlier implementation matched a lyric to a note only when the ticks were equal, so a
// sequencer that placed one a tick early lost it without a word.
BOOST_AUTO_TEST_CASE(a_lyric_placed_late_still_lands_on_its_note) {
    TempMidi file("lyric");
    file.note(0, 96, 60);
    file.note(96, 192, 62);
    file.lyric(2, "ka");   // a little after the first note began
    file.lyric(96, "sa");  // exactly on the second

    auto result = importOf(file.save());
    BOOST_REQUIRE(result.project.has_value());

    const auto &notes = result.project->tracks.first().notes;
    BOOST_REQUIRE_EQUAL(notes.size(), 2);
    BOOST_CHECK_EQUAL(notes.at(0).lyric.toStdString(), "ka");
    BOOST_CHECK_EQUAL(notes.at(1).lyric.toStdString(), "sa");
}

// A note with no lyric of its own still has to say something, because a UST note with an empty
// lyric is a rest.
BOOST_AUTO_TEST_CASE(a_note_with_no_lyric_gets_the_default_one) {
    TempMidi file("nolyric");
    file.note(0, 96, 60);

    auto result = importOf(file.save());
    BOOST_REQUIRE(result.project.has_value());

    const auto &notes = result.project->tracks.first().notes;
    BOOST_REQUIRE_EQUAL(notes.size(), 1);
    BOOST_CHECK(!notes.at(0).isRest());
    BOOST_CHECK_EQUAL(notes.at(0).lyric.toStdString(), "la");
}

BOOST_AUTO_TEST_CASE(the_first_tempo_becomes_the_project_tempo) {
    TempMidi file("tempo");
    file.tempo(0, 143.0f);
    file.note(0, 96, 60);

    auto result = importOf(file.save());
    BOOST_REQUIRE(result.project.has_value());
    BOOST_CHECK_CLOSE(result.project->settings.tempo, 143.0, 0.01);
}

// UTAU's keyboard stops at C1 and B7, so anything further out has nowhere to go.
BOOST_AUTO_TEST_CASE(pitches_outside_the_keyboard_are_pulled_back_in) {
    TempMidi file("range");
    file.note(0, 96, 12);
    file.note(96, 192, 120);

    auto result = importOf(file.save());
    BOOST_REQUIRE(result.project.has_value());

    const auto &notes = result.project->tracks.first().notes;
    BOOST_REQUIRE_EQUAL(notes.size(), 2);
    BOOST_CHECK_EQUAL(notes.at(0).noteNum, 24);
    BOOST_CHECK_EQUAL(notes.at(1).noteNum, 107);
    BOOST_CHECK(warned(result.diagnostics));
}

BOOST_AUTO_TEST_CASE(inspect_reports_what_the_file_holds) {
    TempMidi file("inspect");
    file.note(0, 96, 60);
    file.note(96, 192, 72);

    MidiReader reader;
    DiagnosticList diagnostics;
    auto source = reader.inspect(file.save(), diagnostics);

    BOOST_REQUIRE(source.has_value());
    BOOST_REQUIRE(!source->entries.isEmpty());

    const auto &entry = source->entries.first();
    BOOST_CHECK_EQUAL(entry.noteCount, 2);
    BOOST_REQUIRE(entry.lowestNote.has_value());
    BOOST_CHECK_EQUAL(*entry.lowestNote, 60);
    BOOST_REQUIRE(entry.highestNote.has_value());
    BOOST_CHECK_EQUAL(*entry.highestNote, 72);
}

BOOST_AUTO_TEST_CASE(something_that_is_not_a_midi_file_is_an_error) {
    const auto path = fs::temp_directory_path() / "hellokit_notmidi.mid";
    {
        std::ofstream out(path, std::ios::binary);
        out << "this is not a MIDI file";
    }

    auto result = importOf(path);
    BOOST_CHECK(!result.project.has_value());
    BOOST_CHECK(!result.cancelled);
    BOOST_CHECK(hasError(result.diagnostics));

    std::error_code ignored;
    fs::remove(path, ignored);
}

BOOST_AUTO_TEST_SUITE_END()
