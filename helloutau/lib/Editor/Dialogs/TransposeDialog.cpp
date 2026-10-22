#include "TransposeDialog.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/Document/DocumentConstants.h>

#include <helloutau/Widgets/FormLayout.h>
#include <helloutau/Widgets/PianoKeyboard.h>

namespace hello::daw {

    TransposeDialog::TransposeDialog(int lowest, int highest, QWidget *parent)
        : QDialog(parent), m_lowest(lowest), m_highest(highest) {
        setWindowTitle(tr("Transpose"));

        m_semitones = new QSpinBox();
        m_semitones->setRange(kit::lowestNoteNum - lowest, kit::highestNoteNum - highest);
        m_semitones->setValue(0);
        m_semitones->setSuffix(tr(" semitones"));
        m_summary = new QLabel();
        m_summary->setWordWrap(true);
        auto form = new FormLayout();
        form->addRow(tr("&Interval:"), m_semitones);
        form->addRow(m_summary);
        connect(m_semitones, &QSpinBox::valueChanged, this, &TransposeDialog::showSummary);

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(buttons);
        showSummary();
    }

    TransposeDialog::~TransposeDialog() = default;

    int TransposeDialog::semitones() const {
        return m_semitones->value();
    }

    QSpinBox *TransposeDialog::semitoneBox() const {
        return m_semitones;
    }

    QLabel *TransposeDialog::summaryLabel() const {
        return m_summary;
    }

    void TransposeDialog::showSummary() {
        const int semitones = m_semitones->value();
        const auto span = [](int lowest, int highest) {
            return lowest == highest
                       ? PianoKeyboard::keyName(lowest)
                       : tr("%1 to %2")
                             .arg(PianoKeyboard::keyName(lowest), PianoKeyboard::keyName(highest));
        };
        m_summary->setText(
            tr("The notes span %1, and %2 after the transposition. Within the keyboard range of "
               "UTAU, %3, the notes can be transposed by %4 to %5 semitones.")
                .arg(span(m_lowest, m_highest), span(m_lowest + semitones, m_highest + semitones),
                     span(kit::lowestNoteNum, kit::highestNoteNum))
                .arg(m_semitones->minimum())
                .arg(m_semitones->maximum()));
    }

}
