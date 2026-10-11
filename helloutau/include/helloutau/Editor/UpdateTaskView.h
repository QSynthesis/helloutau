#ifndef HELLOUTAU_EDITOR_UPDATETASKVIEW_H
#define HELLOUTAU_EDITOR_UPDATETASKVIEW_H

#include <functional>

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QLabel;
class QProgressBar;
class QPushButton;
class QTextBrowser;

namespace hello::daw {

    class AppSettings;
    class UpdateTask;

    /// The content of the update dialog, which shows an UpdateTask: the stage of the task, the
    /// release notes, the progress and the buttons of the stage, and below them the programs
    /// that start when the application quits, each of which can be canceled. The view only
    /// shows the task, which runs on without the view. See the update dialog in
    /// docs/Distribution.md.
    class HELLOUTAU_EDITOR_EXPORT UpdateTaskView : public QWidget {
        Q_OBJECT
    public:
        /// Constructs the view of \a task with \a settings, which outlive the view.
        UpdateTaskView(UpdateTask *task, AppSettings &settings, QWidget *parent = nullptr);
        ~UpdateTaskView();

        /// Sets the function that checks the releases again, which Check Again calls.
        void setCheck(const std::function<void()> &check);

        /// Sets the function that closes every window as quitting does, and returns whether
        /// every window was closed. Install Now calls it before the application quits.
        void setCloseWindows(const std::function<bool()> &closeWindows);

    Q_SIGNALS:
        /// The user chose to leave the view, with Close, Later, Skip This Version or Hide.
        void closeRequested();

    private:
        void showState();
        void showProgress(qint64 done, qint64 total);
        void showExitLaunches();
        void installNow();

        QPointer<UpdateTask> m_task;
        AppSettings &m_settings;
        std::function<void()> m_check;
        std::function<bool()> m_closeWindows;

        QLabel *m_message;
        QTextBrowser *m_notes;
        QProgressBar *m_progress;

        // The buttons of the stages, shown by showState()
        QPushButton *m_download;
        QPushButton *m_skip;
        QPushButton *m_cancel;
        QPushButton *m_hide;
        QPushButton *m_installNow;
        QPushButton *m_installOnQuit;
        QPushButton *m_discard;
        QPushButton *m_retry;
        QPushButton *m_checkAgain;
        QPushButton *m_openPage;
        QPushButton *m_close;

        // The programs that start when the application quits
        QWidget *m_exitLaunches;
        QLabel *m_installLabel;
        QPushButton *m_cancelInstall;
        QLabel *m_restartLabel;
        QPushButton *m_cancelRestart;
    };

}

#endif // HELLOUTAU_EDITOR_UPDATETASKVIEW_H
