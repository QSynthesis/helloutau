#ifndef HELLOKIT_SYNTH_SYNTHDIAGNOSER_H
#define HELLOKIT_SYNTH_SYNTHDIAGNOSER_H

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Synth/HelloKitSynthGlobal.h>
#include <hellokit/Synth/SynthFailure.h>
#include <hellokit/Synth/SynthRunner.h>

namespace hello::kit {

    /// A probable cause of failures of a render, with the remedies for the user.
    struct SynthDiagnosis {
        /// The cause in one sentence
        QString cause;

        QString explanation;

        /// The steps that remove the cause, each a sentence
        QStringList remedies;

        /// The indexes in SynthDiagnosisContext::failures of the failures that the cause
        /// explains
        QList<int> failures;
    };

    /// The data available to a diagnosis: the failures of one render and its configuration.
    struct SynthDiagnosisContext {
        /// The operation whose synth tool calls failed. The causes that apply and the wording of
        /// the diagnoses depend on it.
        enum Operation {
            Render,                 ///< The render of the notes of a project
            FrequencyMapGeneration, ///< The generation of frequency maps by the resampler
        };

        Operation operation = Render;

        /// The display name of the format of the frequency maps, in FrequencyMapGeneration
        QString frequencyFormat;

        /// The synth tools of the local settings, which the render ran
        SynthTools synthTools;

        /// Whether the fragments are joined by WavtoolMixer rather than by the wavtool, as in
        /// realtime playback. The mixer reads only RIFF/WAVE files.
        bool joinedInternally = false;

        QList<SynthFailure> failures;
    };

    /// Finds the probable causes of failures of a render. Registered in a
    /// SynthDiagnoserRegistry, by this library for the causes that concern no particular synth
    /// tool and by plugins for others. See docs/SynthDiagnostics.md.
    ///
    /// A diagnoser runs on the application thread, examines the records and at most a few small
    /// files, such as a configuration file beside a synth tool, and runs no program.
    class HELLOKIT_SYNTH_EXPORT SynthDiagnoser {
    public:
        virtual ~SynthDiagnoser();

        /// The ID, the name under which the diagnoser is registered
        virtual QString id() const = 0;

        /// Diagnosers of a higher priority run first, and a failure that one of them explains
        /// is not passed to the others. A diagnoser of a particular synth tool returns a
        /// positive value. The default is 0.
        virtual int priority() const;

        /// Returns the causes of failures in \a context, or an empty list if none applies.
        virtual QList<SynthDiagnosis> diagnose(const SynthDiagnosisContext &context) const = 0;
    };

}

#endif // HELLOKIT_SYNTH_SYNTHDIAGNOSER_H
