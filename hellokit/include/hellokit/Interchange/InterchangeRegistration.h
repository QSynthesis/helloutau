#ifndef HELLOKIT_INTERCHANGE_INTERCHANGEREGISTRATION_H
#define HELLOKIT_INTERCHANGE_INTERCHANGEREGISTRATION_H

#include <memory>

#include <QtCore/QtGlobal>

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>
#include <hellokit/Interchange/InterchangeReader.h>
#include <hellokit/Interchange/InterchangeWriter.h>

namespace hello::kit {

    /// Registration of an import driver or an export driver. The driver is present in every
    /// InterchangeRegistry for the lifetime of the registration. See docs/Plugins.md.
    ///
    /// A driver that supports both directions requires two registrations, one as a reader and
    /// one as a writer. A plugin creates its registrations in initialize() and destroys them in
    /// aboutToShutdown(), before its library is unloaded. Registrations and registries are used
    /// on the application thread only.
    class HELLOKIT_INTERCHANGE_EXPORT InterchangeRegistration {
    public:
        explicit InterchangeRegistration(std::unique_ptr<InterchangeReader> reader);
        explicit InterchangeRegistration(std::unique_ptr<InterchangeWriter> writer);
        ~InterchangeRegistration();

        /// Returns the import driver, or null for the registration of an export driver.
        InterchangeReader *reader() const;

        /// Returns the export driver, or null for the registration of an import driver.
        InterchangeWriter *writer() const;

    private:
        std::unique_ptr<InterchangeReader> m_reader;
        std::unique_ptr<InterchangeWriter> m_writer;

        Q_DISABLE_COPY_MOVE(InterchangeRegistration)
    };

}

#endif // HELLOKIT_INTERCHANGE_INTERCHANGEREGISTRATION_H
