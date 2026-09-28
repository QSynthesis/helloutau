#include "VibratoDialog.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QVBoxLayout>

namespace hello::daw {

    namespace {

        // The values of a vibrato in the order of VBR
        constexpr double kit::Vibrato::*Values[] = {
            &kit::Vibrato::length, &kit::Vibrato::period,  &kit::Vibrato::amplitude,
            &kit::Vibrato::attack, &kit::Vibrato::release, &kit::Vibrato::phase,
            &kit::Vibrato::offset,
        };

    }

    VibratoDialog::VibratoDialog(const kit::Vibrato &vibrato, QWidget *parent)
        : QDialog(parent), m_vibrato(vibrato) {
        setWindowTitle(tr("Vibrato"));

        const auto percent = QStringLiteral(" %");
        const struct {
            const char *label;
            double minimum;
            double maximum;
            QString suffix;
        } fields[] = {
            {QT_TR_NOOP("&Length:"),   0,       100,    percent              },
            {QT_TR_NOOP("&Period:"),   0,       100000, QStringLiteral(" ms")},
            {QT_TR_NOOP("&Depth:"),    0,       100000, tr(" cents")         },
            {QT_TR_NOOP("Fade &in:"),  0,       100,    percent              },
            {QT_TR_NOOP("Fade &out:"), 0,       100,    percent              },
            {QT_TR_NOOP("P&hase:"),    -100000, 100000, percent              },
            {QT_TR_NOOP("&Height:"),   -100000, 100000, percent              },
        };
        auto form = new QFormLayout();
        for (size_t i = 0; i < m_fields.size(); ++i) {
            auto box = new QDoubleSpinBox();
            box->setDecimals(2);
            box->setRange(fields[i].minimum, fields[i].maximum);
            box->setSuffix(fields[i].suffix);
            box->setValue(vibrato.*Values[i]);
            m_fields[i] = box;
            m_shown[i] = box->value();
            form->addRow(tr(fields[i].label), box);
        }

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(buttons);
    }

    VibratoDialog::~VibratoDialog() = default;

    kit::Vibrato VibratoDialog::vibrato() const {
        auto result = m_vibrato;
        for (size_t i = 0; i < m_fields.size(); ++i) {
            if (m_fields[i]->value() != m_shown[i]) {
                result.*Values[i] = m_fields[i]->value();
            }
        }
        return result;
    }

    QDoubleSpinBox *VibratoDialog::field(int index) const {
        return m_fields.at(size_t(index));
    }

    kit::Vibrato VibratoDialog::defaultVibrato() {
        kit::Vibrato vibrato;
        vibrato.length = 65;
        vibrato.period = 180;
        vibrato.amplitude = 35;
        vibrato.attack = 20;
        vibrato.release = 20;
        return vibrato;
    }

}
