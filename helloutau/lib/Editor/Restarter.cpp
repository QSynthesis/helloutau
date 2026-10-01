#include "Restarter.h"

#include <QtCore/QProcess>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>

#include "Editor.h"

namespace hello::daw {

    namespace {

        bool s_needed = false;
        bool s_restarting = false;

    }

    void Restarter::markNeeded() {
        s_needed = true;
    }

    bool Restarter::isNeeded() {
        return s_needed;
    }

    bool Restarter::offer(QWidget *parent, Editor *editor) {
        if (!s_needed) {
            return false;
        }
        s_needed = false;
        const auto answer =
            QMessageBox::question(parent, QApplication::applicationName(),
                                  tr("The changes take effect after %1 restarts. Restart now?")
                                      .arg(QApplication::applicationName()),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (answer != QMessageBox::Yes || !editor->closeAll()) {
            return false;
        }
        s_restarting = true;
        QApplication::quit();
        return true;
    }

    bool Restarter::isRestarting() {
        return s_restarting;
    }

    bool Restarter::startAgain(const QStringList &arguments) {
        if (!s_restarting) {
            return false;
        }
        s_restarting = false;
        // QProcess rather than stdc::Popen (author's decision, 2026-10-01): the new instance
        // only needs to be started detached, without pipes or a return code.
        return QProcess::startDetached(QCoreApplication::applicationFilePath(), arguments);
    }

}
