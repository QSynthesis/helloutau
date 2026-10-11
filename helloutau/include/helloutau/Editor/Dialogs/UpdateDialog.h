#ifndef HELLOUTAU_EDITOR_DIALOGS_UPDATEDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_UPDATEDIALOG_H

#include <QtWidgets/QDialog>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    class AppSettings;
    class UpdateTask;
    class UpdateTaskView;

    /// The modal dialog of the update of the application, which shows its UpdateTask with an
    /// UpdateTaskView. Closing the dialog leaves the task running, and the dialog shows the task
    /// again when it is opened again. See the update dialog in docs/Distribution.md.
    class HELLOUTAU_EDITOR_EXPORT UpdateDialog : public QDialog {
        Q_OBJECT
    public:
        /// Constructs the dialog of \a task with \a settings, which outlive the dialog.
        UpdateDialog(UpdateTask *task, AppSettings &settings, QWidget *parent = nullptr);
        ~UpdateDialog();

        UpdateTaskView *view() const;

    private:
        UpdateTaskView *m_view;
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_UPDATEDIALOG_H
