#ifndef HELLOKIT_SYNTH_SYNTHDIAGNOSERS_H
#define HELLOKIT_SYNTH_SYNTHDIAGNOSERS_H

#include <memory>

#include <QtCore/QList>
#include <QtCore/QtGlobal>

#include <hellokit/Synth/HelloKitSynthGlobal.h>
#include <hellokit/Synth/SynthDiagnoser.h>
#include <hellokit/Synth/SynthDiagnoserRegistry.h>

namespace hello::kit {

    /// The diagnosers of render failures of a host, such as an editor: one instance of each
    /// diagnoser registered in registry(). See docs/SynthDiagnostics.md.
    class HELLOKIT_SYNTH_EXPORT SynthDiagnosers {
    public:
        SynthDiagnosers();
        ~SynthDiagnosers();

        /// Returns the registry in which the diagnosers of this object are registered.
        SynthDiagnoserRegistry &registry() const;

        /// Returns the diagnosers in the order in which they run: by priority, highest first,
        /// and in the order of registration among equal priorities.
        QList<SynthDiagnoser *> diagnosers() const;

        /// Runs the diagnosers on \a context in order. Each failure is explained at most once: a
        /// diagnoser receives only the failures that no earlier diagnoser explained. The
        /// indexes in the returned diagnoses refer to the failures of \a context.
        QList<SynthDiagnosis> diagnose(const SynthDiagnosisContext &context) const;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;

        Q_DISABLE_COPY_MOVE(SynthDiagnosers)
    };

}

#endif // HELLOKIT_SYNTH_SYNTHDIAGNOSERS_H
