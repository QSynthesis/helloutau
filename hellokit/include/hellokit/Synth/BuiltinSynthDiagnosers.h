#ifndef HELLOKIT_SYNTH_BUILTINSYNTHDIAGNOSERS_H
#define HELLOKIT_SYNTH_BUILTINSYNTHDIAGNOSERS_H

#include <vector>

#include <QtCore/QtGlobal>

#include <hellokit/Synth/HelloKitSynthGlobal.h>
#include <hellokit/Synth/SynthDiagnoserRegistry.h>

namespace hello::kit {

    /// Registrations of the diagnosers of this library in \a registry for the lifetime of the
    /// object, those that concern no particular synth tool. See docs/SynthDiagnostics.md.
    ///
    /// - \c codePage: a path that the ANSI code page of Windows cannot represent.
    /// - \c start: a synth tool that did not start.
    /// - \c exitStatus: an exit code of Windows that indicates a missing DLL, a wrong image
    ///   format or a crash.
    /// - \c timeout: a synth tool killed at the time limit.
    /// - \c longPath: a path longer than the 260 characters of Windows.
    /// - \c output: the remaining failures, by the stage and the examined file. Its priority is
    ///   the lowest, so that it explains only what no other diagnoser explains.
    class HELLOKIT_SYNTH_EXPORT BuiltinSynthDiagnosers {
    public:
        explicit BuiltinSynthDiagnosers(SynthDiagnoserRegistry &registry);
        ~BuiltinSynthDiagnosers();

    private:
        std::vector<SynthDiagnoserRegistry::AddFactory> m_registrations;

        Q_DISABLE_COPY_MOVE(BuiltinSynthDiagnosers)
    };

}

#endif // HELLOKIT_SYNTH_BUILTINSYNTHDIAGNOSERS_H
