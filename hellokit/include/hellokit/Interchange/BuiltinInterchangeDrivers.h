#ifndef HELLOKIT_INTERCHANGE_BUILTININTERCHANGEDRIVERS_H
#define HELLOKIT_INTERCHANGE_BUILTININTERCHANGEDRIVERS_H

#include <memory>
#include <vector>

#include <QtCore/QtGlobal>

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>

namespace hello::kit {

    class InterchangeRegistration;

    /// Registrations of the drivers of this library, MidiReader and MidiWriter, for the lifetime
    /// of the object.
    ///
    /// The Interchange plugin owns an instance. A test that requires these drivers owns its own
    /// instance.
    class HELLOKIT_INTERCHANGE_EXPORT BuiltinInterchangeDrivers {
    public:
        BuiltinInterchangeDrivers();
        ~BuiltinInterchangeDrivers();

    private:
        std::vector<std::unique_ptr<InterchangeRegistration>> m_registrations;

        Q_DISABLE_COPY_MOVE(BuiltinInterchangeDrivers)
    };

}

#endif // HELLOKIT_INTERCHANGE_BUILTININTERCHANGEDRIVERS_H
