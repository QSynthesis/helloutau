#ifndef HELLOUTAU_EDITOR_DIALOGS_OUTPUTLOGDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_OUTPUTLOGDIALOG_H

#include <memory>

#include <QtWidgets/QDialog>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QPlainTextEdit;

namespace hello::kit {
    class SynthToolOutputLog;
}

namespace hello::daw {

    /// Shows the output that a log of synth tools holds, scrolled to its end, with a button that
    /// clears the log: the render log of a project window and the log of the generation of
    /// frequency maps of a voice bank window.
    class HELLOUTAU_EDITOR_EXPORT OutputLogDialog : public QDialog {
        Q_OBJECT
    public:
        OutputLogDialog(std::shared_ptr<kit::SynthToolOutputLog> log, const QString &title,
                        QWidget *parent = nullptr);
        ~OutputLogDialog();

        /// The text of the log as shown.
        QPlainTextEdit *textView() const;

    private:
        std::shared_ptr<kit::SynthToolOutputLog> m_log;
        QPlainTextEdit *m_text;
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_OUTPUTLOGDIALOG_H
