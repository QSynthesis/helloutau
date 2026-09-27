#include <QtTest/QTest>

#include <hellokit/Document/TempoMap.h>

using namespace hello::kit;

class test_TempoMap : public QObject {
    Q_OBJECT

private:
    // Three notes: a quarter at 120, a quarter that sets 60, and a half that inherits 60
    static TempoMap threeNotes() {
        TempoMap map(120);
        map.append(480, std::nullopt);
        map.append(480, 60.0);
        map.append(960, std::nullopt);
        return map;
    }

private Q_SLOTS:
    void a_quarter_note_at_120_lasts_half_a_second() {
        QCOMPARE(TempoMap::duration(480, 120), 500.0);
        QCOMPARE(TempoMap::duration(240, 60), 500.0);
    }

    void each_note_starts_where_the_preceding_one_ends_at_its_own_tempo() {
        const auto map = threeNotes();
        QCOMPARE(map.noteCount(), 3);
        QCOMPARE(map.startTick(0), 0);
        QCOMPARE(map.startTick(1), 480);
        QCOMPARE(map.startTick(2), 960);
        QCOMPARE(map.startTick(3), 1920);
        QCOMPARE(map.startTime(1), 500.0);
        QCOMPARE(map.startTime(2), 1500.0);
        QCOMPARE(map.startTime(3), 3500.0);
        QCOMPARE(map.tempo(0), 120.0);
        QCOMPARE(map.tempo(1), 60.0);
        QCOMPARE(map.tempo(2), 60.0);
    }

    void ticks_and_times_convert_within_and_beyond_the_notes() {
        const auto map = threeNotes();
        QCOMPARE(map.timeOf(240), 250.0);
        QCOMPARE(map.timeOf(720), 1000.0);
        QCOMPARE(map.timeOf(1920), 3500.0);
        // Before the first note at the initial tempo, after the last at the last tempo
        QCOMPARE(map.timeOf(-480), -500.0);
        QCOMPARE(map.timeOf(2400), 4500.0);

        for (const double tick : {-480.0, 0.0, 240.0, 480.0, 720.0, 1500.0, 1920.0, 2400.0}) {
            QCOMPARE(map.tickOf(map.timeOf(tick)), tick);
        }
    }

    void the_note_at_a_tick_is_found() {
        const auto map = threeNotes();
        QCOMPARE(map.noteAt(-1), -1);
        QCOMPARE(map.noteAt(0), 0);
        QCOMPARE(map.noteAt(479.5), 0);
        QCOMPARE(map.noteAt(480), 1);
        QCOMPARE(map.noteAt(1919), 2);
        QCOMPARE(map.noteAt(1920), 3);
    }

    void a_note_of_zero_length_contains_no_tick() {
        TempoMap map(120);
        map.append(480, std::nullopt);
        map.append(0, 60.0);
        map.append(480, std::nullopt);
        QCOMPARE(map.noteAt(480), 2);
        QCOMPARE(map.tempo(2), 60.0);
        QCOMPARE(map.timeOf(960), 1500.0);
        QCOMPARE(map.tickOf(750), 600.0);
    }

    void a_tempo_that_is_not_positive_is_ignored() {
        TempoMap map(0);
        map.append(480, -10.0);
        map.append(480, 0.0);
        QCOMPARE(map.tempo(0), utau::DEFAULT_VALUE_TEMPO);
        QCOMPARE(map.tempo(1), utau::DEFAULT_VALUE_TEMPO);
        QCOMPARE(map.startTime(2), 1000.0);

        // A negative length would move the following notes backward
        map.append(-480, std::nullopt);
        QCOMPARE(map.startTick(3), 960);
    }

    void a_project_gives_the_map_of_its_first_track() {
        Note first;
        first.length = 480;
        Note second;
        second.length = 480;
        second.tempo = 60;

        Track track;
        track.notes = {first, second};
        Project project;
        project.settings.tempo = 240;
        project.tracks.push_back(track);

        const auto map = TempoMap::of(project);
        QCOMPARE(map.noteCount(), 2);
        QCOMPARE(map.startTime(1), 250.0);
        QCOMPARE(map.startTime(2), 1250.0);
    }
};

QTEST_APPLESS_MAIN(test_TempoMap)

#include "test_TempoMap.moc"
