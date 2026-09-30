#ifndef HELLOKIT_INTERCHANGE_BUILTININTERCHANGEDRIVERS_H
#define HELLOKIT_INTERCHANGE_BUILTININTERCHANGEDRIVERS_H

#include <memory>
#include <vector>

#include <QtCore/QtGlobal>

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>

namespace hello::kit {

    class InterchangeRegistration;

    /// Registers the drivers of this library while it exists: MidiReader and MidiWriter.
    ///
    /// The plugin Interchange holds one, and so does a test that needs them.
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
