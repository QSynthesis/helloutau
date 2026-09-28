#include "ScalePitchDialog.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QVBoxLayout>

namespace hello::daw {

    namespace {

        // The largest factor offered, in percent
        constexpr double maximumPercent = 1000;

        QDoubleSpinBox *percentBox() {
            auto box = new QDoubleSpinBox();
            box->setDecimals(0);
            box->setRange(0, maximumPercent);
            box->setSingleStep(10);
            box->setSuffix(QStringLiteral(" %"));
            box->setValue(100);
            return box;
        }

    }

    ScalePitchDialog::ScalePitchDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle(tr("Scale Pitch"));

        m_portamento = percentBox();
        m_vibrato = percentBox();
        auto form = new QFormLayout();
        form->addRow(tr("&Pitch points:"), m_portamento);
        form->addRow(tr("&Vibrato depth:"), m_vibrato);

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(buttons);
    }

    ScalePitchDialog::~ScalePitchDialog() = default;

    double ScalePitchDialog::portamento() const {
        return m_portamento->value() / 100;
    }

    double ScalePitchDialog::vibrato() const {
        return m_vibrato->value() / 100;
    }

    QDoubleSpinBox *ScalePitchDialog::portamentoBox() const {
        return m_portamento;
    }

    QDoubleSpinBox *ScalePitchDialog::vibratoBox() const {
        return m_vibrato;
    }

}
