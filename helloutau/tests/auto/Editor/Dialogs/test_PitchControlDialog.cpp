#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDoubleSpinBox>

#include <helloutau/Editor/Dialogs/PitchControlDialog.h>

using namespace hello;
using namespace hello::daw;

namespace {

    // A selection whose notes all have portamento and vibrato, the first of 480 milliseconds
    PitchControlDialog::Selection selectionOf(const kit::PortamentoSettings &settings,
                                              const QList<kit::PortamentoPoint> &points = {},
                                              double duration = 480) {
        PitchControlDialog::Selection selection;
        selection.portamento = true;
        selection.vibrato = true;
        selection.portamentoSettings = settings;
        selection.points = points;
        selection.duration = duration;
        return selection;
    }

}

class test_PitchControlDialog : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void mixed_controls_are_shown_as_partial() {
        PitchControlDialog dialog(PitchControlDialog::Selection{});
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

        auto selection = selectionOf({});
        selection.vibratoValues = vibrato;
        PitchControlDialog dialog(selection);
        QCOMPARE(dialog.field(0)->value(), 65.13);
        QCOMPARE(dialog.field(6)->value(), -12.0);
        QCOMPARE(dialog.vibrato(), vibrato);

        dialog.field(1)->setValue(200);
        auto expected = vibrato;
        expected.period = 200;
        QCOMPARE(dialog.vibrato(), expected);
    }

    void portamento_modes_and_values_are_retained() {
        kit::PortamentoSettings settings;
        settings.mode = kit::PortamentoSettings::AddPoints;
        settings.position = kit::PortamentoSettings::Left;
        settings.presetLength = 100;
        settings.length = 80;
        settings.start = -40;
        settings.count = 5;
        settings.evenlyDistributed = false;
        auto selection = selectionOf(settings);
        selection.vibratoPreset = 2;
        PitchControlDialog dialog(selection);
        QCOMPARE(dialog.portamentoSettings(), settings);
        QCOMPARE(dialog.vibratoPreset(), 2);

        const auto points = dialog.portamentoPoints();
        QCOMPARE(points.size(), 5);
        QCOMPARE(points.first().x, -100.0);
        QCOMPARE(points.last().x, 0.0);
    }

    void custom_portamento_uses_length_and_start() {
        kit::PortamentoSettings settings;
        settings.mode = kit::PortamentoSettings::Custom;
        settings.length = 72;
        settings.start = -18;
        PitchControlDialog dialog(selectionOf(settings));
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
        kit::PortamentoSettings settings;
        settings.mode = kit::PortamentoSettings::AddPoints;
        settings.count = 3;
        PitchControlDialog dialog(selectionOf(settings, existing, 160));
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
        kit::PortamentoSettings settings;
        settings.mode = kit::PortamentoSettings::AddPoints;
        settings.count = 3;
        settings.evenlyDistributed = false;
        PitchControlDialog dialog(selectionOf(settings, existing, 160));
        QCOMPARE(dialog.portamentoPoints(), existing);
    }

    void existing_seven_points_are_retained_without_portamento_edits() {
        QList<kit::PortamentoPoint> existing;
        for (int i = 0; i < 7; ++i) {
            existing.push_back({double(i * 20), double(i), kit::PortamentoPoint::S});
        }
        kit::PortamentoSettings settings;
        settings.mode = kit::PortamentoSettings::AddPoints;
        settings.count = 7;
        PitchControlDialog dialog(selectionOf(settings, existing, 120));
        QCOMPARE(dialog.portamentoSettings().count, 7);
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
