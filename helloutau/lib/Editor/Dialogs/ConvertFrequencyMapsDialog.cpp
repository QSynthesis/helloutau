#include "ConvertFrequencyMapsDialog.h"

#include <algorithm>

#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/VoiceBank/FrequencyFormat.h>

namespace hello::daw {

    // The boxes hold the indexes in m_formats as their item data.
    ConvertFrequencyMapsDialog::ConvertFrequencyMapsDialog(
        const QList<kit::FrequencyFormat *> &formats, const kit::FrequencyFormat *source, int count,
        QWidget *parent)
        : QDialog(parent), m_formats(formats) {
        setWindowTitle(tr("Convert Frequency Maps"));

        m_source = new QComboBox();
        m_target = new QComboBox();
        for (qsizetype i = 0; i < m_formats.size(); ++i) {
            const auto format = m_formats.at(i);
            m_source->addItem(format->name(), int(i));
            if (format->canWrite()) {
                m_target->addItem(format->name(), int(i));
            }
        }
        m_source->setCurrentIndex(std::max(0, m_source->findData(int(m_formats.indexOf(source)))));
        for (int i = 0; i < m_target->count(); ++i) {
            if (m_formats.at(m_target->itemData(i).toInt()) != source) {
                m_target->setCurrentIndex(i);
                break;
            }
        }

        auto label = new QLabel(tr("The frequency maps of %n audio file(s) are converted. An audio "
                                   "file without a map of the source format is skipped. The maps "
                                   "of the target format are replaced, and their analysis caches "
                                   "are removed.",
                                   nullptr, count));
        label->setWordWrap(true);

        auto form = new QFormLayout();
        form->addRow(tr("&From:"), m_source);
        form->addRow(tr("&To:"), m_target);

        m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(m_source, &QComboBox::currentIndexChanged, this,
                &ConvertFrequencyMapsDialog::updateButtons);
        connect(m_target, &QComboBox::currentIndexChanged, this,
                &ConvertFrequencyMapsDialog::updateButtons);

        auto layout = new QVBoxLayout(this);
        layout->addWidget(label);
        layout->addLayout(form);
        layout->addWidget(m_buttons);
        updateButtons();
    }

    ConvertFrequencyMapsDialog::~ConvertFrequencyMapsDialog() = default;

    kit::FrequencyFormat *ConvertFrequencyMapsDialog::sourceFormat() const {
        const auto data = m_source->currentData();
        return data.isValid() ? m_formats.at(data.toInt()) : nullptr;
    }

    kit::FrequencyFormat *ConvertFrequencyMapsDialog::targetFormat() const {
        const auto data = m_target->currentData();
        return data.isValid() ? m_formats.at(data.toInt()) : nullptr;
    }

    QComboBox *ConvertFrequencyMapsDialog::sourceBox() const {
        return m_source;
    }

    QComboBox *ConvertFrequencyMapsDialog::targetBox() const {
        return m_target;
    }

    QDialogButtonBox *ConvertFrequencyMapsDialog::buttonBox() const {
        return m_buttons;
    }

    void ConvertFrequencyMapsDialog::updateButtons() {
        const auto source = sourceFormat();
        const auto target = targetFormat();
        m_buttons->button(QDialogButtonBox::Ok)->setEnabled(source && target && source != target);
    }

}
