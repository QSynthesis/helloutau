#include "UpdateTaskView.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/Support/ExitLauncher.h>

#include "AppSettings.h"
#include "UpdateTask.h"

namespace hello::daw {

    namespace {

        // The resolution of the progress bar, whose range is an int
        constexpr int progressSteps = 1000;

        QString applicationName() {
            return QApplication::applicationDisplayName();
        }

    }

    UpdateTaskView::UpdateTaskView(UpdateTask *task, AppSettings &settings, QWidget *parent)
        : QWidget(parent), m_task(task), m_settings(settings) {
        m_message = new QLabel(this);
        m_message->setObjectName(QStringLiteral("message"));
        m_message->setWordWrap(true);
        m_notes = new QTextBrowser(this);
        m_notes->setObjectName(QStringLiteral("notes"));
        m_notes->setOpenExternalLinks(true);
        m_progress = new QProgressBar(this);
        m_progress->setTextVisible(false);

        const auto button = [this](const QString &text, const char *name) {
            auto result = new QPushButton(text, this);
            result->setObjectName(QLatin1String(name));
            return result;
        };
        m_download = button(tr("&Download and Install"), "download");
        m_skip = button(tr("&Skip This Version"), "skip");
        m_cancel = button(tr("Cancel"), "cancel");
        m_hide = button(tr("Run in &Background"), "hide");
        m_installNow = button(tr("Install &Now"), "installNow");
        m_installOnQuit = button(tr("Install on &Quit"), "installOnQuit");
        m_discard = button(tr("&Discard Update"), "discard");
        m_retry = button(tr("&Retry"), "retry");
        m_checkAgain = button(tr("Check &Again"), "checkAgain");
        m_openPage = button(tr("Open Release &Page"), "openPage");
        m_close = button(tr("&Close"), "close");

        m_exitLaunches = new QGroupBox(tr("When %1 quits").arg(applicationName()), this);
        m_installLabel = new QLabel(m_exitLaunches);
        m_cancelInstall = new QPushButton(tr("Cancel Installation"), m_exitLaunches);
        m_cancelInstall->setObjectName(QStringLiteral("cancelInstall"));
        m_restartLabel = new QLabel(m_exitLaunches);
        m_cancelRestart = new QPushButton(tr("Cancel Restart"), m_exitLaunches);
        m_cancelRestart->setObjectName(QStringLiteral("cancelRestart"));
        auto launches = new QGridLayout(m_exitLaunches);
        launches->addWidget(m_installLabel, 0, 0);
        launches->addWidget(m_cancelInstall, 0, 1);
        launches->addWidget(m_restartLabel, 1, 0);
        launches->addWidget(m_cancelRestart, 1, 1);
        launches->setColumnStretch(0, 1);

        auto buttons = new QHBoxLayout();
        buttons->addStretch();
        for (const auto widget : {m_download, m_skip, m_installNow, m_installOnQuit, m_discard,
                                  m_retry, m_checkAgain, m_openPage, m_hide, m_cancel, m_close}) {
            buttons->addWidget(widget);
        }
        auto layout = new QVBoxLayout(this);
        layout->addWidget(m_message);
        layout->addWidget(m_notes, 1);
        layout->addWidget(m_progress);
        layout->addWidget(m_exitLaunches);
        layout->addLayout(buttons);

        connect(m_download, &QPushButton::clicked, this, [this] {
            if (m_task) {
                m_task->prepare();
            }
        });
        connect(m_skip, &QPushButton::clicked, this, [this] {
            if (m_task && m_task->release()) {
                m_settings.updates().setSkippedVersion(m_task->release()->version.toString());
            }
            Q_EMIT closeRequested();
        });
        connect(m_cancel, &QPushButton::clicked, this, [this] {
            if (m_task) {
                m_task->cancel();
            }
        });
        connect(m_discard, &QPushButton::clicked, this, [this] {
            if (m_task) {
                m_task->cancel();
            }
        });
        connect(m_hide, &QPushButton::clicked, this, &UpdateTaskView::closeRequested);
        connect(m_close, &QPushButton::clicked, this, &UpdateTaskView::closeRequested);
        connect(m_installNow, &QPushButton::clicked, this, &UpdateTaskView::installNow);
        connect(m_installOnQuit, &QPushButton::clicked, this, [this] {
            if (m_task) {
                m_task->schedule(false);
            }
            showState();
        });
        connect(m_retry, &QPushButton::clicked, this, [this] {
            if (m_task && m_task->release()) {
                m_task->prepare();
            } else if (m_check) {
                m_check();
            }
        });
        connect(m_checkAgain, &QPushButton::clicked, this, [this] {
            if (m_check) {
                m_check();
            }
        });
        connect(m_openPage, &QPushButton::clicked, this, [this] {
            if (m_task && m_task->release()) {
                QDesktopServices::openUrl(m_task->release()->page);
            }
        });
        connect(m_cancelInstall, &QPushButton::clicked, this, [this] {
            if (m_task) {
                m_task->unschedule();
            }
            showState();
        });
        connect(m_cancelRestart, &QPushButton::clicked, this, [this] {
            if (const auto launcher = kit::ExitLauncher::instance()) {
                launcher->removeLaunch(QLatin1String(kit::ExitLauncher::restartKey));
            }
            showExitLaunches();
        });
        if (m_task) {
            connect(m_task, &UpdateTask::stateChanged, this, &UpdateTaskView::showState);
            connect(m_task, &UpdateTask::progressChanged, this, &UpdateTaskView::showProgress);
        }
        showState();
    }

