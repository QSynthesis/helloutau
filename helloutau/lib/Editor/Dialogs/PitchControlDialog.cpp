#include "PitchControlDialog.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include <QtCore/QSignalBlocker>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>

namespace hello::daw {

    namespace {

        // The vibrato values in the order of VBR, which is that of the fields
        constexpr double kit::Vibrato::*vibratoValues[] = {
            &kit::Vibrato::length, &kit::Vibrato::period,  &kit::Vibrato::amplitude,
            &kit::Vibrato::attack, &kit::Vibrato::release, &kit::Vibrato::phase,
            &kit::Vibrato::offset,
        };

        // The bound of the fields in milliseconds and percent
        constexpr int maximumValue = 100000;

        // The vibrato presets, which set the length, the period and the depth
        const struct {
            const char *name;
            double length;
            double period;
            double amplitude;
        } vibratoPresets[] = {
            {QT_TRANSLATE_NOOP("hello::daw::PitchControlDialog", "Default (65, 180, 35)"), 65, 180,
             35},
            {QT_TRANSLATE_NOOP("hello::daw::PitchControlDialog", "Deep (65, 210, 55)"),    65, 210,
             55},
            {QT_TRANSLATE_NOOP("hello::daw::PitchControlDialog", "Light (65, 165, 20)"),   65, 165,
             20},
        };

        // The portamento presets are listed by length, each at the three positions.
        constexpr int positionCount = 3;

        int presetIndexOf(const kit::PortamentoSettings &settings) {
            const auto &lengths = kit::PortamentoSettings::presetLengths;
            const auto length =
                std::find(std::begin(lengths), std::end(lengths), settings.presetLength);
            const int index =
                int(length - std::begin(lengths)) * positionCount + int(settings.position);
            return std::clamp(index, 0, int(std::size(lengths)) * positionCount - 1);
        }

        // Returns a row of a spin box and a slider of the same value. The slider covers the
        // usual range, and the spin box the whole range.
        QWidget *sliderRow(QWidget *box, QSlider *slider) {
            auto row = new QWidget();
            auto layout = new QHBoxLayout(row);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->addWidget(box);
            layout->addWidget(slider, 1);
            return row;
        }

        // Keeps slider at the value of box, within the range of the slider, and box at the
        // value of slider.
        template <class Box>
        void follow(Box *box, QSlider *slider) {
            QObject::connect(box, &Box::valueChanged, slider, [slider](auto value) {
                const QSignalBlocker blocker(slider);
                slider->setValue(std::clamp(int(value), slider->minimum(), slider->maximum()));
            });
            QObject::connect(slider, &QSlider::valueChanged, box,
                             [box](int value) { box->setValue(value); });
        }

        // Returns the check box of a group, partially checked if the notes differ. A click on a
        // partially checked box checks it.
        QCheckBox *groupBox(const QString &text, std::optional<bool> state) {
            auto box = new QCheckBox(text);
            box->setTristate(true);
            box->setCheckState(state ? (*state ? Qt::Checked : Qt::Unchecked)
                                     : Qt::PartiallyChecked);
            QObject::connect(box, &QCheckBox::checkStateChanged, box, [box](Qt::CheckState now) {
                if (now == Qt::PartiallyChecked) {
                    box->setCheckState(Qt::Checked);
                }
            });
            return box;
        }

    }

