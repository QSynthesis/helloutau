#include "BuiltinInterchangeDrivers.h"

#include "Formats/MidiConvert.h"
#include "InterchangeRegistration.h"

namespace hello::kit {

    BuiltinInterchangeDrivers::BuiltinInterchangeDrivers() {
        m_registrations.push_back(
            std::make_unique<InterchangeRegistration>(std::make_unique<MidiReader>()));
        m_registrations.push_back(
            std::make_unique<InterchangeRegistration>(std::make_unique<MidiWriter>()));
    }

    BuiltinInterchangeDrivers::~BuiltinInterchangeDrivers() = default;

}
