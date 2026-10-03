#include "PitchControlDialog.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>

namespace hello::daw {

    namespace {
        constexpr double kit::Vibrato::*Values[] = {
            &kit::Vibrato::length, &kit::Vibrato::period,  &kit::Vibrato::amplitude,
            &kit::Vibrato::attack, &kit::Vibrato::release, &kit::Vibrato::phase,
            &kit::Vibrato::offset,
        };
    }

    PitchControlDialog::PitchControlDialog(std::optional<bool> portamento,
                                           std::optional<bool> vibrato, const kit::Vibrato &values,
                                           QWidget *parent)
        : QDialog(parent), m_vibrato(values) {
        setWindowTitle(tr("Pitch Control (Mode2)"));
        m_portamento = new QCheckBox(tr("&Portamento"));
        m_portamento->setTristate(true);
        m_portamento->setCheckState(portamento ? (*portamento ? Qt::Checked : Qt::Unchecked)
                                               : Qt::PartiallyChecked);
        m_vibratoBox = new QCheckBox(tr("&Vibrato"));
        m_vibratoBox->setTristate(true);
        m_vibratoBox->setCheckState(vibrato ? (*vibrato ? Qt::Checked : Qt::Unchecked)
                                            : Qt::PartiallyChecked);

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
        form->addRow(m_vibratoBox);
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
            for (const auto field : m_fields)
                field->setEnabled(enabled);
        };
        connect(m_vibratoBox, &QCheckBox::checkStateChanged, this, updateVibratoFields);
        updateVibratoFields();
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
        : PitchControlDialog(true, true, values, parent) {
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
