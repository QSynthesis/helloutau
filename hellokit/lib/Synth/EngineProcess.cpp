#include "EngineProcess.h"

#ifdef _WIN32
#  include <QtCore/qt_windows.h>
#  include <tlhelp32.h>
#else
#  include <signal.h>
#endif

#include <algorithm>
#include <chrono>
#include <string>
#include <mutex>
#include <utility>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>

#include <stdcorelib/support/popen.h>

#include <hellokit/Support/TextCodec.h>

namespace hello::kit {

    namespace {

        void fail(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Error, message});
        }

        // The interval in milliseconds at which a running script is checked for cancellation
        constexpr int ScriptPollInterval = 100;

        // Kills the script of process and the engines it started, which a kill of the script
        // alone would leave running. On Windows these are the descendants of the command
        // processor, found by the process ID of the parent that Windows records for each
        // process and keeps after the parent ends. Elsewhere they are the process group of the
        // script.
        void killTree(stdc::Popen &process) {
#ifdef _WIN32
            const auto root = DWORD(process.pid());
            // The command processor first, so that it starts nothing more
            process.kill();
            process.wait();
            const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snapshot == INVALID_HANDLE_VALUE) {
                return;
            }
            std::vector<std::pair<DWORD, DWORD>> processes;
            PROCESSENTRY32W entry{};
            entry.dwSize = sizeof(entry);
            for (BOOL found = Process32FirstW(snapshot, &entry); found;
                 found = Process32NextW(snapshot, &entry)) {
                processes.push_back({entry.th32ProcessID, entry.th32ParentProcessID});
            }
            CloseHandle(snapshot);
            std::vector<DWORD> tree{root};
            for (size_t i = 0; i < tree.size(); ++i) {
                for (const auto &[id, parent] : processes) {
                    if (parent == tree[i] &&
                        std::find(tree.begin(), tree.end(), id) == tree.end()) {
                        tree.push_back(id);
                    }
                }
            }
            for (size_t i = 1; i < tree.size(); ++i) {
                if (const auto handle = OpenProcess(PROCESS_TERMINATE, FALSE, tree[i])) {
                    TerminateProcess(handle, 1);
                    CloseHandle(handle);
                }
            }
#else
            ::kill(-pid_t(process.pid()), SIGKILL);
            process.wait();
