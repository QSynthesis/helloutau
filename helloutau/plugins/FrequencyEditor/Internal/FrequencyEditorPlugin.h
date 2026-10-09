#ifndef HELLOUTAU_FREQUENCYEDITOR_INTERNAL_FREQUENCYEDITORPLUGIN_H
#define HELLOUTAU_FREQUENCYEDITOR_INTERNAL_FREQUENCYEDITORPLUGIN_H

#include <memory>
#include <string>

#include <stdcorelib/pluginsystem/iplugin.h>

namespace hello::kit {
    class BuiltinFrequencyFormats;
}

namespace hello::daw {

    /// The plugin of frequency tables: registers the formats frq, dio and mrq, and later the
    /// editor of the tables. See docs/FrequencyTables.md.
    class FrequencyEditorPlugin : public stdc::pluginsystem::IPlugin {
    public:
        FrequencyEditorPlugin();
        ~FrequencyEditorPlugin();

        bool initialize(std::string *errorMessage) override;
        void aboutToShutdown() override;

    private:
        std::unique_ptr<kit::BuiltinFrequencyFormats> m_formats;
    };

}

#endif // HELLOUTAU_FREQUENCYEDITOR_INTERNAL_FREQUENCYEDITORPLUGIN_H
