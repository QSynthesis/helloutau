#ifndef HELLOUTAU_FREQUENCYEDITOR_INTERNAL_FREQUENCYEDITORPLUGIN_H
#define HELLOUTAU_FREQUENCYEDITOR_INTERNAL_FREQUENCYEDITORPLUGIN_H

#include <memory>
#include <string>

#include <stdcorelib/pluginsystem/iplugin.h>

#include <hellokit/Synth/SynthDiagnosers.h>

namespace hello::kit {
    class BuiltinFrequencyFormats;
}

namespace hello::daw {

    /// The plugin of frequency tables: registers the formats frq, dio and mrq, the diagnoser of
    /// moresampler, and later the editor of the tables. See docs/FrequencyTables.md and
    /// docs/SynthDiagnostics.md.
    class FrequencyEditorPlugin : public stdc::pluginsystem::IPlugin {
    public:
        FrequencyEditorPlugin();
        ~FrequencyEditorPlugin();

        bool initialize(std::string *errorMessage) override;
        void aboutToShutdown() override;

    private:
        std::unique_ptr<kit::BuiltinFrequencyFormats> m_formats;
        kit::SynthDiagnoserRegistry::AddFactory m_moresampler;
    };

}

#endif // HELLOUTAU_FREQUENCYEDITOR_INTERNAL_FREQUENCYEDITORPLUGIN_H
