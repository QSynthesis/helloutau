#ifndef HELLOKIT_INTERCHANGE_INTERCHANGEREGISTRATION_H
#define HELLOKIT_INTERCHANGE_INTERCHANGEREGISTRATION_H

#include <memory>

#include <QtCore/QtGlobal>

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>
#include <hellokit/Interchange/InterchangeReader.h>
#include <hellokit/Interchange/InterchangeWriter.h>

namespace hello::kit {

    /// Registers a driver that imports or one that exports with every InterchangeRegistry while
    /// the registration exists. See docs/Plugins.md.
    ///
    /// A driver that does both is registered twice, once as a reader and once as a writer. A
    /// plugin creates its registrations in initialize() and destroys them in aboutToShutdown(),
    /// before its library is unloaded. Registrations and registries are used on the thread of
    /// the application only.
    class HELLOKIT_INTERCHANGE_EXPORT InterchangeRegistration {
    public:
        explicit InterchangeRegistration(std::unique_ptr<InterchangeReader> reader);
        explicit InterchangeRegistration(std::unique_ptr<InterchangeWriter> writer);
        ~InterchangeRegistration();

        /// The driver that imports, or null if this registers one that exports.
        InterchangeReader *reader() const;

        /// The driver that exports, or null if this registers one that imports.
        InterchangeWriter *writer() const;

    private:
        std::unique_ptr<InterchangeReader> m_reader;
        std::unique_ptr<InterchangeWriter> m_writer;

        Q_DISABLE_COPY_MOVE(InterchangeRegistration)
    };

}

#endif // HELLOKIT_INTERCHANGE_INTERCHANGEREGISTRATION_H
