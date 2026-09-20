#ifndef HELLOKIT_INTERCHANGE_INTERCHANGEREGISTRY_H
#define HELLOKIT_INTERCHANGE_INTERCHANGEREGISTRY_H

#include <memory>

#include <QList>
#include <QString>
#include <QStringList>

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>
#include <hellokit/Interchange/InterchangeReader.h>
#include <hellokit/Interchange/InterchangeWriter.h>

namespace hello::kit {

    /// Every format this build can read or write.
    ///
    /// A driver that ships with the application and a driver that arrived in a plugin are added
    /// the same way and are told apart nowhere. The file dialog's filters, the lookup by
    /// suffix and the import menu are all generated from here, so a format that registers gets
    /// all of them at once.
    ///
    /// \note Not a singleton, and there is deliberately no global instance. The application
    ///       holds one and passes it along. A global would be shared state that tests have to
    ///       put back the way they found it, and a test that forgot would fail some other test
    ///       instead of itself.
    class HELLOKIT_INTERCHANGE_EXPORT InterchangeRegistry {
    public:
        InterchangeRegistry();
        ~InterchangeRegistry();

        /// Takes ownership. A driver whose id is already registered is refused, and the returned
        /// value says which way it went.
        bool addReader(std::unique_ptr<InterchangeReader> reader);
        bool addWriter(std::unique_ptr<InterchangeWriter> writer);

        QList<InterchangeReader *> readers() const;
        QList<InterchangeWriter *> writers() const;

        InterchangeReader *readerForId(const QString &id) const;
        InterchangeWriter *writerForId(const QString &id) const;

        /// The driver claiming \a suffix, which is compared without its dot and without case.
        ///
        /// \return the first driver that claims it, or null. Where two drivers claim the same
        ///         suffix the one registered first wins, so a plugin cannot take a format away
        ///         from the application by claiming it too.
        InterchangeReader *readerForSuffix(const QString &suffix) const;
        InterchangeWriter *writerForSuffix(const QString &suffix) const;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;

        Q_DISABLE_COPY_MOVE(InterchangeRegistry)
    };

}

#endif // HELLOKIT_INTERCHANGE_INTERCHANGEREGISTRY_H
