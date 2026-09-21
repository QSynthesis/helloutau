#include "EngineProcess.h"

#include <string>
#include <vector>

#include <QtCore/QCoreApplication>

#include <stdcorelib/support/popen.h>

#include <hellokit/Support/TextCodec.h>

namespace hello::kit {

    namespace {

        QString tr(const char *text) {
            return QCoreApplication::translate("hello::kit::EngineProcess", text);
        }

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

    EngineRun EngineProcess::run(const std::filesystem::path &program,
                                 const QStringList &arguments,
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
            fail(diagnostics,
                 tr("The engine \"%1\" could not be started.").arg(displayed(program)));
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
            fail(diagnostics, tr("The engine \"%1\" did not finish within %2 seconds and was "
                                 "stopped.")
                                  .arg(displayed(program))
                                  .arg(timeout / 1000));
            return result;
        }

        result.exitCode = process.returnCode().value_or(-1);
        return result;
    }

}
