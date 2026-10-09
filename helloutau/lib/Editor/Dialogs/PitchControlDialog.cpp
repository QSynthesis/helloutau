#include "PitchControlDialog.h"

#include <algorithm>
#include <QtCore/QSignalBlocker>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/Document/PortamentoSettings.h>

namespace hello::daw {

    namespace {

        constexpr double kit::Vibrato::*Values[] = {
            &kit::Vibrato::length, &kit::Vibrato::period,  &kit::Vibrato::amplitude,
            &kit::Vibrato::attack, &kit::Vibrato::release, &kit::Vibrato::phase,
            &kit::Vibrato::offset,
        };

    }

    PitchControlDialog::PitchControlDialog(
        std::optional<bool> portamento, std::optional<bool> vibrato, const kit::Vibrato &values,
        int portamentoPreset, int vibratoPreset, int portamentoMode, int portamentoLength,
        int portamentoStart, int portamentoCount, bool averagePoints,
        const QList<kit::PortamentoPoint> &existingPortamento, double noteDuration,
        double previousNoteDuration, QWidget *parent)
        : QDialog(parent), m_vibrato(values), m_existingPortamento(existingPortamento),
          m_noteDuration(std::max(0.0, noteDuration)),
          m_previousNoteDuration(std::max(0.0, previousNoteDuration)) {
        setWindowTitle(tr("Pitch Control (Mode2)"));
        m_portamento = new QCheckBox(tr("&Portamento"));
        m_portamento->setTristate(true);
        m_portamento->setCheckState(portamento ? (*portamento ? Qt::Checked : Qt::Unchecked)
                                               : Qt::PartiallyChecked);
        m_vibratoBox = new QCheckBox(tr("&Vibrato"));
        m_vibratoBox->setTristate(true);
        m_vibratoBox->setCheckState(vibrato ? (*vibrato ? Qt::Checked : Qt::Unchecked)
                                            : Qt::PartiallyChecked);
        const auto skipPartialOnClick = [](QCheckBox *box, Qt::CheckState state) {
            if (state == Qt::PartiallyChecked)
                box->setCheckState(Qt::Checked);
        };
        connect(m_portamento, &QCheckBox::checkStateChanged, this,
                [skipPartialOnClick, this](Qt::CheckState state) {
                    skipPartialOnClick(m_portamento, state);
                });
        connect(m_vibratoBox, &QCheckBox::checkStateChanged, this,
                [skipPartialOnClick, this](Qt::CheckState state) {
                    skipPartialOnClick(m_vibratoBox, state);
                });

        m_portamentoPreset = new QComboBox();
        m_portamentoPreset->addItems({tr("Center - 50 ms"), tr("Left - 50 ms"), tr("Right - 50 ms"),
                                      tr("Center - 100 ms"), tr("Left - 100 ms"),
                                      tr("Right - 100 ms"), tr("Center - 200 ms"),
                                      tr("Left - 200 ms"), tr("Right - 200 ms")});
        m_portamentoPreset->setCurrentIndex(std::clamp(portamentoPreset, 0, 8));
        m_portamentoPresetMode = new QRadioButton(tr("&Preset"));
        m_portamentoCustom = new QRadioButton(tr("&Custom"));
        m_portamentoAddPoints = new QRadioButton(tr("Add control &points"));
        m_portamentoPresetMode->setChecked(portamentoMode == 0);
        m_portamentoCustom->setChecked(portamentoMode == 1);
        m_portamentoAddPoints->setChecked(portamentoMode == 2);
        m_portamentoLength = new QSpinBox();
        m_portamentoLength->setRange(0, 100000);
        m_portamentoLength->setValue(std::clamp(portamentoLength, 0, 100000));
        m_portamentoLength->setSuffix(tr(" ms"));
        m_portamentoLengthSlider = new QSlider(Qt::Horizontal);
        m_portamentoLengthSlider->setRange(20, 320);
        m_portamentoLengthSlider->setValue(m_portamentoLength->value());
        m_portamentoStart = new QSpinBox();
        m_portamentoStart->setRange(-int(std::min(100000.0, m_previousNoteDuration)),
                                    int(std::min(100000.0, m_noteDuration)));
        m_portamentoStart->setValue(std::clamp(portamentoStart, m_portamentoStart->minimum(),
                                               m_portamentoStart->maximum()));
        m_portamentoStart->setSuffix(tr(" ms"));
        m_portamentoStartSlider = new QSlider(Qt::Horizontal);
        m_portamentoStartSlider->setRange(-200, 200);
        m_portamentoStartSlider->setValue(m_portamentoStart->value());
        m_portamentoCount = new QComboBox();
        const int maximumPortamentoCount = std::max(6, int(m_existingPortamento.size()));
        for (int i = 2; i <= maximumPortamentoCount; ++i)
            m_portamentoCount->addItem(QString::number(i));
        m_portamentoCount->setCurrentIndex(
            std::clamp(portamentoCount - 2, 0, maximumPortamentoCount - 2));
        m_averagePoints = new QCheckBox(tr("&Evenly distribute"));
        m_averagePoints->setChecked(averagePoints);
        m_vibratoPreset = new QComboBox();
        m_vibratoPreset->addItems(
            {tr("Default (65, 180, 35)"), tr("Deep (65, 210, 55)"), tr("Light (65, 165, 20)")});
        m_vibratoPreset->setCurrentIndex(std::clamp(vibratoPreset, 0, 2));
        m_portamentoDefault = new QPushButton(tr("Set as Default"));

        auto form = new QFormLayout();
        const struct {
            const char *label;
            double minimum;
            double maximum;
            QString suffix;
        } fields[] = {
            {QT_TR_NOOP("&Length:"),   0,       200,    QStringLiteral(" %") },
            {QT_TR_NOOP("&Period:"),   1,       2000,   QStringLiteral(" ms")},
            {QT_TR_NOOP("&Depth:"),    0,       500,    tr(" cents")         },
            {QT_TR_NOOP("Fade &in:"),  0,       100,    QStringLiteral(" %") },
            {QT_TR_NOOP("Fade &out:"), 0,       100,    QStringLiteral(" %") },
            {QT_TR_NOOP("P&hase:"),    -100000, 100000, QStringLiteral(" %") },
            {QT_TR_NOOP("&Height:"),   -100000, 100000, QStringLiteral(" %") },
        };
        form->addRow(m_portamento);
        auto portamentoPresetRow = new QWidget();
        auto portamentoPresetLayout = new QHBoxLayout(portamentoPresetRow);
        portamentoPresetLayout->setContentsMargins(0, 0, 0, 0);
        portamentoPresetLayout->addWidget(m_portamentoPreset);
        portamentoPresetLayout->addWidget(m_portamentoDefault);
        form->addRow(m_portamentoPresetMode, portamentoPresetRow);
        form->addRow(m_portamentoCustom);
        auto sliderRow = [](QSpinBox *spin, QSlider *slider) {
            auto row = new QWidget();
            auto layout = new QHBoxLayout(row);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->addWidget(spin);
            layout->addWidget(slider, 1);
            return row;
        };
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
        m_vibratoDefault = new QPushButton(tr("Set as Default"));
        auto vibratoPresetRow = new QWidget();
        auto vibratoPresetLayout = new QHBoxLayout(vibratoPresetRow);
        vibratoPresetLayout->setContentsMargins(0, 0, 0, 0);
        vibratoPresetLayout->addWidget(m_vibratoPreset);
        vibratoPresetLayout->addWidget(m_vibratoDefault);
        form->addRow(tr("Vibrato preset:"), vibratoPresetRow);
        for (size_t i = 0; i < m_fields.size(); ++i) {
            auto box = new QDoubleSpinBox();
            box->setDecimals(2);
            box->setRange(fields[i].minimum, fields[i].maximum);
            box->setSuffix(fields[i].suffix);
            box->setValue(values.*Values[i]);
            m_fields[i] = box;
            m_shown[i] = box->value();
            if (i < m_vibratoSliders.size()) {
                constexpr int sliderMinimum[] = {10, 64, 5};
                constexpr int sliderMaximum[] = {100, 512, 200};
                auto slider = new QSlider(Qt::Horizontal);
                slider->setRange(sliderMinimum[i], sliderMaximum[i]);
                slider->setValue(int(box->value()));
                m_vibratoSliders[i] = slider;
                auto row = new QWidget();
                auto layout = new QHBoxLayout(row);
                layout->setContentsMargins(0, 0, 0, 0);
                layout->addWidget(box);
                layout->addWidget(slider, 1);
                connect(box, &QDoubleSpinBox::valueChanged, slider, [slider](double value) {
                    const QSignalBlocker blocker(slider);
                    slider->setValue(std::clamp(int(value), slider->minimum(), slider->maximum()));
                });
                connect(slider, &QSlider::valueChanged, box,
                        [box](int value) { box->setValue(value); });
                form->addRow(tr(fields[i].label), row);
            } else {
                form->addRow(tr(fields[i].label), box);
            }
        }
        const auto updateVibratoFields = [this] {
            const bool enabled = m_vibratoBox->checkState() == Qt::Checked;
            m_vibratoPreset->setEnabled(enabled);
            m_vibratoDefault->setEnabled(enabled);
            for (const auto field : m_fields)
                field->setEnabled(enabled);
            for (const auto slider : m_vibratoSliders)
                slider->setEnabled(enabled);
        };
        connect(m_vibratoBox, &QCheckBox::checkStateChanged, this, updateVibratoFields);
        updateVibratoFields();
        const auto updatePortamentoMode = [this] {
            const bool custom = m_portamentoCustom->isChecked();
            const bool add = m_portamentoAddPoints->isChecked();
            const bool enabled = m_portamento->checkState() == Qt::Checked;
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
        connect(m_portamentoPresetMode, &QRadioButton::toggled, this, updatePortamentoMode);
        connect(m_portamentoCustom, &QRadioButton::toggled, this, updatePortamentoMode);
        connect(m_portamentoAddPoints, &QRadioButton::toggled, this, updatePortamentoMode);
        connect(m_portamento, &QCheckBox::checkStateChanged, this, updatePortamentoMode);
        connect(m_portamentoLength, &QSpinBox::valueChanged, m_portamentoLengthSlider,
                [this](int value) {
                    const QSignalBlocker blocker(m_portamentoLengthSlider);
                    m_portamentoLengthSlider->setValue(
                        std::clamp(value, m_portamentoLengthSlider->minimum(),
                                   m_portamentoLengthSlider->maximum()));
                });
        connect(m_portamentoLengthSlider, &QSlider::valueChanged, m_portamentoLength,
                &QSpinBox::setValue);
        connect(
            m_portamentoStart, &QSpinBox::valueChanged, m_portamentoStartSlider, [this](int value) {
                const QSignalBlocker blocker(m_portamentoStartSlider);
                m_portamentoStartSlider->setValue(std::clamp(
                    value, m_portamentoStartSlider->minimum(), m_portamentoStartSlider->maximum()));
            });
        connect(m_portamentoStartSlider, &QSlider::valueChanged, m_portamentoStart,
                &QSpinBox::setValue);
        const auto updatePortamentoBounds = [this] {
            const int maximumLength = std::clamp(
                int(std::lround(m_noteDuration - m_portamentoStart->value())), 0, 100000);
            m_portamentoLength->setMaximum(maximumLength);
            const int maximumStart =
                std::clamp(int(std::lround(m_noteDuration - m_portamentoLength->value())),
                           m_portamentoStart->minimum(), int(std::min(100000.0, m_noteDuration)));
            m_portamentoStart->setMaximum(maximumStart);
        };
        connect(m_portamentoLength, &QSpinBox::valueChanged, this, updatePortamentoBounds);
        connect(m_portamentoStart, &QSpinBox::valueChanged, this, updatePortamentoBounds);
        updatePortamentoBounds();
        updatePortamentoMode();
        m_initialPortamentoState = m_portamento->checkState();
        m_initialPortamentoPreset = m_portamentoPreset->currentIndex();
        m_initialPortamentoMode = this->portamentoMode();
        m_initialPortamentoLength = m_portamentoLength->value();
        m_initialPortamentoStart = m_portamentoStart->value();
        m_initialPortamentoCount = this->portamentoCount();
        m_initialAveragePoints = m_averagePoints->isChecked();
        connect(m_vibratoPreset, &QComboBox::currentIndexChanged, this, [this](int index) {
            const std::array<std::array<double, 3>, 3> values = {
                {{{65, 180, 35}}, {{65, 210, 55}}, {{65, 165, 20}}}
            };
            if (index >= 0 && index < int(values.size())) {
                for (size_t i = 0; i < values[index].size(); ++i)
                    m_fields[i]->setValue(values[index][i]);
            }
        });
        auto strength = new QSpinBox();
        strength->setRange(-100000, 100000);
        strength->setValue(int(values.intensity));
        strength->setEnabled(false);
        form->addRow(tr("Strength:"), strength);

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        auto layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(buttons);
    }

    PitchControlDialog::PitchControlDialog(const kit::Vibrato &values, QWidget *parent)
        : PitchControlDialog(true, true, values, 0, 0, 0, 59, -30, 2, true, {}, 480, 100000,
                             parent) {
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
                result.*Values[i] = m_fields[i]->value();
            }
        }
        return result;
    }

    bool PitchControlDialog::vibratoEdited() const {
        for (size_t i = 0; i < m_fields.size(); ++i) {
            if (m_fields[i]->value() != m_shown[i])
                return true;
        }
        return false;
    }

    bool PitchControlDialog::portamentoEdited() const {
        return m_portamento->checkState() != m_initialPortamentoState ||
               m_portamentoPreset->currentIndex() != m_initialPortamentoPreset ||
               portamentoMode() != m_initialPortamentoMode ||
               m_portamentoLength->value() != m_initialPortamentoLength ||
               m_portamentoStart->value() != m_initialPortamentoStart ||
               portamentoCount() != m_initialPortamentoCount ||
               m_averagePoints->isChecked() != m_initialAveragePoints;
    }

    int PitchControlDialog::portamentoPreset() const {
        return m_portamentoPreset->currentIndex();
    }

    int PitchControlDialog::vibratoPreset() const {
        return m_vibratoPreset->currentIndex();
    }

    int PitchControlDialog::portamentoMode() const {
        return m_portamentoCustom->isChecked() ? 1 : m_portamentoAddPoints->isChecked() ? 2 : 0;
    }

    int PitchControlDialog::portamentoLength() const {
        return m_portamentoLength->value();
    }

    int PitchControlDialog::portamentoStart() const {
        return m_portamentoStart->value();
    }

    int PitchControlDialog::portamentoCount() const {
        return m_portamentoCount->currentText().toInt();
    }

    bool PitchControlDialog::averagePoints() const {
        return m_averagePoints->isChecked();
    }

    void PitchControlDialog::setPortamentoContext(const QList<kit::PortamentoPoint> &points,
                                                  double noteDuration,
                                                  double previousNoteDuration) {
        m_existingPortamento = points;
        m_noteDuration = std::max(0.0, noteDuration);
        m_previousNoteDuration = std::max(0.0, previousNoteDuration);
    }

    QList<kit::PortamentoPoint> PitchControlDialog::portamentoPoints() const {
        // The presets are listed by length, each at the three positions.
        kit::PortamentoSettings settings;
        settings.mode = kit::PortamentoSettings::Mode(portamentoMode());
        settings.position = kit::PortamentoSettings::Position(portamentoPreset() % 3);
        settings.presetLength = kit::PortamentoSettings::presetLengths[portamentoPreset() / 3];
        settings.start = m_portamentoStart->value();
        settings.length = m_portamentoLength->value();
        settings.count = portamentoCount();
        settings.evenlyDistributed = m_averagePoints->isChecked();
        return settings.pointsFor(m_existingPortamento, m_noteDuration);
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

    kit::Vibrato PitchControlDialog::defaultVibrato() {
        kit::Vibrato vibrato;
        vibrato.length = 65;
        vibrato.period = 180;
        vibrato.amplitude = 35;
        vibrato.attack = 20;
        vibrato.release = 20;
        return vibrato;
    }

}
