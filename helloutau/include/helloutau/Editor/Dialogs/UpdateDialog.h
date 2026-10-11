#ifndef HELLOUTAU_EDITOR_DIALOGS_UPDATEDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_UPDATEDIALOG_H

#include <QtWidgets/QDialog>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QTextBrowser;

namespace hello::daw {

    /// Offers a newer version of the application with its release notes. The user downloads it,
    /// skips it, or postpones the decision.
    class HELLOUTAU_EDITOR_EXPORT UpdateDialog : public QDialog {
        Q_OBJECT
    public:
        /// The decisions of the user
        enum Choice {
            /// Remind Me Later, also the result of closing the dialog
            Later,
            Download,
            Skip,
        };

        /// Constructs the dialog for \a version, newer than \a currentVersion, with \a notes in
        /// Markdown.
        UpdateDialog(const QString &version, const QString &currentVersion, const QString &notes,
                     QWidget *parent = nullptr);
        ~UpdateDialog();

        /// Returns the decision of the user, Later until a button is clicked.
        Choice choice() const;

        QTextBrowser *notesBrowser() const;

    private:
        Choice m_choice = Later;
        QTextBrowser *m_notes;
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_UPDATEDIALOG_H
