#include "BuiltinInterchangeDrivers.h"

#include "Formats/MidiConvert.h"

namespace hello::kit {

    BuiltinInterchangeDrivers::BuiltinInterchangeDrivers(InterchangeReaderRegistry &readers,
                                                         InterchangeWriterRegistry &writers)
        : m_midiReader(InterchangeReaderRegistry::Add<MidiReader>(readers, "midi", {})),
          m_midiWriter(InterchangeWriterRegistry::Add<MidiWriter>(writers, "midi", {})) {
    }

    BuiltinInterchangeDrivers::~BuiltinInterchangeDrivers() = default;

}