    PitchControlDialog::PitchControlDialog(const Selection &selection, QWidget *parent)
        : QDialog(parent), m_vibrato(selection.vibratoValues), m_points(selection.points),
          m_duration(std::max(0.0, selection.duration)),
          m_initialPortamento(selection.portamentoSettings) {
        setWindowTitle(tr("Pitch Control (Mode2)"));
        const auto &portamento = selection.portamentoSettings;

        m_portamento = groupBox(tr("&Portamento"), selection.portamento);
        m_portamentoPreset = new QComboBox();
        m_portamentoPreset->addItems({tr("Center - 50 ms"), tr("Left - 50 ms"), tr("Right - 50 ms"),
                                      tr("Center - 100 ms"), tr("Left - 100 ms"),
                                      tr("Right - 100 ms"), tr("Center - 200 ms"),
                                      tr("Left - 200 ms"), tr("Right - 200 ms")});
        m_portamentoPreset->setCurrentIndex(presetIndexOf(portamento));
        m_portamentoDefault = new QPushButton(tr("Set as Default"));
        m_portamentoPresetMode = new QRadioButton(tr("&Preset"));
        m_portamentoCustom = new QRadioButton(tr("&Custom"));
        m_portamentoAddPoints = new QRadioButton(tr("Add control &points"));
        m_portamentoPresetMode->setChecked(portamento.mode == kit::PortamentoSettings::Preset);
        m_portamentoCustom->setChecked(portamento.mode == kit::PortamentoSettings::Custom);
        m_portamentoAddPoints->setChecked(portamento.mode == kit::PortamentoSettings::AddPoints);

        // The custom portamento starts within the previous note or this note.
        m_portamentoLength = new QSpinBox();
        m_portamentoLength->setRange(0, maximumValue);
        m_portamentoLength->setValue(std::clamp(portamento.length, 0, maximumValue));
        m_portamentoLength->setSuffix(tr(" ms"));
        m_portamentoLengthSlider = new QSlider(Qt::Horizontal);
        m_portamentoLengthSlider->setRange(20, 320);
        m_portamentoStart = new QSpinBox();
        const double before =
            std::min<double>(maximumValue, selection.previousDuration.value_or(maximumValue));
        m_portamentoStart->setRange(-int(before), int(std::min<double>(maximumValue, m_duration)));
        m_portamentoStart->setValue(std::clamp(portamento.start, m_portamentoStart->minimum(),
                                               m_portamentoStart->maximum()));
        m_portamentoStart->setSuffix(tr(" ms"));
        m_portamentoStartSlider = new QSlider(Qt::Horizontal);
        m_portamentoStartSlider->setRange(-200, 200);
        m_portamentoLengthSlider->setValue(m_portamentoLength->value());
        m_portamentoStartSlider->setValue(m_portamentoStart->value());
        follow(m_portamentoLength, m_portamentoLengthSlider);
        follow(m_portamentoStart, m_portamentoStartSlider);

        // At least as many points as the first note has
        m_portamentoCount = new QComboBox();
        const int maximumCount = std::max(6, int(m_points.size()));
        for (int count = 2; count <= maximumCount; ++count) {
            m_portamentoCount->addItem(QString::number(count), count);
        }
        m_portamentoCount->setCurrentIndex(std::clamp(portamento.count - 2, 0, maximumCount - 2));
        m_averagePoints = new QCheckBox(tr("&Evenly distribute"));
        m_averagePoints->setChecked(portamento.evenlyDistributed);

        m_vibratoBox = groupBox(tr("&Vibrato"), selection.vibrato);
        m_vibratoPreset = new QComboBox();
        for (const auto &preset : vibratoPresets) {
            m_vibratoPreset->addItem(tr(preset.name));
        }
        m_vibratoPreset->setCurrentIndex(
            std::clamp(selection.vibratoPreset, 0, int(std::size(vibratoPresets)) - 1));
        m_vibratoDefault = new QPushButton(tr("Set as Default"));

        auto form = new QFormLayout();
        form->addRow(m_portamento);
        auto presetRow = new QWidget();
        auto presetLayout = new QHBoxLayout(presetRow);
        presetLayout->setContentsMargins(0, 0, 0, 0);
        presetLayout->addWidget(m_portamentoPreset);
        presetLayout->addWidget(m_portamentoDefault);
        form->addRow(m_portamentoPresetMode, presetRow);
        form->addRow(m_portamentoCustom);
        form->addRow(tr("Length:"), sliderRow(m_portamentoLength, m_portamentoLengthSlider));
        form->addRow(tr("Start:"), sliderRow(m_portamentoStart, m_portamentoStartSlider));
        form->addRow(m_portamentoAddPoints);
        form->addRow(tr("Count:"), m_portamentoCount);
        form->addRow(m_averagePoints);
        auto separator = new QFrame();
        separator->setFrameShape(QFrame::HLine);
        separator->setFrameShadow(QFrame::Sunken);
        form->addRow(separator);
        form->addRow(m_vibratoBox);
        auto vibratoPresetRow = new QWidget();
        auto vibratoPresetLayout = new QHBoxLayout(vibratoPresetRow);
        vibratoPresetLayout->setContentsMargins(0, 0, 0, 0);
        vibratoPresetLayout->addWidget(m_vibratoPreset);
        vibratoPresetLayout->addWidget(m_vibratoDefault);
        form->addRow(tr("Vibrato preset:"), vibratoPresetRow);

        // The fields of the vibrato. The first three have a slider over the usual range.
        const struct {
            const char *label;
            double minimum;
            double maximum;
            QString suffix;
            int sliderMinimum;
            int sliderMaximum;
        } fields[] = {
            {QT_TR_NOOP("&Length:"),   0,             200,          QStringLiteral(" %"),  10, 100},
            {QT_TR_NOOP("&Period:"),   1,             2000,         QStringLiteral(" ms"), 64, 512},
            {QT_TR_NOOP("&Depth:"),    0,             500,          tr(" cents"),          5,  200},
            {QT_TR_NOOP("Fade &in:"),  0,             100,          QStringLiteral(" %"),  0,  0  },
            {QT_TR_NOOP("Fade &out:"), 0,             100,          QStringLiteral(" %"),  0,  0  },
            {QT_TR_NOOP("P&hase:"),    -maximumValue, maximumValue, QStringLiteral(" %"),  0,  0  },
            {QT_TR_NOOP("&Height:"),   -maximumValue, maximumValue, QStringLiteral(" %"),  0,  0  },
        };
        for (size_t i = 0; i < m_fields.size(); ++i) {
            auto box = new QDoubleSpinBox();
            box->setDecimals(2);
            box->setRange(fields[i].minimum, fields[i].maximum);
            box->setSuffix(fields[i].suffix);
            box->setValue(m_vibrato.*vibratoValues[i]);
            m_fields[i] = box;
            m_shown[i] = box->value();
            if (i >= m_vibratoSliders.size()) {
                form->addRow(tr(fields[i].label), box);
                continue;
            }
            auto slider = new QSlider(Qt::Horizontal);
            slider->setRange(fields[i].sliderMinimum, fields[i].sliderMaximum);
            slider->setValue(int(box->value()));
            m_vibratoSliders[i] = slider;
            follow(box, slider);
            form->addRow(tr(fields[i].label), sliderRow(box, slider));
        }
        // The eighth value of VBR, which UTAU ignores, is shown for compatibility only.
        auto strength = new QSpinBox();
        strength->setRange(-maximumValue, maximumValue);
        strength->setValue(int(m_vibrato.intensity));
        strength->setEnabled(false);
        form->addRow(tr("Strength:"), strength);

        connect(m_vibratoPreset, &QComboBox::currentIndexChanged, this, [this](int index) {
            if (index < 0 || index >= int(std::size(vibratoPresets))) {
                return;
            }
            const auto &preset = vibratoPresets[index];
            m_fields[0]->setValue(preset.length);
            m_fields[1]->setValue(preset.period);
            m_fields[2]->setValue(preset.amplitude);
        });
        const auto updateVibratoFields = [this] {
            const bool enabled = m_vibratoBox->checkState() == Qt::Checked;
            m_vibratoPreset->setEnabled(enabled);
            m_vibratoDefault->setEnabled(enabled);
            for (const auto field : m_fields) {
                field->setEnabled(enabled);
            }
            for (const auto slider : m_vibratoSliders) {
                slider->setEnabled(enabled);
            }
        };
        connect(m_vibratoBox, &QCheckBox::checkStateChanged, this, updateVibratoFields);
        updateVibratoFields();

        const auto updatePortamentoFields = [this] {
            const bool enabled = m_portamento->checkState() == Qt::Checked;
            const bool custom = m_portamentoCustom->isChecked();
            const bool add = m_portamentoAddPoints->isChecked();
            m_portamentoPresetMode->setEnabled(enabled);
            m_portamentoCustom->setEnabled(enabled);
            m_portamentoAddPoints->setEnabled(enabled);
            m_portamentoDefault->setEnabled(enabled);
            m_portamentoPreset->setEnabled(enabled && m_portamentoPresetMode->isChecked());
            m_portamentoLength->setEnabled(enabled && custom);
            m_portamentoStart->setEnabled(enabled && custom);
            m_portamentoCount->setEnabled(enabled && add);
            m_averagePoints->setEnabled(enabled && add);
        };
        for (const auto button :
             {m_portamentoPresetMode, m_portamentoCustom, m_portamentoAddPoints}) {
            connect(button, &QRadioButton::toggled, this, updatePortamentoFields);
        }
        connect(m_portamento, &QCheckBox::checkStateChanged, this, updatePortamentoFields);
        updatePortamentoFields();

        // The custom portamento ends within the note.
        const auto updatePortamentoBounds = [this] {
            const auto toEnd = [this](int from) { return int(std::lround(m_duration - from)); };
            m_portamentoLength->setMaximum(
                std::clamp(toEnd(m_portamentoStart->value()), 0, maximumValue));
            m_portamentoStart->setMaximum(
                std::clamp(toEnd(m_portamentoLength->value()), m_portamentoStart->minimum(),
                           int(std::min<double>(maximumValue, m_duration))));
        };
        connect(m_portamentoLength, &QSpinBox::valueChanged, this, updatePortamentoBounds);
        connect(m_portamentoStart, &QSpinBox::valueChanged, this, updatePortamentoBounds);
        updatePortamentoBounds();

        // After the bounds, which may have changed the custom portamento
        m_initialPortamento = portamentoSettings();
        m_initialPortamentoState = m_portamento->checkState();

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        auto layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(buttons);
    }

