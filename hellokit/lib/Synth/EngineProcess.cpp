#include "EngineProcess.h"

#ifdef _WIN32
#  include <QtCore/qt_windows.h>
#endif

#include <string>
#include <vector>

#include <QtCore/QCoreApplication>

#include <stdcorelib/support/popen.h>

#include <hellokit/Support/TextCodec.h>

namespace hello::kit {

    namespace {

        void fail(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Error, message});
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

    EngineProcess::EngineProcess() = default;

    EngineProcess::~EngineProcess() = default;

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
                                       DiagnosticList &diagnostics) const {
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
#endif

        if (!process.start()) {
            fail(diagnostics,
                 tr("The rendering script \"%1\" could not be started.").arg(displayed(script)));
            return result;
        }
        result.started = true;

        if (!process.wait(timeout)) {
            process.kill();
            process.wait();
            result.timedOut = true;
            fail(diagnostics, tr("The rendering script did not finish within %1 seconds and was "
                                 "stopped.")
                                  .arg(timeout / 1000));
            return result;
        }

        result.exitCode = process.returnCode().value_or(-1);
        return result;
    }
}
