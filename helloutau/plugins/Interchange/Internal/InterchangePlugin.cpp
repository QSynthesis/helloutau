#include "InterchangePlugin.h"

#include <QtCore/QtGlobal>

#include <hellokit/Interchange/BuiltinInterchangeDrivers.h>
#include <hellokit/Interchange/InterchangeRegistry.h>

#include <helloutau/Editor/AppLoader.h>
#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/Translations.h>
#include <helloutau/Widgets/ActionRegistration.h>

#include <Interchange/InterchangeStepRegistration.h>

#include "InterchangeContribution.h"
#include "MidiEncodingPage.h"

namespace hello::daw {

    InterchangePlugin::InterchangePlugin() = default;

    InterchangePlugin::~InterchangePlugin() = default;

    bool InterchangePlugin::initialize(std::string *errorMessage) {
        Translations::load(QStringLiteral("Interchange"),
                           QStringLiteral(":/helloutau/plugins/Interchange/translations"));
        // The core plugin, on which this plugin depends, has created the editor.
        const auto loader = AppLoader::instance();
        const auto editor = loader ? loader->editor() : nullptr;
        if (!editor) {
            if (errorMessage) {
                *errorMessage = "The editor of the core plugin does not exist.";
            }
            return false;
        }
        m_drivers = std::make_unique<kit::BuiltinInterchangeDrivers>();
        m_registry = std::make_unique<kit::InterchangeRegistry>();
        m_midiEncoding = std::make_unique<InterchangeStepRegistration>(
            QString::fromLatin1(MidiEncodingPage::stepId), [] { return new MidiEncodingPage(); });
        m_actions = std::make_unique<ActionRegistration>(
            editor->actionContributionRegistry(),
            std::make_unique<InterchangeContribution>(m_registry.get()));
        return true;
    }

    // The actions are destroyed first because they reference the registry.
    void InterchangePlugin::aboutToShutdown() {
        m_actions.reset();
        m_midiEncoding.reset();
        m_registry.reset();
        m_drivers.reset();
    }

}

STDC_EXPORT_PLUGIN(hello::daw::InterchangePlugin)
