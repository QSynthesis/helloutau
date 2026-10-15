#ifndef HELLOUTAU_SYNTHTOOLSUPPORT_INTERNAL_SYNTHTOOLSUPPORTPLUGIN_H
#define HELLOUTAU_SYNTHTOOLSUPPORT_INTERNAL_SYNTHTOOLSUPPORTPLUGIN_H

#include <string>

#include <stdcorelib/pluginsystem/iplugin.h>

#include <hellokit/Synth/SynthDiagnosers.h>
#include <hellokit/VoiceBank/FrequencyFormats.h>

namespace hello::daw {

    /// The plugin that supports third-party synth tools: registers the formats dio, frc, mrq and
    /// pmk of frequency maps and the diagnoser of moresampler. The editor registers frq itself. See
    /// docs/FrequencyMaps.md and docs/SynthDiagnostics.md.
    class SynthToolSupportPlugin : public stdc::pluginsystem::IPlugin {
    public:
        SynthToolSupportPlugin();
        ~SynthToolSupportPlugin();

        bool initialize(std::string *errorMessage) override;
        void aboutToShutdown() override;

    private:
        kit::FrequencyFormatRegistry::AddFactory m_dio;
        kit::FrequencyFormatRegistry::AddFactory m_frc;
        kit::FrequencyFormatRegistry::AddFactory m_mrq;
        kit::FrequencyFormatRegistry::AddFactory m_pmk;
        kit::SynthDiagnoserRegistry::AddFactory m_moresampler;
    };

}

#endif // HELLOUTAU_SYNTHTOOLSUPPORT_INTERNAL_SYNTHTOOLSUPPORTPLUGIN_H
