#ifndef HELLOUTAU_FREQUENCYEDITOR_INTERNAL_FREQUENCYEDITORPLUGIN_H
#define HELLOUTAU_FREQUENCYEDITOR_INTERNAL_FREQUENCYEDITORPLUGIN_H

#include <string>

#include <stdcorelib/pluginsystem/iplugin.h>

#include <hellokit/Synth/SynthDiagnosers.h>
#include <hellokit/VoiceBank/FrequencyFormats.h>

namespace hello::daw {

    /// The plugin of frequency maps: registers the formats dio and mrq, the diagnoser of
    /// moresampler, and later the editor of the maps. The editor registers frq itself. See
    /// docs/FrequencyMaps.md and docs/SynthDiagnostics.md.
    class FrequencyEditorPlugin : public stdc::pluginsystem::IPlugin {
    public:
        FrequencyEditorPlugin();
        ~FrequencyEditorPlugin();

        bool initialize(std::string *errorMessage) override;
        void aboutToShutdown() override;

    private:
        kit::FrequencyFormatRegistry::AddFactory m_dio;
        kit::FrequencyFormatRegistry::AddFactory m_mrq;
        kit::SynthDiagnoserRegistry::AddFactory m_moresampler;
    };

}

#endif // HELLOUTAU_FREQUENCYEDITOR_INTERNAL_FREQUENCYEDITORPLUGIN_H
