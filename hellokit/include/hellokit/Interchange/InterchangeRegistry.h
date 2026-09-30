#ifndef HELLOKIT_INTERCHANGE_INTERCHANGEREGISTRY_H
#define HELLOKIT_INTERCHANGE_INTERCHANGEREGISTRY_H

#include <memory>

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>
#include <hellokit/Interchange/InterchangeReader.h>
#include <hellokit/Interchange/InterchangeWriter.h>

namespace hello::kit {

    /// The import and export drivers registered in the process. The content changes as
    /// registrations are created and destroyed. See docs/Interchange.md.
    ///
    /// Built-in drivers and plugin drivers are both registered by InterchangeRegistration and
    /// are not distinguished. The file dialog filters and the lookup by suffix are generated
    /// from the registry, so a registered format is available in each of them.
    ///
    /// \note Deliberately not a singleton, and there is no global instance. The owner passes
    ///       the registry to its users, and each registry emits driversChanged(). A test
    ///       registers only the drivers it requires, and destroys the registrations when it
    ///       ends.
    class HELLOKIT_INTERCHANGE_EXPORT InterchangeRegistry : public QObject {
        Q_OBJECT
    public:
        explicit InterchangeRegistry(QObject *parent = nullptr);
        ~InterchangeRegistry();

        /// Returns the drivers in the order of registration. Of several drivers with the same
        /// ID, only the first registered is included, so that a plugin cannot replace a built-in
        /// driver. When that driver is unregistered, the next registered driver with the ID
        /// replaces it.
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

        Q_DISABLE_COPY_MOVE(InterchangeRegistry)
    };

}

#endif // HELLOKIT_INTERCHANGE_INTERCHANGEREGISTRY_H
