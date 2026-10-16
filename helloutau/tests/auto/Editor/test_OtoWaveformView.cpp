#include <cmath>

#include <QtCore/QtMath>
#include <QtGui/QWheelEvent>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <helloutau/Editor/OtoWaveformKeyboardModifiers.h>
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

    // A drag changes the view and is emitted once released. Escape abandons it.
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

    // The curve of a frequency map on the pitch axis, an octave of room below and above,
    // with a gap where the frames are unvoiced
    void the_frequency_curve_is_drawn() {
        OtoWaveformView view;
        show(view);
        view.setFrequencyColor(QColor(255, 0, 255));
        QCOMPARE(view.pitchRange(), (std::pair<double, double>(36, 84)));
        kit::FrequencyMap table;
        for (int time = 0; time <= 1000; time += 5) {
            table.frames.push_back({double(time), time >= 290 && time <= 310 ? 0.0 : 440.0, {}});
        }
        view.setFrequencyMap(table);
        QCOMPARE(view.pitchRange(), (std::pair<double, double>(57, 81)));
        QCOMPARE(view.frequencyMap()->frames.size(), table.frames.size());

        const auto drawnAt = [&view](double time, double note) {
            const auto image = view.viewport()->grab().toImage();
            const QPoint at(int(view.xOf(time)), int(view.yOfNote(note)));
            for (int dy = -2; dy <= 2; ++dy) {
                const auto pixel = image.pixelColor(at + QPoint(0, dy));
                if (pixel.red() > 180 && pixel.green() < 120 && pixel.blue() > 180) {
                    return true;
                }
            }
            return false;
        };
        QVERIFY(drawnAt(600, 69));
        QVERIFY(!drawnAt(300, 69));
        QVERIFY(!drawnAt(600, 75));
        view.setFrequencyMap(std::nullopt);
        QVERIFY(!drawnAt(600, 69));
    }

    // Each channel is drawn in a lane of its own, the first at the top: the first channel above
    // the zero line of its lane and the second below that of its lane. The samples alternate
    // with 0 at 8000 Hz, so that each column spans from 0 to the peak.
    void each_channel_has_a_lane() {
        auto audio = std::make_shared<kit::WaveAudio>();
        audio->sampleRate = 8000;
        audio->channels = 2;
        for (int i = 0; i < 8000; ++i) {
            audio->samples.push_back(i % 2 == 0 ? 0.5f : 0.0f);
            audio->samples.push_back(i % 2 == 0 ? -0.5f : 0.0f);
        }
        OtoWaveformView view;
        view.resize(1040, 300);
        view.show();
        view.setWaveColor(QColor(0, 255, 0));
        view.setAudio(audio);
        QVERIFY(view.yOfSample(0, -1) < view.yOfSample(1, 1));

        const auto image = view.viewport()->grab().toImage();
        const auto drawn = [&](int channel, double sample) {
            const auto pixel =
                image.pixelColor(QPoint(int(view.xOf(500)), int(view.yOfSample(channel, sample))));
            return pixel.green() > 200 && pixel.red() < 100;
        };
        QVERIFY(drawn(0, 0.25));
        QVERIFY(!drawn(0, -0.25));
        QVERIFY(drawn(1, -0.25));
        QVERIFY(!drawn(1, 0.25));
    }

    // The spectrogram in place of the waveform: a sine is bright at its pitch.
    void the_spectrogram_shows_the_pitch() {
        auto audio = std::make_shared<kit::WaveAudio>();
        audio->sampleRate = 8000;
        audio->channels = 1;
        for (int i = 0; i < 8000; ++i) {
            audio->samples.push_back(float(0.5 * std::sin(2 * M_PI * 440 * i / 8000)));
        }
        OtoWaveformView view;
        view.resize(1040, 300);
        view.show();
        view.setAudio(audio);
        view.setSpectrumColor(QColor(0, 0, 255));
        view.setSpectrogram(std::make_shared<const kit::Spectrogram>(kit::Spectrogram::of(*audio)));
        QVERIFY(view.spectrogram());
        const auto image = view.viewport()->grab().toImage();
        const auto at = [&](double note) {
            return image.pixelColor(QPoint(int(view.xOf(500)), int(view.yOfNote(note))));
        };
        // Blue where the sine is, the background an octave above
        QVERIFY(at(69).red() < 100);
        QVERIFY(at(81).red() > 200);
    }

    // Zooming keeps the time under the pointer. A double click off the boundaries requests
    // playback from there.
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

    // By default the wheel zooms with Ctrl and scrolls without modifiers. Other bindings apply
    // once set, and modifiers that no role has do nothing.
    void the_wheel_follows_the_bindings() {
        OtoWaveformView view;
        show(view);
        const auto wheel = [&view](Qt::KeyboardModifiers modifiers) {
            const QPointF at(300, 100);
            QWheelEvent event(at, view.viewport()->mapToGlobal(at), QPoint(), QPoint(0, 120),
                              Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
            QApplication::sendEvent(view.viewport(), &event);
        };
        const double fitted = view.scale();
        wheel(Qt::ControlModifier);
        QVERIFY(view.scale() > fitted);
        double start = view.viewStart();
        QVERIFY(start > 0);
        wheel(Qt::NoModifier);
        QVERIFY(view.viewStart() < start);

        KeyboardModifierBindings bindings(OtoWaveformKeyboardModifiers::scheme());
        bindings.setModifiers(OtoWaveformKeyboardModifiers::TimeZoom, Qt::AltModifier);
        view.setKeyboardModifierBindings(bindings);
        const double scale = view.scale();
        start = view.viewStart();
        wheel(Qt::ControlModifier);
        QCOMPARE(view.scale(), scale);
        QCOMPARE(view.viewStart(), start);
        wheel(Qt::AltModifier);
        QVERIFY(view.scale() > scale);
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
