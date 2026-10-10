#ifndef HELLOUTAU_EDITOR_DIALOGS_REPLACELYRICSDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_REPLACELYRICSDIALOG_H

#include <QtWidgets/QDialog>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QCheckBox;
class QPlainTextEdit;

namespace hello::daw {

    /// Dialog for the lyrics that replace those of the selected notes, separated by spaces or one
    /// per character, and for whether they repeat to fill the selection. The caller assigns the
    /// lyrics to the notes.
    class HELLOUTAU_EDITOR_EXPORT ReplaceLyricsDialog : public QDialog {
        Q_OBJECT
    public:
        explicit ReplaceLyricsDialog(QWidget *parent = nullptr);
        ~ReplaceLyricsDialog();

        /// The text entered, as typed
        QString lyrics() const;

        /// Whether the lyrics repeat until every selected note has one
        bool repeat() const;

        /// Whether each character other than a space is a lyric, rather than each word
        bool splitCharacters() const;

        /// Shows \a lyrics selected, so that typing replaces them.
        void setLyrics(const QString &lyrics);

    private:
        QPlainTextEdit *m_lyrics = nullptr;
        QCheckBox *m_repeat = nullptr;
        QCheckBox *m_splitCharacters = nullptr;
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_REPLACELYRICSDIALOG_H
