#ifndef HELLOKIT_SYNTH_ENGINEPROCESS_H
#define HELLOKIT_SYNTH_ENGINEPROCESS_H

#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Synth/HelloKitSynthGlobal.h>

namespace hello::kit {

    /// Thread-safe output retained by the engines of one playback owner.
    class HELLOKIT_SYNTH_EXPORT EngineOutputLog {
    public:
        enum Mode {
            Latest,
            Accumulated,
        };

        EngineOutputLog();
        ~EngineOutputLog();

        QString text() const;
        void clear();
        void setMode(Mode mode);
        void setLimit(qsizetype bytes);
        void setFileName(const QString &fileName);
        void record(const std::filesystem::path &program, const QString &output);

    private:
        void trim();

        mutable std::mutex m_mutex;
        QString m_outputs;
        qsizetype m_limit = 1024 * 1024;
        Mode m_mode = Accumulated;
        bool m_runStarted = false;
        QString m_fileName;

        void writeFile();
        void loadFile();
    };

    /// The result of one engine invocation.
    struct EngineRun {
        /// Whether the program was started.
        ///
        /// Distinct from success, and the two require different messages to the user: a start
        /// failure indicates that the engine is not at the configured location, whereas a
        /// nonzero result indicates that the engine rejected its input.
        bool started = false;

        /// The exit code of the engine, meaningful only if it started.
        ///
        /// \note Engines are inconsistent in this respect. Some report nothing and some report
        ///       success regardless, so a caller that must know whether a render occurred checks
        ///       whether the output file appeared.
        int exitCode = 0;

        /// Whether the engine was killed because it exceeded the time limit.
        bool timedOut = false;

        /// Whether the script was killed, with the processes it started, because the caller
        /// cancelled it.
        bool cancelled = false;

        /// All output of the engine on both streams, for the diagnostic on failure.
        QString output;

        inline bool succeeded() const {
            return started && !timedOut && exitCode == 0;
        }
    };

    /// Runs one of the engines that a render consists of.
    ///
    /// **Arguments are passed as a vector and are never joined into a command line.** UTAU
    /// renders by writing and executing a batch file, which is a shell script: a sample path, an
    /// alias or a flags string containing \c & or a newline appends commands to it. This is the
    /// mechanism of CVE-2024-28886, and the reason there is deliberately no overload that takes
    /// a complete command line.
    ///
    /// \warning \a program is always the engine configured by the host, never one specified by
    ///          the project. A UST stores engine paths in \c Tool1 and \c Tool2 , and executing
    ///          them without confirmation lets the file choose which program runs. Storing them
    ///          is permitted. Executing them is not.
    ///
    /// See CLAUDE.md for both rules and the reason they are separate rules.
    class HELLOKIT_SYNTH_EXPORT EngineProcess {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::EngineProcess)
    public:
        explicit EngineProcess(std::shared_ptr<EngineOutputLog> outputLog = {});

        /// Virtual, as are the two functions below, so that a runner can be given a substitute
        /// that starts engines. The engines are third-party programs outside this repository,
        /// and without a substitute, no behavior of a runner after the arguments are passed can
        /// be tested.
        ///
        /// \sa SynthRunner::makeEngineProcess()
        virtual ~EngineProcess();

        /// The time limit of one call, in milliseconds, after which the engine is killed.
        ///
        /// Otherwise an engine that never returns would halt the render indefinitely, as it
        /// does in UTAU.
        int timeout = 30000;

        /// The working directory, or empty for that of the current process.
        std::filesystem::path workingDirectory;

        /// Runs \a program with \a arguments and waits for it to finish.
        ///
        /// \a program is used as given rather than searched for along \c PATH, so that the
        /// selected engine runs and not a program of the same name in an earlier directory.
        virtual EngineRun run(const std::filesystem::path &program, const QStringList &arguments,
                              DiagnosticList &diagnostics) const;

        /// Returns the timestamped output collected by this process's log.
        QString outputLog() const;

        /// Executes \a script with the command processor, in a visible console.
        ///
        /// The only function in this library that executes a command line, because a batch file
        /// is one. It resides here rather than in a separate class so that time limits and
        /// termination are handled in one place, and it takes a path rather than text so that
        /// no caller can pass an ad hoc command.
        ///
        /// \a cancelled, if given, is queried about every 100 milliseconds while the script
        /// runs. Once it returns true, the script and every process it started are killed,
        /// and \c EngineRun::cancelled is set. The time limit kills them all as well.
        ///
        /// \note The visible console matches UTAU behavior and is intentional: the output of
        ///       the script is meant to be read by the user. Nothing is captured, so
        ///       \c EngineRun::output is empty.
        ///
        /// \warning **The script writer is responsible for its content.** A batch file is a
        ///          shell script, so a lyric or a flags string written into it unescaped appends
        ///          commands. See \c ClassicSynthRunner, the only caller.
        virtual EngineRun runScript(const std::filesystem::path &script,
                                    DiagnosticList &diagnostics,
                                    const std::function<bool()> &cancelled = {}) const;

    private:
        std::shared_ptr<EngineOutputLog> m_outputLog;
    };

}

#endif // HELLOKIT_SYNTH_ENGINEPROCESS_H
