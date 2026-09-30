#include "ClassicPluginHostPlugin.h"

#include <QtCore/QtGlobal>

namespace hello::daw {

    ClassicPluginHostPlugin::ClassicPluginHostPlugin() = default;

    ClassicPluginHostPlugin::~ClassicPluginHostPlugin() = default;

    bool ClassicPluginHostPlugin::initialize(std::string *errorMessage) {
        Q_UNUSED(errorMessage);
        return true;
    }

    void ClassicPluginHostPlugin::aboutToShutdown() {
    }

}

STDC_EXPORT_PLUGIN(hello::daw::ClassicPluginHostPlugin)
