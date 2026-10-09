#ifndef HELLOUTAU_EDITOR_DIALOGS_REPLACELYRICSDIALOG_P_H
#define HELLOUTAU_EDITOR_DIALOGS_REPLACELYRICSDIALOG_P_H

#include <QtWidgets/QDialog>

class QCheckBox;
class QTextEdit;

namespace hello::daw {

    class ReplaceLyricsDialog : public QDialog {
        Q_OBJECT

    public:
        explicit ReplaceLyricsDialog(QWidget *parent = nullptr);

        QString lyrics() const;
        bool repeat() const;
        bool splitCharacters() const;

        void setLyrics(const QString &lyrics);

    private:
        QTextEdit *m_lyrics = nullptr;
        QCheckBox *m_repeat = nullptr;
        QCheckBox *m_splitCharacters = nullptr;
    };

}

#endif

