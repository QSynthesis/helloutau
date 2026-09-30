#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <helloutau/Editor/OtoWaveformView.h>

using namespace hello;
using namespace hello::daw;

namespace {

    using Value = OtoWaveformView::Value;

    // A second of audio at 1000 Hz, one frame per millisecond
    std::shared_ptr<const kit::WaveAudio> second() {
        auto audio = std::make_shared<kit::WaveAudio>();
        audio->sampleRate = 1000;
        audio->channels = 1;
        audio->samples.assign(1000, 0.5f);
        return audio;
    }

    // offset 100, consonant 50, cutoff -300, pre-utterance 80, overlap 20: at 100, 150, 400,
    // 180 and 120
    kit::VoiceOtoEntry entry(double cutoff = -300) {
        kit::VoiceOtoEntry entry;
        entry.fileName = QStringLiteral("a.wav");
        entry.alias = QStringLiteral("a");
        entry.offset = 100;
        entry.consonant = 50;
        entry.cutoff = cutoff;
        entry.preUtterance = 80;
        entry.voiceOverlap = 20;
        return entry;
    }

    double at(const kit::VoiceOtoEntry &value, Value which) {
        return OtoWaveformView::positionOf(value, which, 1000);
    }

}

class test_OtoWaveformView : public QObject {
    Q_OBJECT

private:
    static void show(OtoWaveformView &view) {
        view.resize(1040, 200);
        view.show();
        view.setAudio(second());
        view.setEntry(entry());
    }

    static QPoint pointAt(const OtoWaveformView &view, double time) {
        return QPointF(view.xOf(time), 100).toPoint();
    }

    static void drag(OtoWaveformView &view, double from, double to) {
        const auto viewport = view.viewport();
        QTest::mousePress(viewport, Qt::LeftButton, {}, pointAt(view, from));
        QTest::mouseMove(viewport, pointAt(view, (from + to) / 2));
        QTest::mouseMove(viewport, pointAt(view, to));
        QTest::mouseRelease(viewport, Qt::LeftButton, {}, pointAt(view, to));
    }

private Q_SLOTS:
    void a_negative_cutoff_counts_from_the_offset_and_a_positive_one_from_the_end() {
        QCOMPARE(at(entry(), Value::Offset), 100.0);
        QCOMPARE(at(entry(), Value::Overlap), 120.0);
        QCOMPARE(at(entry(), Value::PreUtterance), 180.0);
        QCOMPARE(at(entry(), Value::Consonant), 150.0);
        QCOMPARE(at(entry(), Value::Cutoff), 400.0);
        QCOMPARE(at(entry(200), Value::Cutoff), 800.0);
        QCOMPARE(at(entry(0), Value::Cutoff), 1000.0);
    }

    // As in UTAU and OpenUtau, the other values stay at their times.
    void moving_the_offset_keeps_the_other_values_in_place() {
        const auto moved = OtoWaveformView::moved(entry(), Value::Offset, 60, 1000);
        QCOMPARE(moved.offset, 60.0);
        QCOMPARE(moved.consonant, 90.0);
        QCOMPARE(moved.preUtterance, 120.0);
        QCOMPARE(moved.voiceOverlap, 60.0);
        QCOMPARE(moved.cutoff, -340.0);
        QCOMPARE(OtoWaveformView::moved(entry(200), Value::Offset, 60, 1000).cutoff, 200.0);
        QCOMPARE(OtoWaveformView::moved(entry(0), Value::Offset, 60, 1000).cutoff, 0.0);

        // Past the end of the consonant, which moves along
        const auto past = OtoWaveformView::moved(entry(), Value::Offset, 170, 1000);
        QCOMPARE(past.consonant, 0.0);
        QCOMPARE(past.preUtterance, 10.0);
        QCOMPARE(past.voiceOverlap, -50.0);
        QCOMPARE(past.cutoff, -230.0);

        // Clamped to the audio
        QCOMPARE(OtoWaveformView::moved(entry(), Value::Offset, -20, 1000).offset, 0.0);
    }

    // The sign of the cutoff is kept, and a cutoff of zero becomes negative once moved.
    void the_cutoff_keeps_its_sign() {
        QCOMPARE(OtoWaveformView::moved(entry(), Value::Cutoff, 500, 1000).cutoff, -400.0);
        QCOMPARE(OtoWaveformView::moved(entry(200), Value::Cutoff, 500, 1000).cutoff, 500.0);
        QCOMPARE(OtoWaveformView::moved(entry(0), Value::Cutoff, 500, 1000).cutoff, -400.0);
        QCOMPARE(OtoWaveformView::moved(entry(200), Value::Cutoff, 1200, 1000).cutoff, 0.0);

        // Not before the end of the consonant, and a negative one never zero
        QCOMPARE(OtoWaveformView::moved(entry(), Value::Cutoff, 120, 1000).cutoff, -50.0);
        auto bare = entry();
        bare.consonant = 0;
        QCOMPARE(OtoWaveformView::moved(bare, Value::Cutoff, 50, 1000).cutoff, -1.0);

        // The consonant pushes the cutoff along.
        const auto pushed = OtoWaveformView::moved(entry(), Value::Consonant, 450, 1000);
        QCOMPARE(pushed.consonant, 350.0);
        QCOMPARE(pushed.cutoff, -350.0);
        QCOMPARE(OtoWaveformView::moved(entry(200), Value::Consonant, 900, 1000).cutoff, 100.0);
        QCOMPARE(OtoWaveformView::moved(entry(), Value::Consonant, 20, 1000).consonant, 0.0);
    }

