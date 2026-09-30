#include "MidiEncodingPage.h"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/Interchange/InterchangeReader.h>

#include <Interchange/SourcePreview.h>

#include "InterchangeOptionForm.h"

namespace hello::daw {

    namespace {

        // The key of the encoding option of the MIDI drivers
        const char encodingKey[] = "encoding";

    }

    MidiEncodingPage::MidiEncodingPage(QWidget *parent) : InterchangeStepPage(parent) {
        m_encodings = new QListWidget();
        m_preview = new QPlainTextEdit();
        m_preview->setReadOnly(true);

        auto content = new QHBoxLayout();
        content->addWidget(m_encodings, 1);
        content->addWidget(m_preview, 3);

        m_layout = new QVBoxLayout(this);
        m_layout->setContentsMargins({});
        auto label = new QLabel(tr("MIDI does not specify the encoding of its text. Select the "
                                   "encoding in which the text is displayed correctly."));
        label->setWordWrap(true);
        m_layout->addWidget(label);
        m_layout->addLayout(content, 1);

        connect(m_encodings, &QListWidget::currentRowChanged, this, [this] {
            updatePreview();
            Q_EMIT completeChanged();
        });
    }

    MidiEncodingPage::~MidiEncodingPage() = default;

    void MidiEncodingPage::reset(const kit::InterchangeReader &reader,
                                 const kit::InterchangeSource &source) {
        m_source = source;

        // The candidates are the choices that the driver declares for the encoding option.
        QStringList candidates;
        QList<kit::InterchangeOption> others;
        for (const auto &option : reader.optionSchema()) {
            if (option.key == QLatin1String(encodingKey)) {
                candidates = option.choices;
            } else {
                others.push_back(option);
            }
        }

        const auto texts = SourcePreview::textsOf(source);
        const auto grey = palette().brush(QPalette::Disabled, QPalette::Text);
        m_encodings->clear();
        for (const auto &candidate : std::as_const(candidates)) {
            auto item = new QListWidgetItem(candidate, m_encodings);
            if (!SourcePreview::decodes(texts, candidate)) {
                item->setForeground(grey);
            }
        }
        const auto items = m_encodings->findItems(SourcePreview::defaultEncoding(texts, candidates),
                                                  Qt::MatchExactly);
        if (!items.isEmpty()) {
            m_encodings->setCurrentItem(items.first());
        }

        delete m_form;
        m_form = new InterchangeOptionForm(others);
        m_layout->addWidget(m_form);
        updatePreview();
    }

    bool MidiEncodingPage::apply(kit::ImportRequest &request) const {
        if (!isComplete()) {
            return false;
        }
        if (m_form) {
            const auto values = m_form->values();
            for (auto it = values.begin(); it != values.end(); ++it) {
                request.driverOptions.insert(it.key(), it.value());
            }
        }
        request.driverOptions.insert(QLatin1String(encodingKey), selectedEncoding());
        return true;
    }

    bool MidiEncodingPage::isComplete() const {
        return !selectedEncoding().isEmpty();
    }

    QString MidiEncodingPage::selectedEncoding() const {
        const auto item = m_encodings->currentItem();
        return item ? item->text() : QString();
    }

    void MidiEncodingPage::updatePreview() {
        const auto encoding = selectedEncoding();
        m_preview->setPlainText(encoding.isEmpty() ? QString()
                                                   : SourcePreview::preview(m_source, encoding));
    }

}
