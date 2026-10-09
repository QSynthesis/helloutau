#ifndef HELLOUTAU_EDITOR_DIALOGS_USTCHARSETDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_USTCHARSETDIALOG_H

#include <QtWidgets/QDialog>

#include <hellokit/Edit/ProjectDocument.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QListWidget;
class QPlainTextEdit;

namespace hello::daw {

    /// Asks the user in which encoding to read a UST that does not state it.
    ///
    /// The candidates are those of \c TextCodec::ustCandidates(). The text of the file is
    /// previewed in the selected encoding, so that the user can recognize the right one. The
    /// encoding that reads it best is selected at first, see TextCodec::ranked(), and those that
    /// cannot read it are grey.
    class HELLOUTAU_EDITOR_EXPORT UstCharsetDialog : public QDialog,
                                                     public kit::UstCharsetSelector {
        Q_OBJECT
    public:
        explicit UstCharsetDialog(QWidget *parent = nullptr);
        ~UstCharsetDialog();

        /// Shows the dialog for \a ust and waits for the answer.
        std::optional<QString> selectCharset(const kit::UstDocument &ust,
                                             const std::filesystem::path &path) override;

        /// The text of \a ust that the dialog shows in \a charset: the project name, the voice
        /// directory and the lyrics. Text that is not valid in \a charset is marked as such
        /// rather than shown garbled.
        static QString preview(const kit::UstDocument &ust, const QString &charset);

        /// Prepares the dialog for \a ust without showing it.
        void setDocument(const kit::UstDocument &ust, const std::filesystem::path &path);

        QString selectedCharset() const;
        void setSelectedCharset(const QString &charset);

        QString previewText() const;

        /// Whether candidate \a charset is grey, unable to read the text of the file.
        bool isGrey(const QString &charset) const;

    private:
        const kit::UstDocument *m_ust = nullptr;
        QListWidget *m_charsets;
        QPlainTextEdit *m_preview;

        void updatePreview();
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_USTCHARSETDIALOG_H