    void moved_values_keep_the_digits_of_their_terms() {
        auto value = entry();
        value.offset = 10.1;
        value.preUtterance = 20.2;
        const auto moved = OtoWaveformView::moved(value, Value::Offset, 5, 1000);
        QCOMPARE(moved.preUtterance, 25.3);
        QCOMPARE(OtoWaveformView::moved(value, Value::PreUtterance, 40, 1000).preUtterance, 29.9);
    }

    // A drag changes the view and is emitted once released; Escape abandons it.
    void a_drag_moves_a_value_and_escape_abandons_it() {
        OtoWaveformView view;
        show(view);
        QSignalSpy edited(&view, &OtoWaveformView::entryEdited);

        drag(view, 180, 300);
        QCOMPARE(edited.size(), 1);
        const auto first = edited.at(0).at(0).value<kit::VoiceOtoEntry>();
        QCOMPARE(first.preUtterance, 200.0);
        QCOMPARE(first.offset, 100.0);
        QCOMPARE(view.entry()->preUtterance, 200.0);

        // The cutoff at 400, abandoned midway
        const auto viewport = view.viewport();
        QTest::mousePress(viewport, Qt::LeftButton, {}, pointAt(view, 400));
        QTest::mouseMove(viewport, pointAt(view, 600));
        QCOMPARE(view.entry()->cutoff, -500.0);
        QCOMPARE(view.activeValue(), std::optional<Value>(Value::Cutoff));
        QTest::keyClick(&view, Qt::Key_Escape);
        QCOMPARE(view.entry()->cutoff, -300.0);
        QTest::mouseRelease(viewport, Qt::LeftButton, {}, pointAt(view, 600));
        QCOMPARE(edited.size(), 1);

        // Off every boundary, nothing is dragged.
        drag(view, 700, 800);
        QCOMPARE(edited.size(), 1);
        QCOMPARE(*view.entry(), first);
    }

    void the_boundary_taken_is_the_nearest_within_the_grip() {
        OtoWaveformView view;
        show(view);
        const auto viewport = view.viewport();
        QTest::mouseMove(viewport, QPoint(int(view.xOf(150)) + 4, 100));
        QCOMPARE(view.activeValue(), std::optional<Value>(Value::Consonant));
        QTest::mouseMove(viewport, QPoint(int(view.xOf(150)) + 9, 100));
        QCOMPARE(view.activeValue(), std::optional<Value>());
        QVERIFY(view.pointerTime().has_value());

        // Of two within the grip, the nearer: the pre-utterance at 153 before the consonant
        auto close = entry();
        close.preUtterance = 53;
        view.setEntry(close);
        QTest::mouseMove(viewport, QPointF(view.xOf(152), 100).toPoint());
        QCOMPARE(view.activeValue(), std::optional<Value>(Value::PreUtterance));
        QTest::mouseMove(viewport, QPointF(view.xOf(150.5), 100).toPoint());
        QCOMPARE(view.activeValue(), std::optional<Value>(Value::Consonant));

        // Without audio, no values are shown or taken.
        view.setAudio(nullptr);
        QTest::mouseMove(viewport, pointAt(view, 150));
        QCOMPARE(view.activeValue(), std::optional<Value>());
        QVERIFY(!view.setValueAt(Value::Offset, 50));
    }

    void a_value_is_set_at_a_time() {
        OtoWaveformView view;
        show(view);
        QSignalSpy edited(&view, &OtoWaveformView::entryEdited);
        QVERIFY(view.setValueAt(Value::Overlap, 90.4));
        QCOMPARE(edited.size(), 1);
        QCOMPARE(view.entry()->voiceOverlap, -10.0);
        QVERIFY(!view.setValueAt(Value::Overlap, 90));
        QCOMPARE(edited.size(), 1);
    }

    // Zooming keeps the time under the pointer; a double click off the boundaries asks to
    // play from there.
    void the_view_zooms_about_the_pointer() {
        OtoWaveformView view;
        show(view);
        const double fitted = view.scale();
        QCOMPARE(view.viewStart(), 0.0);
        QVERIFY(qAbs(view.xOf(1000) - view.viewport()->width()) < 1);

        const double time = view.timeAt(300);
        view.zoom(4, 300);
        QCOMPARE(view.scale(), fitted * 4);
        QVERIFY(qAbs(view.timeAt(300) - time) < 0.01);
        view.zoom(0.01, 300);
        QCOMPARE(view.scale(), fitted);
        QCOMPARE(view.viewStart(), 0.0);

        QSignalSpy play(&view, &OtoWaveformView::playRequested);
        QTest::mouseDClick(view.viewport(), Qt::LeftButton, {}, pointAt(view, 700));
        QCOMPARE(play.size(), 1);
        QVERIFY(qAbs(play.at(0).at(0).toDouble() - 700) < 1);
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_OtoWaveformView test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_OtoWaveformView.moc"
