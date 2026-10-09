#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDoubleSpinBox>

#include <helloutau/Editor/Dialogs/PitchControlDialog.h>

using namespace hello;
using namespace hello::daw;

class test_PitchControlDialog : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void mixed_controls_are_shown_as_partial() {
        PitchControlDialog dialog(std::nullopt, std::nullopt, PitchControlDialog::defaultVibrato());
        QCOMPARE(dialog.portamentoState(), Qt::PartiallyChecked);
        QCOMPARE(dialog.vibratoState(), Qt::PartiallyChecked);
    }

    // The values are shown in the order of VBR, and those left alone keep every decimal, the
    // eighth value included.
    void only_the_changed_values_are_written() {
        kit::Vibrato vibrato;
        vibrato.length = 65.125;
        vibrato.period = 180;
        vibrato.amplitude = 35;
        vibrato.attack = 20;
        vibrato.release = 20;
        vibrato.phase = 0.001;
        vibrato.offset = -12;
        vibrato.intensity = 7;

        PitchControlDialog dialog(true, true, vibrato);
        QCOMPARE(dialog.field(0)->value(), 65.13);
        QCOMPARE(dialog.field(6)->value(), -12.0);
        QCOMPARE(dialog.vibrato(), vibrato);

        dialog.field(1)->setValue(200);
        auto expected = vibrato;
        expected.period = 200;
        QCOMPARE(dialog.vibrato(), expected);
    }

    void the_default_is_that_of_utau() {
        const auto vibrato = PitchControlDialog::defaultVibrato();
        QCOMPARE(vibrato.length, 65.0);
        QCOMPARE(vibrato.period, 180.0);
        QCOMPARE(vibrato.amplitude, 35.0);
        QCOMPARE(vibrato.attack, 20.0);
        QCOMPARE(vibrato.release, 20.0);
        QCOMPARE(vibrato.phase, 0.0);
        QCOMPARE(vibrato.offset, 0.0);
    }

    void portamento_modes_and_values_are_retained() {
        PitchControlDialog dialog(true, true, PitchControlDialog::defaultVibrato(), 4, 2, 2, 80,
                                  -40, 5, false);
        QCOMPARE(dialog.portamentoPreset(), 4);
        QCOMPARE(dialog.vibratoPreset(), 2);
        QCOMPARE(dialog.portamentoMode(), 2);
        QCOMPARE(dialog.portamentoLength(), 80);
        QCOMPARE(dialog.portamentoStart(), -40);
        QCOMPARE(dialog.portamentoCount(), 5);
        QCOMPARE(dialog.averagePoints(), false);

        const auto points = dialog.portamentoPoints();
        QCOMPARE(points.size(), 5);
        QCOMPARE(points.first().x, -100.0);
        QCOMPARE(points.last().x, 0.0);
    }

    void custom_portamento_uses_length_and_start() {
        PitchControlDialog dialog(true, true, PitchControlDialog::defaultVibrato(), 0, 0, 1, 72,
                                  -18, 2, true);
        const auto points = dialog.portamentoPoints();
        QCOMPARE(points.size(), 2);
        QCOMPARE(points.first().x, -18.0);
        QCOMPARE(points.last().x, 54.0);
    }

    void averaged_points_cover_the_note_after_the_first_point() {
        const QList<kit::PortamentoPoint> existing = {
            {-40, -100, kit::PortamentoPoint::S},
            {40,  100,  kit::PortamentoPoint::S},
        };
        PitchControlDialog dialog(true, true, PitchControlDialog::defaultVibrato(), 0, 0, 2, 59,
                                  -30, 3, true, existing, 160);
        const auto points = dialog.portamentoPoints();
        QCOMPARE(points.size(), 3);
        QCOMPARE(points[0].x, -40.0);
        QCOMPARE(points[1].x, 60.0);
        QCOMPARE(points[2].x, 160.0);
        QCOMPARE(points[1].y, 100.0);
        QCOMPARE(points[2].y, 100.0);
    }

    void non_averaged_existing_points_are_not_replaced() {
        const QList<kit::PortamentoPoint> existing = {
            {-40, -100, kit::PortamentoPoint::S     },
            {0,   40,   kit::PortamentoPoint::Linear},
            {80,  0,    kit::PortamentoPoint::S     },
        };
        PitchControlDialog dialog(true, true, PitchControlDialog::defaultVibrato(), 0, 0, 2, 59,
                                  -30, 3, false, existing, 160);
        QCOMPARE(dialog.portamentoPoints(), existing);
    }

    void existing_seven_points_are_retained_without_portamento_edits() {
        QList<kit::PortamentoPoint> existing;
        for (int i = 0; i < 7; ++i) {
            existing.push_back({double(i * 20), double(i), kit::PortamentoPoint::S});
        }
        PitchControlDialog dialog(true, true, PitchControlDialog::defaultVibrato(), 0, 0, 2, 59,
                                  -30, 7, true, existing, 120);
        QCOMPARE(dialog.portamentoCount(), 7);
        QVERIFY(!dialog.portamentoEdited());
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_PitchControlDialog test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_PitchControlDialog.moc"
