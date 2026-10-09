#include "ReplaceLyricsDialog_p.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>

namespace hello::daw {

    ReplaceLyricsDialog::ReplaceLyricsDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle(tr("Replace Lyrics"));
        resize(480, 360);

        auto layout = new QVBoxLayout(this);
        auto title = new QLabel(tr("Lyrics (separated by spaces):"), this);
        layout->addWidget(title);
        m_lyrics = new QTextEdit(this);
        m_lyrics->setAcceptRichText(false);
        m_lyrics->setTabChangesFocus(false);
        layout->addWidget(m_lyrics, 1);

        m_repeat = new QCheckBox(tr("Repeat to fill the selected notes"), this);
        m_splitCharacters = new QCheckBox(tr("Split by character"), this);
        layout->addWidget(m_repeat);
        layout->addWidget(m_splitCharacters);

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
    }

    QString ReplaceLyricsDialog::lyrics() const {
        return m_lyrics->toPlainText();
    }

    bool ReplaceLyricsDialog::repeat() const {
        return m_repeat->isChecked();
    }

    bool ReplaceLyricsDialog::splitCharacters() const {
        return m_splitCharacters->isChecked();
    }

    void ReplaceLyricsDialog::setLyrics(const QString &lyrics) {
        m_lyrics->setPlainText(lyrics);
        m_lyrics->selectAll();
    }

}

