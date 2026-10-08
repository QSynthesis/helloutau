#include "FrequencyEditorPlugin.h"

#include <QtCore/QtGlobal>

#include <hellokit/VoiceBank/BuiltinFrequencyFormats.h>
#include <hellokit/VoiceBank/FrequencyFormats.h>

#include <helloutau/Editor/AppLoader.h>
#include <helloutau/Editor/Editor.h>

namespace hello::daw {

    FrequencyEditorPlugin::FrequencyEditorPlugin() = default;

    FrequencyEditorPlugin::~FrequencyEditorPlugin() = default;

    bool FrequencyEditorPlugin::initialize(std::string *errorMessage) {
        // The core plugin, on which this plugin depends, has created the editor.
        const auto loader = AppLoader::instance();
        const auto editor = loader ? loader->editor() : nullptr;
        if (!editor) {
            if (errorMessage) {
                *errorMessage = "The editor of the core plugin does not exist.";
            }
            return false;
        }
        m_formats =
            std::make_unique<kit::BuiltinFrequencyFormats>(editor->frequencyFormats().registry());
        return true;
    }

    void FrequencyEditorPlugin::aboutToShutdown() {
        m_formats.reset();
    }

}

STDC_EXPORT_PLUGIN(hello::daw::FrequencyEditorPlugin)
