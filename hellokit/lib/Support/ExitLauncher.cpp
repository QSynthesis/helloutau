#include "ExitLauncher.h"

#include <algorithm>
#include <cassert>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QProcess>

namespace hello::kit {

    namespace {

        ExitLauncher *s_instance = nullptr;

    }

    ExitLauncher::ExitLauncher(const QStringList &restartArguments)
        : m_restartArguments(restartArguments) {
        assert(!s_instance);
        s_instance = this;
    }

    ExitLauncher::~ExitLauncher() {
        s_instance = nullptr;
    }

    ExitLauncher *ExitLauncher::instance() {
        return s_instance;
    }

    void ExitLauncher::setLaunch(const QString &key, const QString &program,
                                 const QStringList &arguments) {
        removeLaunch(key);
        m_launches.push_back({key, program, arguments});
    }

    void ExitLauncher::removeLaunch(const QString &key) {
        m_launches.removeIf([&key](const Launch &launch) { return launch.key == key; });
    }

    bool ExitLauncher::hasLaunch(const QString &key) const {
        return std::any_of(m_launches.begin(), m_launches.end(),
                           [&key](const Launch &launch) { return launch.key == key; });
    }

    void ExitLauncher::scheduleRestart() {
        setLaunch(QLatin1String(restartKey), QCoreApplication::applicationFilePath(),
                  m_restartArguments);
    }

    bool ExitLauncher::isRestartScheduled() const {
        return hasLaunch(QLatin1String(restartKey));
    }

    void ExitLauncher::requireRestart() {
        m_restartRequired = true;
    }

    bool ExitLauncher::isRestartRequired() const {
        return m_restartRequired;
    }

    void ExitLauncher::clearRestartRequired() {
        m_restartRequired = false;
    }

    int ExitLauncher::launchAll() {
        const auto launches = std::exchange(m_launches, {});
        int started = 0;
        for (const auto &launch : launches) {
            // QProcess rather than stdc::Popen (author's decision, 2026-10-01): each process only
            // needs to be started detached, without pipes or a return code.
            if (QProcess::startDetached(launch.program, launch.arguments)) {
                ++started;
            }
        }
        return started;
    }

}
