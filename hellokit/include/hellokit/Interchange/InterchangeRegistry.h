#ifndef HELLOKIT_INTERCHANGE_INTERCHANGEREGISTRY_H
#define HELLOKIT_INTERCHANGE_INTERCHANGEREGISTRY_H

#include <memory>

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>
#include <hellokit/Interchange/InterchangeReader.h>
#include <hellokit/Interchange/InterchangeWriter.h>

namespace hello::kit {

    /// All formats that this build can import or export.
    ///
    /// Built-in drivers and plugin drivers are registered identically and are not
    /// distinguished anywhere. The file dialog filters, the lookup by suffix and the import menu
    /// are all generated from the registry, so a registered format appears in all of them.
    ///
    /// \note Deliberately not a singleton, and there is no global instance. The application
    ///       owns one and passes it on. A global instance would be shared state that each test
    ///       must restore, and a test that failed to do so would cause another test to fail.
    class HELLOKIT_INTERCHANGE_EXPORT InterchangeRegistry {
    public:
        InterchangeRegistry();
        ~InterchangeRegistry();

        /// Takes ownership. A driver whose ID is already registered is rejected. Returns whether
        /// the driver was added.
        bool addReader(std::unique_ptr<InterchangeReader> reader);
        bool addWriter(std::unique_ptr<InterchangeWriter> writer);

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

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;

        Q_DISABLE_COPY_MOVE(InterchangeRegistry)
    };

}

#endif // HELLOKIT_INTERCHANGE_INTERCHANGEREGISTRY_H
