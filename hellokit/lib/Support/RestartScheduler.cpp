#include "RestartScheduler.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QProcess>

namespace hello::kit {

    namespace {

        bool s_required = false;
        bool s_scheduled = false;

    }

    void RestartScheduler::requireRestart() {
        s_required = true;
    }

    bool RestartScheduler::isRestartRequired() {
        return s_required;
    }

    void RestartScheduler::clearRestartRequired() {
        s_required = false;
    }

    void RestartScheduler::scheduleRestart() {
        s_scheduled = true;
    }

    bool RestartScheduler::isRestartScheduled() {
        return s_scheduled;
    }

    bool RestartScheduler::relaunchIfScheduled(const QStringList &arguments) {
        if (!s_scheduled) {
            return false;
        }
        s_scheduled = false;
        // QProcess rather than stdc::Popen (author's decision, 2026-10-01): the new process only
        // needs to be started detached, without pipes or a return code.
        return QProcess::startDetached(QCoreApplication::applicationFilePath(), arguments);
    }

}
