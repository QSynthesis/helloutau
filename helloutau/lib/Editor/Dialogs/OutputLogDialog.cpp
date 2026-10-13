#include "OutputLogDialog.h"

#include <QtGui/QTextCursor>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/Synth/SynthToolProcess.h>

namespace hello::daw {

    OutputLogDialog::OutputLogDialog(std::shared_ptr<kit::SynthToolOutputLog> log,
                                     const QString &title, QWidget *parent)
        : QDialog(parent), m_log(std::move(log)) {
        setWindowTitle(title);
        resize(720, 480);
        auto layout = new QVBoxLayout(this);
        m_text = new QPlainTextEdit(this);
        m_text->setReadOnly(true);
        m_text->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_text->setPlainText(m_log->text());
        m_text->moveCursor(QTextCursor::End);
        m_text->verticalScrollBar()->setValue(m_text->verticalScrollBar()->maximum());
        layout->addWidget(m_text);
        auto buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
        auto clear = buttons->addButton(tr("Clear"), QDialogButtonBox::DestructiveRole);
        connect(clear, &QPushButton::clicked, this, [this] {
            m_log->clear();
            m_text->clear();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
    }

    OutputLogDialog::~OutputLogDialog() = default;

    QPlainTextEdit *OutputLogDialog::textView() const {
        return m_text;
    }

}