    PitchControlDialog::~PitchControlDialog() = default;

    Qt::CheckState PitchControlDialog::portamentoState() const {
        return m_portamento->checkState();
    }

    Qt::CheckState PitchControlDialog::vibratoState() const {
        return m_vibratoBox->checkState();
    }

    kit::Vibrato PitchControlDialog::vibrato() const {
        auto result = m_vibrato;
        for (size_t i = 0; i < m_fields.size(); ++i) {
            if (m_fields[i]->value() != m_shown[i]) {
                result.*vibratoValues[i] = m_fields[i]->value();
            }
        }
        return result;
    }

    bool PitchControlDialog::vibratoEdited() const {
        for (size_t i = 0; i < m_fields.size(); ++i) {
            if (m_fields[i]->value() != m_shown[i]) {
                return true;
            }
        }
        return false;
    }

    bool PitchControlDialog::portamentoEdited() const {
        return m_portamento->checkState() != m_initialPortamentoState ||
               portamentoSettings() != m_initialPortamento;
    }

    kit::PortamentoSettings PitchControlDialog::portamentoSettings() const {
        kit::PortamentoSettings settings;
        settings.mode = m_portamentoCustom->isChecked()      ? kit::PortamentoSettings::Custom
                        : m_portamentoAddPoints->isChecked() ? kit::PortamentoSettings::AddPoints
                                                             : kit::PortamentoSettings::Preset;
        const int preset = m_portamentoPreset->currentIndex();
        settings.position = kit::PortamentoSettings::Position(preset % positionCount);
        settings.presetLength = kit::PortamentoSettings::presetLengths[preset / positionCount];
        settings.start = m_portamentoStart->value();
        settings.length = m_portamentoLength->value();
        settings.count = m_portamentoCount->currentData().toInt();
        settings.evenlyDistributed = m_averagePoints->isChecked();
        return settings;
    }

    int PitchControlDialog::vibratoPreset() const {
        return m_vibratoPreset->currentIndex();
    }

    QList<kit::PortamentoPoint> PitchControlDialog::portamentoPoints() const {
        return portamentoSettings().pointsFor(m_points, m_duration);
    }

    QPushButton *PitchControlDialog::portamentoDefaultButton() const {
        return m_portamentoDefault;
    }

    QPushButton *PitchControlDialog::vibratoDefaultButton() const {
        return m_vibratoDefault;
    }

    QDoubleSpinBox *PitchControlDialog::field(int index) const {
        return m_fields.at(size_t(index));
    }

}
