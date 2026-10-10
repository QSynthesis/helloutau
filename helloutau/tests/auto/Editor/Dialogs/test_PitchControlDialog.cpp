#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSlider>

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

    template <class T>
    static T *childWithText(const QDialog &dialog, const QString &text) {
        for (const auto child : dialog.findChildren<T *>()) {
            if (child->text() == text) {
                return child;
            }
        }
        return nullptr;
    }

    // The two sliders of the portamento, found by their ranges, and the sliders of the vibrato
    static QList<QSlider *> slidersOf(const QDialog &dialog, bool portamento) {
        QList<QSlider *> result;
        for (const auto slider : dialog.findChildren<QSlider *>()) {
            const bool ofPortamento = (slider->minimum() == 20 && slider->maximum() == 320) ||
                                      (slider->minimum() == -200 && slider->maximum() == 200);
            if (ofPortamento == portamento) {
                result.push_back(slider);
            }
        }
        return result;
    }

    static QComboBox *comboWithItem(const QDialog &dialog, const QString &item) {
        for (const auto combo : dialog.findChildren<QComboBox *>()) {
            if (combo->findText(item) >= 0) {
                return combo;
            }
        }
        return nullptr;
    }

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

    // While the portamento is not checked, or checked partially, none of its controls is
    // enabled. Checked, only the controls of the selected way are enabled.
    void the_portamento_controls_follow_the_check_box_and_the_way() {
        for (const std::optional<bool> state :
             {std::optional<bool>(false), std::optional<bool>()}) {
            // The custom way, whose sliders are enabled while the portamento is checked
            kit::PortamentoSettings custom;
            custom.mode = kit::PortamentoSettings::Custom;
            auto selection = selectionOf(custom);
            selection.portamento = state;
            PitchControlDialog dialog(selection);
            QVERIFY(!childWithText<QRadioButton>(dialog, QStringLiteral("&Custom"))->isEnabled());
            QVERIFY(!comboWithItem(dialog, QStringLiteral("Center - 50 ms"))->isEnabled());
            QVERIFY(!dialog.portamentoDefaultButton()->isEnabled());
            for (const auto slider : slidersOf(dialog, true)) {
                QVERIFY(!slider->isEnabled());
            }
        }

        PitchControlDialog dialog(selectionOf(kit::PortamentoSettings()));
        const auto preset = comboWithItem(dialog, QStringLiteral("Center - 50 ms"));
        const auto count = comboWithItem(dialog, QStringLiteral("6"));
        const auto even = childWithText<QCheckBox>(dialog, QStringLiteral("&Evenly distribute"));
        QVERIFY(preset && count && even);
        QCOMPARE(slidersOf(dialog, true).size(), 2);
        QVERIFY(preset->isEnabled());
        QVERIFY(!count->isEnabled());
        for (const auto slider : slidersOf(dialog, true)) {
            QVERIFY(!slider->isEnabled());
        }

        childWithText<QRadioButton>(dialog, QStringLiteral("&Custom"))->setChecked(true);
        QVERIFY(!preset->isEnabled());
        for (const auto slider : slidersOf(dialog, true)) {
            QVERIFY(slider->isEnabled());
        }

        childWithText<QRadioButton>(dialog, QStringLiteral("Add control &points"))
            ->setChecked(true);
        QVERIFY(count->isEnabled());
        QVERIFY(even->isEnabled());
        for (const auto slider : slidersOf(dialog, true)) {
            QVERIFY(!slider->isEnabled());
        }
    }

    // The fields, the sliders and the preset of the vibrato are enabled only while it is
    // checked.
    void the_vibrato_controls_follow_the_check_box() {
        for (const std::optional<bool> state :
             {std::optional<bool>(false), std::optional<bool>()}) {
            auto selection = selectionOf(kit::PortamentoSettings());
            selection.vibrato = state;
            PitchControlDialog dialog(selection);
            QVERIFY(!dialog.field(0)->isEnabled());
            QVERIFY(!dialog.vibratoDefaultButton()->isEnabled());
            QCOMPARE(slidersOf(dialog, false).size(), 3);
            for (const auto slider : slidersOf(dialog, false)) {
                QVERIFY(!slider->isEnabled());
            }
            childWithText<QCheckBox>(dialog, QStringLiteral("&Vibrato"))->setChecked(true);
            QVERIFY(dialog.field(0)->isEnabled());
            QVERIFY(dialog.vibratoDefaultButton()->isEnabled());
            for (const auto slider : slidersOf(dialog, false)) {
                QVERIFY(slider->isEnabled());
            }
        }
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
