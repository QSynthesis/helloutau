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

    /// All formats that this build can import or export: the drivers registered in the process,
    /// which come and go with their registrations. See docs/Interchange.md.
    ///
    /// Built-in drivers and plugin drivers are registered alike by InterchangeRegistration and
    /// are not distinguished anywhere. The file dialog filters, the lookup by suffix and the
    /// import menu are all generated from the registry, so a registered format appears in all
    /// of them.
    ///
    /// \note Deliberately not a singleton, and there is no global instance. The application
    ///       owns one and passes it on, and each registry signals the changes. A test registers
    ///       only the drivers it needs, and its registrations go when it ends.
    class HELLOKIT_INTERCHANGE_EXPORT InterchangeRegistry : public QObject {
        Q_OBJECT
    public:
        explicit InterchangeRegistry(QObject *parent = nullptr);
        ~InterchangeRegistry();

        /// In the order of registration. Of drivers of the same ID, only the first registered is
        /// included, so that a plugin cannot replace a built-in driver; the next registered
        /// takes its place once it goes.
        QList<InterchangeReader *> readers() const;
        QList<InterchangeWriter *> writers() const;

        InterchangeReader *readerForId(const QString &id) const;
        InterchangeWriter *writerForId(const QString &id) const;

        /// Returns the driver registered for \a suffix , compared without the leading dot and
        /// case-insensitively.
        ///
        /// \return the first matching driver, or null. If two drivers register the same suffix,
        ///         the one registered first takes precedence, so a plugin cannot take over a
        ///         built-in format by registering it again.
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
