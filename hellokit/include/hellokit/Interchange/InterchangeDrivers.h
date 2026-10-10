#ifndef HELLOKIT_INTERCHANGE_INTERCHANGEDRIVERS_H
#define HELLOKIT_INTERCHANGE_INTERCHANGEDRIVERS_H

#include <memory>

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>
#include <hellokit/Interchange/InterchangeConvertRegistry.h>
#include <hellokit/Interchange/InterchangeReader.h>
#include <hellokit/Interchange/InterchangeWriter.h>

namespace hello::kit {

    /// The import and export drivers of a host: one instance of each driver registered in
    /// readerRegistry() and writerRegistry(). The content changes as drivers are registered and
    /// unregistered. See docs/Interchange.md.
    ///
    /// Built-in drivers and plugin drivers are registered alike and are not distinguished. The
    /// file dialog filters and the lookup by suffix are generated from the drivers, so a
    /// registered format is available in each of them.
    ///
    /// \note Deliberately not a singleton, and there is no global instance. The owner, such as
    ///       the Interchange plugin, passes the object to its users, and each object emits
    ///       driversChanged(). A test creates an object of its own and registers only the drivers
    ///       it requires.
    class HELLOKIT_INTERCHANGE_EXPORT InterchangeDrivers : public QObject {
        Q_OBJECT
    public:
        explicit InterchangeDrivers(QObject *parent = nullptr);
        ~InterchangeDrivers();

        /// Returns the registries in which the drivers of this object are registered.
        InterchangeReaderRegistry &readerRegistry() const;
        InterchangeWriterRegistry &writerRegistry() const;

        /// Returns the drivers in the order of registration.
        QList<InterchangeReader *> readers() const;
        QList<InterchangeWriter *> writers() const;

        InterchangeReader *readerForId(const QString &id) const;
        InterchangeWriter *writerForId(const QString &id) const;

        /// Returns the driver registered for \a suffix , compared without the leading dot and
        /// case-insensitively.
        ///
        /// \return the first registered driver with a matching suffix, or null if none matches.
        ///         The precedence of the first registration prevents a plugin from taking over a
        ///         built-in format.
        InterchangeReader *readerForSuffix(const QString &suffix) const;
        InterchangeWriter *writerForSuffix(const QString &suffix) const;

    Q_SIGNALS:
        /// Emitted after a driver was registered or unregistered.
        void driversChanged();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;

        Q_DISABLE_COPY_MOVE(InterchangeDrivers)
    };

}

#endif // HELLOKIT_INTERCHANGE_INTERCHANGEDRIVERS_H
