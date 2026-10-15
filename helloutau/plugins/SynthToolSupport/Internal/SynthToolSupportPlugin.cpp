#include "SynthToolSupportPlugin.h"

#include <QtCore/QtGlobal>

#include <hellokit/Support/TranslationLoader.h>
#include <hellokit/Synth/SynthDiagnosers.h>
#include <hellokit/VoiceBank/FrequencyFormats.h>

#include <helloutau/Editor/AppLoader.h>
#include <helloutau/Editor/Editor.h>

#include <SynthToolSupport/DioFormat.h>
#include <SynthToolSupport/FrcFormat.h>
#include <SynthToolSupport/MoresamplerDiagnoser.h>
#include <SynthToolSupport/MrqFormat.h>
#include <SynthToolSupport/PmkFormat.h>

namespace hello::daw {

    SynthToolSupportPlugin::SynthToolSupportPlugin() = default;

    SynthToolSupportPlugin::~SynthToolSupportPlugin() = default;

    bool SynthToolSupportPlugin::initialize(std::string *errorMessage) {
        if (const auto translations = kit::TranslationLoader::instance()) {
            translations->load(QStringLiteral("SynthToolSupport"),
                               QStringLiteral(":/helloutau/plugins/SynthToolSupport/translations"));
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
        auto &formats = editor->frequencyFormats().registry();
        m_dio = kit::FrequencyFormatRegistry::Add<DioFormat>(formats, DioFormat::formatId, {});
        m_frc = kit::FrequencyFormatRegistry::Add<FrcFormat>(formats, FrcFormat::formatId, {});
        m_mrq = kit::FrequencyFormatRegistry::Add<MrqFormat>(formats, MrqFormat::formatId, {});
        m_pmk = kit::FrequencyFormatRegistry::Add<PmkFormat>(formats, PmkFormat::formatId, {});
        m_moresampler = kit::SynthDiagnoserRegistry::Add<MoresamplerDiagnoser>(
            editor->synthDiagnosers().registry(), MoresamplerDiagnoser::diagnoserId, {});
        return true;
    }

    void SynthToolSupportPlugin::aboutToShutdown() {
        m_moresampler = {};
        m_pmk = {};
        m_mrq = {};
        m_frc = {};
        m_dio = {};
        // Removed before this library, which holds the translation, is unloaded
        if (const auto translations = kit::TranslationLoader::instance()) {
            translations->remove(QStringLiteral("SynthToolSupport"));
        }
    }

}

STDC_EXPORT_PLUGIN(hello::daw::SynthToolSupportPlugin)
