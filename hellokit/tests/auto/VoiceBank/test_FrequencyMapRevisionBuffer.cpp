#include <cmath>
#include <limits>

#include <QtTest/QTest>

#include <hellokit/VoiceBank/FrequencyMapRevisionBuffer.h>

using namespace hello::kit;

namespace {

    FrequencyMap mapOf(std::initializer_list<double> frequencies) {
        FrequencyMap map;
        map.hop = 256;
        double time = 0;
        for (const double f : frequencies) {
            FrequencyMap::Frame frame;
            frame.time = time;
            frame.frequency = f;
            frame.amplitude = 1;
            map.frames.push_back(frame);
            time += 5;
        }
        return map;
    }

    std::vector<double> frequenciesOf(const FrequencyMap &map) {
        std::vector<double> frequencies;
        for (const auto &frame : map.frames) {
            frequencies.push_back(frame.frequency);
        }
        return frequencies;
    }

    bool near(double a, double b) {
        return std::abs(a - b) < 1e-9 * std::max(1.0, std::abs(b));
    }

}

class test_FrequencyMapRevisionBuffer : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // A frequency above B8 becomes B8, and a frequency not above 55 Hz marks the frame
    // unvoiced, as NaN does.
    void frequencies_are_limited() {
        QCOMPARE(FrequencyMapRevisionBuffer::limited(440), 440.0);
        QCOMPARE(FrequencyMapRevisionBuffer::limited(10000),
                 FrequencyMapRevisionBuffer::highestFrequency);
        QCOMPARE(FrequencyMapRevisionBuffer::limited(55), 0.0);
        QCOMPARE(FrequencyMapRevisionBuffer::limited(-1), 0.0);
        QCOMPARE(FrequencyMapRevisionBuffer::limited(std::numeric_limits<double>::quiet_NaN()),
                 0.0);
    }

    // A stroke writes the frames between its points on the pitch scale, and is one step that
    // undo and redo restore.
    void a_stroke_is_one_step() {
        FrequencyMapRevisionBuffer map(mapOf({100, 100, 100, 100, 100}));
        map.beginStroke(1, 200);
        QVERIFY(map.isStroking());
        QVERIFY(!map.canUndo());
        QCOMPARE(map.map().frames[1].frequency, 200.0);
        map.strokeTo(3, 800);
        // The middle frame is halfway on the pitch scale, 400 Hz, not 500 Hz.
        QVERIFY(near(map.map().frames[2].frequency, 400));
        QVERIFY(near(map.map().frames[3].frequency, 800));
        QVERIFY(map.endStroke());
        QVERIFY(!map.isStroking());
        QVERIFY(map.isModified());
        const auto drawn = frequenciesOf(map.map());

        QVERIFY(map.undo());
        QCOMPARE(frequenciesOf(map.map()), (std::vector<double>{100, 100, 100, 100, 100}));
        QVERIFY(!map.isModified());
        QVERIFY(!map.canUndo());
        QVERIFY(map.redo());
        QCOMPARE(frequenciesOf(map.map()), drawn);
        QVERIFY(!map.canRedo());
    }

    // A stroke backwards and beyond the map writes the frames within the map, and an unvoiced
    // point unvoices the frames up to it.
    void a_stroke_erases_and_stays_within_the_map() {
        FrequencyMapRevisionBuffer map(mapOf({100, 100, 100, 100}));
        map.beginStroke(6, 0);
        map.strokeTo(2, 0);
        QVERIFY(map.endStroke());
        QCOMPARE(frequenciesOf(map.map()), (std::vector<double>{100, 100, 0, 0}));

        map.beginStroke(0, 300);
        map.strokeTo(1, 0);
        QVERIFY(map.endStroke());
        QCOMPARE(frequenciesOf(map.map()), (std::vector<double>{0, 0, 0, 0}));
    }

    // Canceling a stroke restores the frequencies and adds no step, and a stroke that changes
    // nothing adds no step.
    void a_canceled_stroke_adds_no_step() {
        FrequencyMapRevisionBuffer map(mapOf({100, 100, 100}));
        map.beginStroke(0, 300);
        map.strokeTo(2, 300);
        map.cancelStroke();
        QCOMPARE(frequenciesOf(map.map()), (std::vector<double>{100, 100, 100}));
        QVERIFY(!map.canUndo());

        map.beginStroke(0, 100);
        map.strokeTo(2, 100);
        QVERIFY(!map.endStroke());
        QVERIFY(!map.canUndo());
    }

    // The other edits are rejected while a stroke is in progress.
    void edits_wait_for_the_stroke() {
        FrequencyMapRevisionBuffer map(mapOf({100, 100, 100}));
        map.beginStroke(0, 300);
        QVERIFY(!map.scale(0, 3, 2));
        QVERIFY(!map.unvoice(0, 3));
        QVERIFY(!map.interpolate(0, 3));
        QVERIFY(!map.revert(0, 3));
        QVERIFY(!map.undo());
        QVERIFY(map.endStroke());
        QVERIFY(map.canUndo());
    }

    // Scaling changes only the voiced frames of the range, those above 55 Hz, limited, and the
    // range is limited to the map.
    void scaling_changes_the_voiced_frames() {
        FrequencyMapRevisionBuffer map(mapOf({100, 30, 5000, 200}));
        QVERIFY(map.scale(0, 10, 2));
        QCOMPARE(frequenciesOf(map.map()),
                 (std::vector<double>{200, 30, FrequencyMapRevisionBuffer::highestFrequency, 400}));
        QVERIFY(map.scale(3, 1, 0.5));
        QCOMPARE(map.map().frames[3].frequency, 200.0);
        // A sixth of 200 Hz is not above 55 Hz.
        QVERIFY(map.scale(0, 1, 1.0 / 6));
        QCOMPARE(map.map().frames[0].frequency, 0.0);
        QVERIFY(!map.scale(4, 2, 2));
        QVERIFY(!map.scale(1, 1, 2));
    }

    // Unvoicing marks every frame of the range unvoiced.
    void unvoicing_marks_the_range() {
        FrequencyMapRevisionBuffer map(mapOf({100, 200, 300}));
        QVERIFY(map.unvoice(1, 2));
        QCOMPARE(frequenciesOf(map.map()), (std::vector<double>{100, 0, 0}));
        QVERIFY(!map.unvoice(1, 2));
    }

    // Interpolation fills the frames between the ends linearly in hertz, whether voiced or
    // not, and requires both ends voiced.
    void interpolation_requires_voiced_ends() {
        FrequencyMapRevisionBuffer map(mapOf({100, 0, 900, 400, 9}));
        QVERIFY(map.interpolate(0, 4));
        QCOMPARE(frequenciesOf(map.map()), (std::vector<double>{100, 200, 300, 400, 9}));
        // 9 Hz is not above 55 Hz, therefore unvoiced.
        QVERIFY(!map.interpolate(1, 4));
        QVERIFY(!map.interpolate(0, 2));
    }

    // Reverting restores the saved frequencies of the range, and saving moves the saved map
    // while the history stays.
    void reverting_restores_the_saved_map() {
        FrequencyMapRevisionBuffer map(mapOf({100, 200, 300}));
        QVERIFY(map.scale(0, 3, 2));
        QVERIFY(map.revert(1, 1));
        QCOMPARE(frequenciesOf(map.map()), (std::vector<double>{200, 200, 600}));

        map.markSaved();
        QVERIFY(!map.isModified());
        QCOMPARE(frequenciesOf(map.savedMap()), (std::vector<double>{200, 200, 600}));
        QVERIFY(map.undo());
        QVERIFY(map.isModified());
        QVERIFY(map.revert(0, 3));
        QVERIFY(!map.isModified());
        QVERIFY(map.canUndo());
    }

    // A new step discards the undone steps.
    void a_new_step_discards_the_undone_steps() {
        FrequencyMapRevisionBuffer map(mapOf({100, 200}));
        QVERIFY(map.scale(0, 2, 2));
        QVERIFY(map.unvoice(0, 1));
        QVERIFY(map.undo());
        QVERIFY(map.canRedo());
        QVERIFY(map.scale(1, 1, 0.5));
        QVERIFY(!map.canRedo());
        QVERIFY(map.undo());
        QVERIFY(map.undo());
        QCOMPARE(frequenciesOf(map.map()), (std::vector<double>{100, 200}));
        QVERIFY(!map.canUndo());
    }

    // An edit changes the frequencies only.
    void an_edit_changes_only_the_frequencies() {
        auto source = mapOf({100, 200});
        source.averageFrequency = 150;
        FrequencyMapRevisionBuffer map(source);
        QVERIFY(map.scale(0, 2, 2));
        QCOMPARE(map.map().averageFrequency, std::optional<double>(150));
        QCOMPARE(map.map().hop, 256.0);
        QCOMPARE(map.map().frames[1].time, 5.0);
        QCOMPARE(map.map().frames[1].amplitude, std::optional<double>(1));
    }
};

QTEST_MAIN(test_FrequencyMapRevisionBuffer)

#include "test_FrequencyMapRevisionBuffer.moc"
