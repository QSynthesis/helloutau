#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDoubleSpinBox>

#include <helloutau/Editor/PitchControlDialog.h>

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
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_PitchControlDialog test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_PitchControlDialog.moc"
