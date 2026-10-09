#include "SynthToolProcess.h"

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
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QFile>

#include <stdcorelib/support/popen.h>

#include <hellokit/Support/TextCodec.h>

namespace hello::kit {

    namespace {

        void fail(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Error, message});
        }

        // The interval in milliseconds at which a running synth tool or script is checked for
        // cancellation and for its time limit
        constexpr int PollInterval = 100;

        // Kills the script of process and the synth tools it started, which a kill of the script
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

        /// The output of a synth tool, which on Windows is in the ANSI code page rather than UTF-8.
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

        // Returns the offset in data of a tail of at most bytes: the start of the first line in
        // it, or else the start of the first UTF-8 sequence in it.
        qsizetype tailStart(const QByteArray &data, qsizetype bytes) {
            auto start = std::max<qsizetype>(0, data.size() - bytes);
            if (start == 0) {
                return 0;
            }
            if (const auto newline = data.indexOf('\n', start - 1);
                newline >= 0 && newline + 1 < data.size()) {
                return newline + 1;
            }
            while (start < data.size() && (uchar(data.at(start)) & 0xC0) == 0x80) {
                ++start;
            }
            return start;
        }

    }

    SynthToolOutputLog::SynthToolOutputLog() = default;

    SynthToolOutputLog::~SynthToolOutputLog() = default;

    QString SynthToolOutputLog::text() const {
        const std::lock_guard lock(m_mutex);
        return QString::fromUtf8(read());
    }

    void SynthToolOutputLog::clear() {
        const std::lock_guard lock(m_mutex);
        m_runStarted = false;
        write({});
    }

    void SynthToolOutputLog::setMode(Mode mode) {
        const std::lock_guard lock(m_mutex);
        m_mode = mode;
        m_runStarted = false;
    }

    void SynthToolOutputLog::setLimit(qsizetype bytes) {
        const std::lock_guard lock(m_mutex);
        m_limit = std::max<qsizetype>(1024, bytes);
        if (const auto data = read(); data.size() > m_limit) {
            write(data.mid(tailStart(data, m_limit / 2)));
        }
    }

    void SynthToolOutputLog::setFileName(const QString &fileName) {
        const std::lock_guard lock(m_mutex);
        const auto data = read();
        m_outputs.clear();
        m_fileName = fileName;
        write(data);
    }

    void SynthToolOutputLog::record(const std::filesystem::path &program, const QString &output) {
        const auto name = QString::fromStdU16String(program.u16string());
        const auto time = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
        const auto body = output.isEmpty() ? QStringLiteral("(no output)") : output;
        const auto entry = QStringLiteral("[%1] %2\n%3\n").arg(time, name, body).toUtf8();
        const std::lock_guard lock(m_mutex);
        if (m_mode == Latest && !m_runStarted) {
            write({});
        }
        m_runStarted = true;
        append(entry);
    }

    QByteArray SynthToolOutputLog::read() const {
        if (m_fileName.isEmpty()) {
            return m_outputs;
        }
        QFile file(m_fileName);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    void SynthToolOutputLog::write(const QByteArray &data) {
        if (m_fileName.isEmpty()) {
            m_outputs = data;
            return;
        }
        QFile file(m_fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(data);
        }
    }

    void SynthToolOutputLog::append(const QByteArray &entry) {
        qint64 size = 0;
        if (m_fileName.isEmpty()) {
            m_outputs += entry;
            size = m_outputs.size();
        } else {
            QFile file(m_fileName);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
                return;
            }
            file.write(entry);
            size = file.size();
        }
        if (size > m_limit) {
            const auto data = read();
            write(data.mid(tailStart(data, m_limit / 2)));
        }
    }

    SynthToolProcess::SynthToolProcess(std::shared_ptr<SynthToolOutputLog> outputLog)
        : m_outputLog(outputLog ? std::move(outputLog) : std::make_shared<SynthToolOutputLog>()) {
    }

    SynthToolProcess::~SynthToolProcess() = default;

    QString SynthToolProcess::outputLog() const {
        return m_outputLog->text();
    }

    SynthToolRun SynthToolProcess::run(const std::filesystem::path &program,
                                       const QStringList &arguments, DiagnosticList &diagnostics,
                                       const std::function<bool()> &cancelled) const {
        SynthToolRun result;

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
        // Synth tools are console programs. Started from a program without a console, as the
        // editor is, each would open a console window of its own, two for every note.
        process.creationFlags(CREATE_NO_WINDOW);
#endif

        if (!process.start()) {
            fail(diagnostics,
                 tr("The synth tool \"%1\" could not be started.").arg(displayed(program)));
            return result;
        }
        result.started = true;

        // Both streams are read by communicate() rather than manually. A full pipe blocks its
        // writer, so draining the streams sequentially deadlocks with a verbose synth tool. Each
        // call is limited to the poll interval, and the next call resumes the exchange.
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(0, timeout));
        std::optional<std::tuple<std::string, std::string>> exchanged;
        bool timedOut = false;
        bool stopped = false;
        for (;;) {
            int slice = PollInterval;
            if (timeout >= 0) {
                const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                                           deadline - std::chrono::steady_clock::now())
                                           .count();
                if (remaining <= 0) {
                    timedOut = true;
                    break;
                }
                slice = int(std::min<qint64>(slice, remaining));
            }
            exchanged = process.communicate({}, slice);
            if (exchanged || process.errorCode() != std::errc::timed_out) {
                break;
            }
            if (cancelled && cancelled()) {
                stopped = true;
                break;
            }
        }
        if (timedOut || stopped) {
            // communicate() leaves the synth tool running. The kill closes its end of the pipe, and
            // the next call returns the output written before the kill.
            std::ignore = process.kill();
            exchanged = process.communicate();
        }
        if (exchanged) {
            const auto &[out, err] = *exchanged;
            result.output = printed(out) + printed(err);
        }
        m_outputLog->record(program, result.output);

        if (stopped) {
            result.cancelled = true;
            return result;
        }
        if (timedOut) {
            result.timedOut = true;
            fail(diagnostics, tr("The synth tool \"%1\" did not finish within %2 seconds and was "
                                 "stopped.")
                                  .arg(displayed(program))
                                  .arg(timeout / 1000));
            return result;
        }

        result.exitCode = process.returnCode().value_or(-1);
        return result;
    }

    SynthToolRun SynthToolProcess::runScript(const std::filesystem::path &script,
                                             DiagnosticList &diagnostics,
                                             const std::function<bool()> &cancelled) const {
        SynthToolRun result;

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
        // A process group of its own, which killTree() kills with the synth tools in it
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
        while (!process.wait(PollInterval)) {
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
