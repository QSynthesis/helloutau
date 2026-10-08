#ifndef HELLOKIT_INTERCHANGE_BUILTININTERCHANGEDRIVERS_H
#define HELLOKIT_INTERCHANGE_BUILTININTERCHANGEDRIVERS_H

#include <QtCore/QtGlobal>

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>
#include <hellokit/Interchange/InterchangeReaderRegistry.h>
#include <hellokit/Interchange/InterchangeWriterRegistry.h>

namespace hello::kit {

    /// Registrations of the drivers of this library, MidiReader in \a readers and MidiWriter in
    /// \a writers, for the lifetime of the object.
    ///
    /// The Interchange plugin owns an instance for the registries of its InterchangeDrivers. A
    /// test that requires these drivers owns its own instance.
    class HELLOKIT_INTERCHANGE_EXPORT BuiltinInterchangeDrivers {
    public:
        BuiltinInterchangeDrivers(InterchangeReaderRegistry &readers,
                                  InterchangeWriterRegistry &writers);
        ~BuiltinInterchangeDrivers();

    private:
        InterchangeReaderRegistry::AddFactory m_midiReader;
        InterchangeWriterRegistry::AddFactory m_midiWriter;

        Q_DISABLE_COPY_MOVE(BuiltinInterchangeDrivers)
    };

}

#endif // HELLOKIT_INTERCHANGE_BUILTININTERCHANGEDRIVERS_H
