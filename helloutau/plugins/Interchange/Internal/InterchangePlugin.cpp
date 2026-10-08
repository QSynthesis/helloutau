#include "InterchangePlugin.h"

#include <QtCore/QtGlobal>

#include <hellokit/Interchange/BuiltinInterchangeDrivers.h>
#include <hellokit/Interchange/InterchangeDrivers.h>

#include <helloutau/Editor/AppLoader.h>
#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/Translations.h>

#include <Interchange/InterchangeService.h>

#include "InterchangeContribution.h"
#include "MidiEncodingPage.h"

namespace hello::daw {

    namespace {

        const char pluginId[] = "org.helloutau.interchange";

    }

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
        m_service = std::make_unique<InterchangeService>();
        const auto &drivers = m_service->drivers();
        m_drivers = std::make_unique<kit::BuiltinInterchangeDrivers>(drivers.readerRegistry(),
                                                                     drivers.writerRegistry());
        m_midiEncoding = InterchangeStepRegistry::AddFactory(
            m_service->stepPages(), MidiEncodingPage::stepId, {},
            [] { return std::make_unique<MidiEncodingPage>(); });
        m_actions = ActionContributionRegistry::AddFactory(
            editor->actionContributions(), pluginId, {}, [service = m_service.get()] {
                return std::make_unique<InterchangeContribution>(service);
            });
        if (!m_actions.entry()) {
            if (errorMessage) {
                *errorMessage = "The actions of the plugin are already registered.";
            }
            return false;
        }
        return true;
    }

    // The registrations are destroyed before the service, because the actions reference it and
    // the registries of the others belong to it.
    void InterchangePlugin::aboutToShutdown() {
        m_actions = {};
        m_midiEncoding = {};
        m_drivers.reset();
        m_service.reset();
    }

}

STDC_EXPORT_PLUGIN(hello::daw::InterchangePlugin)
