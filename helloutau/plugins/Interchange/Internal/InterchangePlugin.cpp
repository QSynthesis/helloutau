#include "InterchangePlugin.h"

#include <QtCore/QtGlobal>

#include <hellokit/Interchange/BuiltinInterchangeDrivers.h>
#include <hellokit/Interchange/InterchangeRegistry.h>

#include <helloutau/Editor/ActionRegistration.h>
#include <helloutau/Editor/Translations.h>

#include <Interchange/InterchangeStepRegistration.h>

#include "InterchangeContribution.h"
#include "MidiEncodingPage.h"

namespace hello::daw {

    InterchangePlugin::InterchangePlugin() = default;

    InterchangePlugin::~InterchangePlugin() = default;

    bool InterchangePlugin::initialize(std::string *errorMessage) {
        Q_UNUSED(errorMessage);
        Translations::load(QStringLiteral("Interchange"),
                           QStringLiteral(":/helloutau/plugins/Interchange/translations"));
        m_drivers = std::make_unique<kit::BuiltinInterchangeDrivers>();
        m_registry = std::make_unique<kit::InterchangeRegistry>();
        m_midiEncoding = std::make_unique<InterchangeStepRegistration>(
            QString::fromLatin1(MidiEncodingPage::stepId), [] { return new MidiEncodingPage(); });
        m_actions = std::make_unique<ActionRegistration>(
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
