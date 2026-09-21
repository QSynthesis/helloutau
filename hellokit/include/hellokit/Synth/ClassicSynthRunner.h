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

    /// Renders the way UTAU has always rendered: by writing \c temp.bat and running it.
    ///
    /// The fallback. Some resamplers want the script to be there, and at least one reads what is
    /// in it: moresampler looks at \c temp.bat to work out whether the call it is serving is the
    /// last one. For those, the script is an input and not merely a way of starting a program,
    /// so nothing that skips it can stand in.
    ///
    /// The console is left on screen, as UTAU leaves it. A batch plugin's output is meant to be
    /// read.
    ///
    /// The layout follows UTAU's: a header of \c @set assignments, one block per note that sets
    /// the note's own values and calls \c temp_helper.bat , and a footer that joins the
    /// wavtool's two pieces into the track wav. The **values** are this program's own, worked
    /// out by \c SynthPlan, and land in exactly the positions the engines read them from.
    ///
    /// \sa docs/Synth.md
    class HELLOKIT_SYNTH_EXPORT ClassicSynthRunner : public SynthRunner {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::ClassicSynthRunner)
    public:
        ClassicSynthRunner();
        ~ClassicSynthRunner() override;

        /// How a value out of the project is written into the script.
        enum class Quoting {
            /// Escaped, so that a value stays a value.
            ///
            /// A batch file is a shell script: a lyric, an alias or a flags string holding
            /// \c & or a newline appends commands to it, which is the shape of CVE-2024-28886.
            /// Here every assignment is written as \c set \c "name=value" and every \c % is
            /// doubled, so nothing a project carries can become a command.
            Escaped,

            /// Written as UTAU writes it, escaping and all left out.
            ///
            /// Only for an engine that turns out to need the script's text to look exactly as it
            /// does under UTAU, which so far is a possibility rather than a case anyone has hit.
            /// **It hands the project file the power to run commands.** Never select it for a
            /// user: it is for a user to select, in a prompt that says what it means.
            Verbatim,
        };

        Quoting quoting = Quoting::Escaped;

        /// Which shell the script is written for.
        enum class ScriptShell {
            /// \c temp.bat , read by the Windows command processor.
            Batch,

            /// \c temp.sh , read by \c /bin/sh . Same layout, \c cat in place of \c copy , and
            /// single quotes in place of the \c set \c "name=value" form.
            Posix,
        };

        /// The shell of the system this is running on, unless something says otherwise.
        ///
        /// \note Settable so that either script can be read back on either system. What a
        ///       renderer writes for the other platform is not something to find out only when
        ///       somebody runs it there.
        ScriptShell shell = nativeShell();

        /// \c Batch on Windows and \c Posix everywhere else.
        static ScriptShell nativeShell();

        /// Where \c temp.bat and \c temp_helper.bat are written, empty for a folder of this
        /// program's own under the system temporary directory.
        ///
        /// \note The two files keep UTAU's names. An engine that goes looking for \c temp.bat
        ///       has to find one.
        std::filesystem::path scriptDirectory;

        /// Whether the scripts are kept after the render, for looking at when something went
        /// wrong.
        bool keepScripts = false;

        SynthOutcome render(const SynthPlan &plan, const SynthEngines &engines,
                            SynthObserver *observer, DiagnosticList &diagnostics) const override;

        /// The script that \a plan and \a engines come to, without writing or running anything.
        ///
        /// What the tests read, and what a user asking "what is it actually going to run" is
        /// shown.
        ///
        /// \return the contents of \c temp.bat and of \c temp_helper.bat , or nothing where a
        ///         value cannot be written safely, with the reason in \a diagnostics
        std::optional<std::pair<QString, QString>> scripts(const SynthPlan &plan,
                                                           const SynthEngines &engines,
                                                           DiagnosticList &diagnostics) const;
    };

}

#endif // HELLOKIT_SYNTH_CLASSICSYNTHRUNNER_H
