#include "ClassicPluginHostPlugin.h"

#include <QtCore/QtGlobal>

#include <helloutau/Editor/ActionRegistration.h>

#include "ClassicPluginContribution.h"

namespace hello::daw {

    ClassicPluginHostPlugin::ClassicPluginHostPlugin() = default;

    ClassicPluginHostPlugin::~ClassicPluginHostPlugin() = default;

    bool ClassicPluginHostPlugin::initialize(std::string *errorMessage) {
        Q_UNUSED(errorMessage);
        m_registration =
            std::make_unique<ActionRegistration>(std::make_unique<ClassicPluginContribution>());
        return true;
    }

    void ClassicPluginHostPlugin::aboutToShutdown() {
        m_registration.reset();
    }

}

STDC_EXPORT_PLUGIN(hello::daw::ClassicPluginHostPlugin)
