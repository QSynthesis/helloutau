#include "EngineProcess.h"

#include <string>
#include <vector>

#include <QtCore/QCoreApplication>

#include <stdcorelib/support/popen.h>

#ifdef _WIN32
#  include <QtCore/qt_windows.h>
#endif

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

        /// What an engine printed, which on Windows is in the code page rather than UTF-8.
        ///
        /// Undecodable bytes are kept as Latin-1 rather than dropped. This is a message for a
        /// human to read when something went wrong, so showing it badly beats showing nothing.
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

        // args[0] is the name the program is given, and executable() is the file that runs. They
        // are set apart here on purpose: leaving args[0] to name the file as well would send it
        // through a PATH lookup.
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

        if (!process.start()) {
            fail(diagnostics, EngineProcess::tr("The engine \"%1\" could not be started.")
                                  .arg(displayed(program)));
            return result;
        }
        result.started = true;

        // Both streams are read here rather than by hand. One pipe blocks its writer once full,
        // so draining them one after the other deadlocks on an engine that talks a lot.
        const auto [out, err] = process.communicate({}, timeout);
        result.output = printed(out) + printed(err);

        // Not the return code: communicate() kills a child that outlasted the timeout, so it is
        // reaped and has one either way. The error is what says the engine was stopped rather
        // than finished.
        if (process.errorCode() == std::errc::timed_out) {
            result.timedOut = true;
            fail(diagnostics,
                 EngineProcess::tr("The engine \"%1\" did not finish within %2 seconds and was "
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
        // The argument vector still holds, even here. shell() quotes each element for the
        // platform shell rather than taking a command line, so the path of the script is one
        // argument whatever is in it.
        process.shell(true);
        process.args({script.u8string()});
        if (!workingDirectory.empty()) {
            process.cwd(workingDirectory);
        }

#ifdef _WIN32
        // Left on screen on purpose. UTAU shows it, a batch plugin's output is meant to be read,
        // and shell() hides it unless this says otherwise.
        stdc::Popen::StartupInfo info{};
        info.dwFlags = STARTF_USESHOWWINDOW;
        info.wShowWindow = SW_SHOWNORMAL;
        process.startupInfo(info);
#endif

        if (!process.start()) {
            fail(diagnostics, EngineProcess::tr("The rendering script \"%1\" could not be started.")
                                  .arg(displayed(script)));
            return result;
        }
        result.started = true;

        if (!process.wait(timeout)) {
            process.kill();
            process.wait();
            result.timedOut = true;
            fail(diagnostics,
                 EngineProcess::tr("The rendering script did not finish within %1 seconds and was "
                                   "stopped.")
                     .arg(timeout / 1000));
            return result;
        }

        result.exitCode = process.returnCode().value_or(-1);
        return result;
    }
}
