#include "PasteParametersDialog.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

namespace hello::daw {

    PasteParametersDialog::PasteParametersDialog(PianoRoll::Parameters parameters, QWidget *parent)
        : QDialog(parent) {
        setWindowTitle(tr("Paste Parameters"));

        m_portamento = new QCheckBox(tr("&Pitch points"));
        m_vibrato = new QCheckBox(tr("&Vibrato"));
        m_envelope = new QCheckBox(tr("&Envelope"));
        m_portamento->setChecked(parameters & PianoRoll::PortamentoParameter);
        m_vibrato->setChecked(parameters & PianoRoll::VibratoParameter);
        m_envelope->setChecked(parameters & PianoRoll::EnvelopeParameter);

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        const auto update = [this, buttons] {
            buttons->button(QDialogButtonBox::Ok)->setEnabled(this->parameters() != 0);
        };
        for (const auto box : {m_portamento, m_vibrato, m_envelope}) {
            connect(box, &QCheckBox::toggled, this, update);
        }
        update();

        auto layout = new QVBoxLayout(this);
        layout->addWidget(m_portamento);
        layout->addWidget(m_vibrato);
        layout->addWidget(m_envelope);
        layout->addWidget(buttons);
    }

    PasteParametersDialog::~PasteParametersDialog() = default;

    PianoRoll::Parameters PasteParametersDialog::parameters() const {
        PianoRoll::Parameters result;
        result.setFlag(PianoRoll::PortamentoParameter, m_portamento->isChecked());
        result.setFlag(PianoRoll::VibratoParameter, m_vibrato->isChecked());
        result.setFlag(PianoRoll::EnvelopeParameter, m_envelope->isChecked());
        return result;
    }

    QCheckBox *PasteParametersDialog::box(PianoRoll::Parameter parameter) const {
        switch (parameter) {
            case PianoRoll::PortamentoParameter:
                return m_portamento;
            case PianoRoll::VibratoParameter:
                return m_vibrato;
            case PianoRoll::EnvelopeParameter:
                return m_envelope;
            default:
                return nullptr;
        }
    }

}