#endif
        }

        QString displayed(const std::filesystem::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

        std::string utf8(const QString &text) {
            const auto bytes = text.toUtf8();
            return std::string(bytes.constData(), size_t(bytes.size()));
        }

        /// The output of an engine, which on Windows is in the ANSI code page rather than UTF-8.
        ///
        /// Invalid bytes are interpreted as Latin-1 rather than dropped. The output is a message
        /// for a person diagnosing a failure, so an imperfect rendering is preferable to none.
        QString printed(const std::string &bytes) {
            const QByteArrayView view(bytes.data(), qsizetype(bytes.size()));
            const TextCodec codec;
            if (const auto text = codec.decode(view)) {
                return *text;
            }
            return QString::fromLatin1(view);
        }

    }

    EngineOutputLog::EngineOutputLog() = default;

    EngineOutputLog::~EngineOutputLog() = default;

    QString EngineOutputLog::text() const {
        const std::lock_guard lock(m_mutex);
        return m_outputs;
    }

    void EngineOutputLog::clear() {
        const std::lock_guard lock(m_mutex);
        m_outputs.clear();
        m_runStarted = false;
    }

    void EngineOutputLog::setMode(Mode mode) {
        const std::lock_guard lock(m_mutex);
        m_mode = mode;
        m_runStarted = false;
    }

    void EngineOutputLog::setLimit(qsizetype bytes) {
        const std::lock_guard lock(m_mutex);
        m_limit = std::max<qsizetype>(1024, bytes);
        trim();
    }

    void EngineOutputLog::trim() {
        const auto bytes = m_outputs.toUtf8();
        if (bytes.size() > m_limit) {
            m_outputs = QString::fromUtf8(bytes.right(m_limit));
        }
    }

    void EngineOutputLog::record(const std::filesystem::path &program, const QString &output) {
        const auto name = QString::fromStdU16String(program.u16string());
        const auto time = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
        const auto body = output.isEmpty() ? QStringLiteral("(no output)") : output;
        const auto entry = QStringLiteral("[%1] %2\n%3\n").arg(time, name, body);
        const std::lock_guard lock(m_mutex);
        if (m_mode == Latest && !m_runStarted) {
            m_outputs.clear();
        }
        m_runStarted = true;
        m_outputs += entry;
        trim();
    }

    EngineProcess::EngineProcess(std::shared_ptr<EngineOutputLog> outputLog)
        : m_outputLog(outputLog ? std::move(outputLog) : std::make_shared<EngineOutputLog>()) {
    }

    EngineProcess::~EngineProcess() = default;

    QString EngineProcess::outputLog() const {
        return m_outputLog->text();
    }

    EngineRun EngineProcess::run(const std::filesystem::path &program, const QStringList &arguments,
                                 DiagnosticList &diagnostics) const {
        EngineRun result;

        // args[0] is the name passed to the program, and executable() is the file executed. They
        // are set separately on purpose, because using args[0] as the file name as well would
        // subject it to a PATH lookup.
        std::vector<std::string> args;
        args.reserve(size_t(arguments.size()) + 1);
        args.push_back(program.filename().u8string());
        for (const auto &argument : arguments) {
            args.push_back(utf8(argument));
        }

        stdc::Popen process;
        process.executable(program);
        process.args(std::move(args));
        process.standardOutput(stdc::Popen::Pipe);
        process.standardError(stdc::Popen::StandardOutput);
        if (!workingDirectory.empty()) {
            process.cwd(workingDirectory);
        }
#ifdef _WIN32
        // Engines are console programs. Started from a program without a console, as the
        // editor is, each would open a console window of its own, two for every note.
        process.creationFlags(CREATE_NO_WINDOW);
#endif

        if (!process.start()) {
            fail(diagnostics,
                 tr("The engine \"%1\" could not be started.").arg(displayed(program)));
            return result;
        }
        result.started = true;

        // Both streams are read by communicate() rather than manually. A full pipe blocks its
        // writer, so draining the streams sequentially deadlocks with a verbose engine.
        const auto [out, err] = process.communicate({}, timeout);
        result.output = printed(out) + printed(err);
        m_outputLog->record(program, result.output);

        // Not the exit code: communicate() kills a child that exceeds the time limit, so the
        // child is reaped and has an exit code in either case. The error indicates that the
        // engine was stopped rather than finished.
        if (process.errorCode() == std::errc::timed_out) {
            result.timedOut = true;
            fail(diagnostics, tr("The engine \"%1\" did not finish within %2 seconds and was "
                                 "stopped.")
                                  .arg(displayed(program))
                                  .arg(timeout / 1000));
            return result;
        }

        result.exitCode = process.returnCode().value_or(-1);
        return result;
    }

    EngineRun EngineProcess::runScript(const std::filesystem::path &script,
                                       DiagnosticList &diagnostics,
                                       const std::function<bool()> &cancelled) const {
        EngineRun result;

        stdc::Popen process;
        // The argument vector applies here as well. shell() quotes each element for the
        // platform shell rather than accepting a command line, so the script path remains a
        // single argument regardless of its content.
        process.shell(true);
        process.args({script.u8string()});
        if (!workingDirectory.empty()) {
            process.cwd(workingDirectory);
        }

#ifdef _WIN32
        // Deliberately visible. UTAU shows the console, the output of a batch plugin is
        // intended to be read, and shell() hides the console unless instructed otherwise.
        stdc::Popen::StartupInfo info{};
        info.dwFlags = STARTF_USESHOWWINDOW;
        info.wShowWindow = SW_SHOWNORMAL;
        process.startupInfo(info);
#else
        // A process group of its own, which killTree() kills with the engines in it
        process.processGroup(0);
#endif

        if (!process.start()) {
            fail(diagnostics,
                 tr("The rendering script \"%1\" could not be started.").arg(displayed(script)));
            return result;
        }
        result.started = true;

        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(0, timeout));
        while (!process.wait(ScriptPollInterval)) {
            if (cancelled && cancelled()) {
                killTree(process);
                result.cancelled = true;
                return result;
            }
            if (timeout >= 0 && std::chrono::steady_clock::now() >= deadline) {
                killTree(process);
                result.timedOut = true;
                fail(diagnostics, tr("The rendering script did not finish within %1 seconds and "
                                     "was stopped.")
                                      .arg(timeout / 1000));
                return result;
            }
        }

        result.exitCode = process.returnCode().value_or(-1);
        return result;
    }

}
