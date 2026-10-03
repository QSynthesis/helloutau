#include "PitchControlDialog.h"

#include <algorithm>
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
        const QList<kit::PortamentoPoint> &existingPortamento, int noteLength, QWidget *parent)
        : QDialog(parent), m_vibrato(values), m_existingPortamento(existingPortamento),
          m_noteLength(noteLength) {
        setWindowTitle(tr("Pitch Control (Mode2)"));
        m_portamento = new QCheckBox(tr("&Portamento"));
        m_portamento->setTristate(true);
        m_portamento->setCheckState(portamento ? (*portamento ? Qt::Checked : Qt::Unchecked)
                                               : Qt::PartiallyChecked);
        m_vibratoBox = new QCheckBox(tr("&Vibrato"));
        m_vibratoBox->setTristate(true);
        m_vibratoBox->setCheckState(vibrato ? (*vibrato ? Qt::Checked : Qt::Unchecked)
                                            : Qt::PartiallyChecked);

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
        m_portamentoLength->setRange(1, 100000);
        m_portamentoLength->setValue(std::max(1, portamentoLength));
        m_portamentoLength->setSuffix(tr(" ms"));
        m_portamentoLengthSlider = new QSlider(Qt::Horizontal);
        m_portamentoLengthSlider->setRange(1, 100000);
        m_portamentoLengthSlider->setValue(m_portamentoLength->value());
        m_portamentoStart = new QSpinBox();
        m_portamentoStart->setRange(-100000, 0);
        m_portamentoStart->setValue(std::min(0, portamentoStart));
        m_portamentoStart->setSuffix(tr(" ms"));
        m_portamentoStartSlider = new QSlider(Qt::Horizontal);
        m_portamentoStartSlider->setRange(-100000, 0);
        m_portamentoStartSlider->setValue(m_portamentoStart->value());
        m_portamentoCount = new QComboBox();
        for (int i = 2; i <= 6; ++i)
            m_portamentoCount->addItem(QString::number(i));
        m_portamentoCount->setCurrentIndex(std::clamp(portamentoCount - 2, 0, 4));
        m_averagePoints = new QCheckBox(tr("&Evenly distribute"));
        m_averagePoints->setChecked(averagePoints);
        m_vibratoPreset = new QComboBox();
        m_vibratoPreset->addItems(
            {tr("Default (65, 180, 35)"), tr("Deep (65, 210, 55)"), tr("Light (65, 165, 20)")});
        m_vibratoPreset->setCurrentIndex(std::clamp(vibratoPreset, 0, 2));

        auto form = new QFormLayout();
        const struct {
            const char *label;
            double minimum;
            double maximum;
            QString suffix;
        } fields[] = {
            {QT_TR_NOOP("&Length:"),   0,       100,    QStringLiteral(" %") },
            {QT_TR_NOOP("&Period:"),   0,       100000, QStringLiteral(" ms")},
            {QT_TR_NOOP("&Depth:"),    0,       100000, tr(" cents")         },
            {QT_TR_NOOP("Fade &in:"),  0,       100,    QStringLiteral(" %") },
            {QT_TR_NOOP("Fade &out:"), 0,       100,    QStringLiteral(" %") },
            {QT_TR_NOOP("P&hase:"),    -100000, 100000, QStringLiteral(" %") },
            {QT_TR_NOOP("&Height:"),   -100000, 100000, QStringLiteral(" %") },
        };
        form->addRow(m_portamento);
        form->addRow(m_portamentoPresetMode, m_portamentoPreset);
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
        form->addRow(tr("Vibrato preset:"), m_vibratoPreset);
        for (size_t i = 0; i < m_fields.size(); ++i) {
            auto box = new QDoubleSpinBox();
            box->setDecimals(2);
            box->setRange(fields[i].minimum, fields[i].maximum);
            box->setSuffix(fields[i].suffix);
            box->setValue(values.*Values[i]);
            m_fields[i] = box;
            m_shown[i] = box->value();
            form->addRow(tr(fields[i].label), box);
        }
        const auto updateVibratoFields = [this] {
            const bool enabled = m_vibratoBox->checkState() == Qt::Checked;
            m_vibratoPreset->setEnabled(enabled);
            for (const auto field : m_fields)
                field->setEnabled(enabled);
        };
        connect(m_vibratoBox, &QCheckBox::checkStateChanged, this, updateVibratoFields);
        updateVibratoFields();
        const auto updatePortamentoMode = [this] {
            const bool custom = m_portamentoCustom->isChecked();
            const bool add = m_portamentoAddPoints->isChecked();
            const bool enabled = m_portamento->checkState() == Qt::Checked;
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
                &QSlider::setValue);
        connect(m_portamentoLengthSlider, &QSlider::valueChanged, m_portamentoLength,
                &QSpinBox::setValue);
        connect(m_portamentoStart, &QSpinBox::valueChanged, m_portamentoStartSlider,
                &QSlider::setValue);
        connect(m_portamentoStartSlider, &QSlider::valueChanged, m_portamentoStart,
                &QSpinBox::setValue);
        updatePortamentoMode();
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
        m_default = new QPushButton(tr("Set as Default"));

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        auto layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(m_default);
        layout->addWidget(buttons);
    }

    PitchControlDialog::PitchControlDialog(const kit::Vibrato &values, QWidget *parent)
        : PitchControlDialog(true, true, values, 0, 0, 0, 59, -30, 2, true, {}, 480, parent) {
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
                                                  int noteLength) {
        m_existingPortamento = points;
        m_noteLength = std::max(1, noteLength);
    }

    QList<kit::PortamentoPoint> PitchControlDialog::portamentoPoints() const {
        if (m_portamentoCustom->isChecked()) {
            const double first = m_portamentoStart->value();
            const double firstY =
                m_existingPortamento.size() == 2 ? m_existingPortamento.first().y : 0;
            const double secondY =
                m_existingPortamento.size() == 2 ? m_existingPortamento.last().y : 0;
            return {
                kit::PortamentoPoint{first,                               firstY,  kit::PortamentoPoint::S},
                kit::PortamentoPoint{first + m_portamentoLength->value(), secondY,
                                     kit::PortamentoPoint::S                                              }
            };
        }
        const int preset = portamentoPreset();
        const double distance = preset / 3 == 0 ? 50 : preset / 3 == 1 ? 100 : 200;
        const int side = preset % 3;
        const double first = side == 2 ? 0 : -distance;
        const double second = side == 1 ? 0 : distance;
        if (!m_portamentoAddPoints->isChecked())
            return {
                kit::PortamentoPoint{first,  0, kit::PortamentoPoint::S},
                kit::PortamentoPoint{second, 0, kit::PortamentoPoint::S}
            };
        const int count = m_portamentoCount->currentText().toInt();
        QList<kit::PortamentoPoint> points;
        points.reserve(count);
        if (!m_averagePoints->isChecked() && count == m_existingPortamento.size() &&
            !m_existingPortamento.isEmpty()) {
            return m_existingPortamento;
        }
        if (!m_existingPortamento.isEmpty()) {
            const double first = m_existingPortamento.first().x;
            const double last = m_averagePoints->isChecked()
                                    ? std::max(first, double(m_noteLength))
                                    : std::max(first, m_existingPortamento.last().x);
            for (int i = 0; i < count; ++i) {
                const double x = first + (last - first) * double(i) / double(count - 1);
                double y = m_existingPortamento.last().y;
                for (int j = 1; j < m_existingPortamento.size(); ++j) {
                    const auto &left = m_existingPortamento.at(j - 1);
                    const auto &right = m_existingPortamento.at(j);
                    if (x <= right.x) {
                        const double span = right.x - left.x;
                        const double fraction = span == 0 ? 0 : (x - left.x) / span;
                        y = left.y + (right.y - left.y) * fraction;
                        break;
                    }
                }
                points.push_back({x, y, kit::PortamentoPoint::S});
            }
            return points;
        }
        for (int i = 0; i < count; ++i) {
            const double fraction = double(i) / double(count - 1);
            points.push_back({first + (second - first) * fraction, 0, kit::PortamentoPoint::S});
        }
        return points;
    }

    QPushButton *PitchControlDialog::defaultButton() const {
        return m_default;
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
