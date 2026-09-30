#include "InterchangePlugin.h"

#include <QtCore/QtGlobal>

#include <hellokit/Interchange/BuiltinInterchangeDrivers.h>

namespace hello::daw {

    InterchangePlugin::InterchangePlugin() = default;

    InterchangePlugin::~InterchangePlugin() = default;

    bool InterchangePlugin::initialize(std::string *errorMessage) {
        Q_UNUSED(errorMessage);
        m_drivers = std::make_unique<kit::BuiltinInterchangeDrivers>();
        return true;
    }

    void InterchangePlugin::aboutToShutdown() {
        m_drivers.reset();
    }

}

STDC_EXPORT_PLUGIN(hello::daw::InterchangePlugin)
