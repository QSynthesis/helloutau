#ifndef HELLOKIT_SYNTH_ENGINEPROCESS_H
#define HELLOKIT_SYNTH_ENGINEPROCESS_H

#include <filesystem>

#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Synth/HelloKitSynthGlobal.h>

namespace hello::kit {

    /// What one run of an engine produced.
    struct EngineRun {
        /// Whether the program was started at all.
        ///
        /// Not the same as whether it worked, and the two want different things said to the
        /// user: one means the engine is not where it was said to be, the other means the
        /// engine did not like what it was given.
        bool started = false;

        /// What the engine exited with, meaningful only where it started.
        ///
        /// \note Engines are not consistent about this. Some report nothing and some report
        ///       success either way, so a caller that needs to know whether a render happened
        ///       looks at whether the file appeared.
        int exitCode = 0;

        /// Whether the engine was still running when the time ran out and was killed.
        bool timedOut = false;

        /// Everything the engine printed, on both streams, for the diagnostic when it failed.
        QString output;

        bool succeeded() const {
            return started && !timedOut && exitCode == 0;
        }
    };

    /// Runs one of the engines a render is made of.
    ///
    /// **Arguments go over as a vector and are never joined into a command line.** UTAU renders
    /// by writing a batch file and running it, and a batch file is a shell script: a sample
    /// path, an alias or a flags string holding \c & or a newline appends commands to it. That
    /// is the shape of CVE-2024-28886, and it is why there is deliberately no overload here
    /// that takes a whole command line.
    ///
    /// \warning \a program is whatever the host settled on, never what the project named. A UST
    ///          carries engine paths in \c Tool1 and \c Tool2 , and running those without asking
    ///          lets the file choose which program runs. Holding them is fine, running them is
    ///          not.
    ///
    /// \sa AGENTS.md, for both rules and why they are two rules rather than one
    class HELLOKIT_SYNTH_EXPORT EngineProcess {
    public:
        EngineProcess();
        ~EngineProcess();

        /// How long one call may take, in milliseconds, before the engine is killed.
        ///
        /// An engine that never returns would otherwise stop the render for good, which is what
        /// UTAU does.
        int timeout = 30000;

        /// The directory to run in, empty for this process's own.
        std::filesystem::path workingDirectory;

        /// Runs \a program with \a arguments and waits for it.
        ///
        /// \a program is named outright rather than looked up along \c PATH, so that what runs
        /// is the engine that was chosen and not whatever an earlier directory happens to hold.
        EngineRun run(const std::filesystem::path &program, const QStringList &arguments,
                      DiagnosticList &diagnostics) const;
    };

}

#endif // HELLOKIT_SYNTH_ENGINEPROCESS_H
