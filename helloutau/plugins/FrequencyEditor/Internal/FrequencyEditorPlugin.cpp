#include "FrequencyEditorPlugin.h"

#include <QtCore/QtGlobal>

#include <hellokit/VoiceBank/BuiltinFrequencyFormats.h>

namespace hello::daw {

    FrequencyEditorPlugin::FrequencyEditorPlugin() = default;

    FrequencyEditorPlugin::~FrequencyEditorPlugin() = default;

    bool FrequencyEditorPlugin::initialize(std::string *errorMessage) {
        Q_UNUSED(errorMessage);
        m_formats = std::make_unique<kit::BuiltinFrequencyFormats>();
        return true;
    }

    void FrequencyEditorPlugin::aboutToShutdown() {
        m_formats.reset();
    }

}

STDC_EXPORT_PLUGIN(hello::daw::FrequencyEditorPlugin)
