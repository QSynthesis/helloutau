#ifndef HELLOUTAU_FREQUENCYEDITOR_MORESAMPLERDIAGNOSER_H
#define HELLOUTAU_FREQUENCYEDITOR_MORESAMPLERDIAGNOSER_H

#include <filesystem>
#include <optional>

#include <QtCore/QCoreApplication>

#include <hellokit/Synth/SynthDiagnoser.h>

#include <FrequencyEditor/FrequencyEditorPluginGlobal.h>

namespace hello::daw {

    /// The diagnoser of moresampler with \c resampler-compatibility off, registered by the
    /// FrequencyEditor plugin. Moresampler then writes a placeholder text at the path of each
    /// fragment and the data beside it with the extension \c .llsm.tmp, which only moresampler
    /// as the wavtool joins. The diagnoser identifies such a fragment by the placeholder text.
    /// See docs/SynthDiagnostics.md.
    class FREQUENCYEDITORPLUGIN_EXPORT MoresamplerDiagnoser : public kit::SynthDiagnoser {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::MoresamplerDiagnoser)
    public:
        static constexpr char diagnoserId[] = "moresampler.compatibility";

        /// The start of the placeholder text that moresampler 0.8.4 writes at the path of a
        /// fragment
        static constexpr char placeholder[] = "This is a dummy file created just to fool UTAU";

        QString id() const override;

        /// A positive value, as for a diagnoser of a particular synth tool
        int priority() const override;

        QList<kit::SynthDiagnosis>
            diagnose(const kit::SynthDiagnosisContext &context) const override;

        /// Returns the value of \c resampler-compatibility in the file \c moreconfig.txt beside
        /// \a resampler, or \c std::nullopt if the file or the option is absent.
        static std::optional<bool> compatibility(const std::filesystem::path &resampler);
    };

}

#endif // HELLOUTAU_FREQUENCYEDITOR_MORESAMPLERDIAGNOSER_H