    UpdateTaskView::~UpdateTaskView() = default;

    void UpdateTaskView::setCheck(const std::function<void()> &check) {
        m_check = check;
    }

    void UpdateTaskView::setCloseWindows(const std::function<bool()> &closeWindows) {
        m_closeWindows = closeWindows;
    }

    void UpdateTaskView::showState() {
        for (const auto widget : {m_download, m_skip, m_cancel, m_hide, m_installNow,
                                  m_installOnQuit, m_discard, m_retry, m_checkAgain, m_openPage}) {
            widget->hide();
        }
        const auto state = m_task ? m_task->state() : UpdateTask::Idle;
        const auto &release = m_task ? m_task->release() : std::nullopt;
        const auto version = release ? release->version.toString() : QString();
        const auto current = QCoreApplication::applicationVersion();
        const auto name = applicationName();

        m_notes->setVisible(release.has_value() && state != UpdateTask::Failed);
        if (release) {
            m_notes->setMarkdown(release->notes);
        }
        m_progress->setVisible(state == UpdateTask::Checking || state == UpdateTask::Downloading ||
                               state == UpdateTask::Extracting);
        // The range of an unknown progress, until the first report
        m_progress->setRange(0, 0);

        switch (state) {
            case UpdateTask::Idle:
                m_message->setText(tr("Updates have not been checked."));
                m_checkAgain->show();
                break;
            case UpdateTask::Checking:
                m_message->setText(tr("Checking for updates..."));
                m_cancel->show();
                break;
            case UpdateTask::UpToDate:
                m_message->setText(tr("%1 %2 is the latest version.").arg(name, current));
                m_checkAgain->show();
                break;
            case UpdateTask::Available:
                if (m_task->installs()) {
                    m_message->setText(tr("%1 %2 is available. The current version is %3.")
                                           .arg(name, version, current));
                    m_download->show();
                } else {
                    m_message->setText(tr("%1 %2 is available. The current version is %3. "
                                          "Download the new version from the page of the release.")
                                           .arg(name, version, current));
                }
                m_skip->show();
                m_openPage->show();
                break;
            case UpdateTask::Downloading:
                m_message->setText(tr("Downloading and verifying %1 %2...").arg(name, version));
                m_cancel->show();
                m_hide->show();
                break;
            case UpdateTask::Extracting:
                m_message->setText(tr("Extracting %1 %2...").arg(name, version));
                m_cancel->show();
                m_hide->show();
                break;
            case UpdateTask::Ready:
                if (m_task->isScheduled()) {
                    m_message->setText(
                        tr("%1 %2 will be installed when %1 quits.").arg(name, version));
                } else {
                    m_message->setText(tr("%1 %2 is ready to be installed.").arg(name, version));
                    m_installNow->show();
                    m_installOnQuit->show();
                }
                m_discard->show();
                break;
            case UpdateTask::Failed:
                m_message->setText(m_task->errorString());
                m_retry->show();
                m_openPage->setVisible(release.has_value());
                break;
        }
        showExitLaunches();
    }

    void UpdateTaskView::showProgress(qint64 done, qint64 total) {
        if (total <= 0) {
            m_progress->setRange(0, 0);
            return;
        }
        m_progress->setRange(0, progressSteps);
        m_progress->setValue(int(done * progressSteps / total));
    }

    void UpdateTaskView::showExitLaunches() {
        const auto launcher = kit::ExitLauncher::instance();
        const bool install = m_task && m_task->isScheduled();
        const bool restart = launcher && launcher->isRestartScheduled();
        const auto &release = m_task ? m_task->release() : std::nullopt;
        m_installLabel->setText(
            tr("Install %1 %2")
                .arg(applicationName(), release ? release->version.toString() : QString()));
        m_restartLabel->setText(tr("Restart %1").arg(applicationName()));
        m_installLabel->setVisible(install);
        m_cancelInstall->setVisible(install);
        m_restartLabel->setVisible(restart);
        m_cancelRestart->setVisible(restart);
        m_exitLaunches->setVisible(install || restart);
    }

    void UpdateTaskView::installNow() {
        if (!m_task || (m_closeWindows && !m_closeWindows())) {
            return;
        }
        if (!m_task->schedule(true)) {
            showState();
            return;
        }
        Q_EMIT closeRequested();
        // Queued, so that the dialog returns from its event loop first.
        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
    }

}
