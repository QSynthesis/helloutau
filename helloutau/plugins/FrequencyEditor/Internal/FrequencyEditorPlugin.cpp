#include "FrequencyEditorPlugin.h"

#include <QtCore/QtGlobal>

#include <hellokit/Support/TranslationLoader.h>
#include <hellokit/Synth/SynthDiagnosers.h>
#include <hellokit/VoiceBank/BuiltinFrequencyFormats.h>
#include <hellokit/VoiceBank/FrequencyFormats.h>

#include <helloutau/Editor/AppLoader.h>
#include <helloutau/Editor/Editor.h>

#include <FrequencyEditor/MoresamplerDiagnoser.h>

namespace hello::daw {

    FrequencyEditorPlugin::FrequencyEditorPlugin() = default;

    FrequencyEditorPlugin::~FrequencyEditorPlugin() = default;

    bool FrequencyEditorPlugin::initialize(std::string *errorMessage) {
        if (const auto translations = kit::TranslationLoader::instance()) {
            translations->load(QStringLiteral("FrequencyEditor"),
                               QStringLiteral(":/helloutau/plugins/FrequencyEditor/translations"));
        }
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
        m_moresampler = kit::SynthDiagnoserRegistry::Add<MoresamplerDiagnoser>(
            editor->synthDiagnosers().registry(), MoresamplerDiagnoser::diagnoserId, {});
        return true;
    }

    void FrequencyEditorPlugin::aboutToShutdown() {
        m_moresampler = {};
        m_formats.reset();
        // Removed before this library, which holds the translation, is unloaded
        if (const auto translations = kit::TranslationLoader::instance()) {
            translations->remove(QStringLiteral("FrequencyEditor"));
        }
    }

}

STDC_EXPORT_PLUGIN(hello::daw::FrequencyEditorPlugin)
