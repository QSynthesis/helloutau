#ifndef HELLOKIT_SYNTH_CLASSICSYNTHRUNNER_H
#define HELLOKIT_SYNTH_CLASSICSYNTHRUNNER_H

#include <filesystem>
#include <optional>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>

#include <hellokit/Synth/HelloKitSynthGlobal.h>
#include <hellokit/Synth/SynthRunner.h>

namespace hello::kit {

    /// Renders in the traditional UTAU manner, by writing and executing \c temp.bat .
    ///
    /// The compatibility fallback. Some resamplers require the script to exist, and at least one
    /// reads its contents: moresampler inspects \c temp.bat to determine whether the current
    /// call is the last one. For such resamplers the script is an input rather than merely a
    /// means of starting a program, so no runner that omits it can substitute.
    ///
    /// The console remains visible, as in UTAU. The output of a batch plugin is meant to be
    /// read.
    ///
    /// The layout follows UTAU: a header of \c @set assignments, one block per note that sets
    /// the values of the note and calls \c temp_helper.bat , and a footer that joins the two
    /// fragments written by the wavtool into the track file. The **values** are computed by
    /// \c SynthPlan and placed exactly where the engines read them.
    ///
    /// \sa docs/Synth.md
    class HELLOKIT_SYNTH_EXPORT ClassicSynthRunner : public SynthRunner {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::ClassicSynthRunner)
    public:
        ClassicSynthRunner();
        ~ClassicSynthRunner() override;

        /// The method of writing project values into the script.
        enum class Quoting {
            /// Escaped, so that a value cannot become code.
            ///
            /// A batch file is a shell script: a lyric, an alias or a flags string containing
            /// \c & or a newline appends commands to it, which is the mechanism of
            /// CVE-2024-28886. Every assignment is therefore written as \c set \c "name=value"
            /// and every \c % is doubled, so that no project data can become a command.
            Escaped,

            /// Written exactly as UTAU writes it, without escaping.
            ///
            /// Only for an engine that requires the script text to match UTAU exactly, which is
            /// so far a hypothetical case with no known instance. **This lets the project file
            /// execute commands.** Never select it on behalf of the user. It must be selected by
            /// the user, in a prompt that explains the consequence.
            Verbatim,
        };

        Quoting quoting = Quoting::Escaped;

        /// The shell for which the script is written.
        enum class ScriptShell {
            /// \c temp.bat , executed by the Windows command processor.
            Batch,

            /// \c temp.sh , executed by \c /bin/sh . Same layout, with \c cat instead of
            /// \c copy , and single quotes instead of the \c set \c "name=value" form.
            Posix,
        };

        /// The shell of the host system unless set otherwise.
        ///
        /// \note Settable so that either script can be generated and inspected on either
        ///       system. The output for another platform must be verifiable without running it
        ///       there.
        ScriptShell shell = nativeShell();

        /// \c Batch on Windows and \c Posix on all other systems.
        static ScriptShell nativeShell();

        /// The directory for \c temp.bat and \c temp_helper.bat , or empty for a directory of
        /// this program under the system temporary directory.
        ///
        /// \note The two files keep their UTAU names, because an engine that searches for
        ///       \c temp.bat must find it.
        std::filesystem::path scriptDirectory;

        /// Whether the scripts are kept after the render, for inspection after a failure.
        bool keepScripts = false;

        SynthOutcome render(const SynthPlan &plan, const SynthEngines &engines,
                            SynthObserver *observer, DiagnosticList &diagnostics) const override;

        /// Returns the scripts generated from \a plan and \a engines , without writing or
        /// executing anything.
        ///
        /// Used by the tests, and for showing a user exactly what will be executed.
        ///
        /// \return the contents of \c temp.bat and \c temp_helper.bat , or \c std::nullopt if a
        ///         value cannot be written safely, with the reason in \a diagnostics
        std::optional<std::pair<QString, QString>> scripts(const SynthPlan &plan,
                                                           const SynthEngines &engines,
                                                           DiagnosticList &diagnostics) const;
    };

}

#endif // HELLOKIT_SYNTH_CLASSICSYNTHRUNNER_H
