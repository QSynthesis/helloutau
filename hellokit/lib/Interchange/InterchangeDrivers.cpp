#include "InterchangeDrivers.h"

#include <stdcorelib/pimpl.h>

#include <hellokit/Support/OrderedObjectRegistryKeeper.h>

namespace hello::kit {

    namespace {

        // The first driver of drivers that claims suffix. The order determines the driver for a
        // suffix claimed by several drivers, so a hash table is not used.
        template <class Driver>
        Driver *forSuffix(const OrderedObjectRegistryKeeper<Driver> &drivers,
                          const QString &suffix) {
            const QString wanted = suffix.startsWith(QLatin1Char('.')) ? suffix.mid(1) : suffix;
            for (const auto driver : drivers.instances()) {
                for (const auto &candidate : driver->suffixes()) {
                    if (candidate.compare(wanted, Qt::CaseInsensitive) == 0) {
                        return driver;
                    }
                }
            }
            return nullptr;
        }

    }

    class InterchangeDrivers::Impl {
    public:
        explicit Impl(InterchangeDrivers *decl)
            : readers("interchange driver", [decl] { Q_EMIT decl->driversChanged(); }),
              writers("interchange driver", [decl] { Q_EMIT decl->driversChanged(); }) {
        }

        OrderedObjectRegistryKeeper<InterchangeReader> readers;
        OrderedObjectRegistryKeeper<InterchangeWriter> writers;
    };

    InterchangeDrivers::InterchangeDrivers(QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(this)) {
    }

    InterchangeDrivers::~InterchangeDrivers() = default;

    InterchangeReaderRegistry &InterchangeDrivers::readerRegistry() const {
        stdc_impl_t;
        return impl.readers.registry();
    }

    InterchangeWriterRegistry &InterchangeDrivers::writerRegistry() const {
        stdc_impl_t;
        return impl.writers.registry();
    }

    QList<InterchangeReader *> InterchangeDrivers::readers() const {
        stdc_impl_t;
        return impl.readers.instances();
    }

    QList<InterchangeWriter *> InterchangeDrivers::writers() const {
        stdc_impl_t;
        return impl.writers.instances();
    }

    InterchangeReader *InterchangeDrivers::readerForId(const QString &id) const {
        stdc_impl_t;
        return impl.readers.instance(id);
    }

    InterchangeWriter *InterchangeDrivers::writerForId(const QString &id) const {
        stdc_impl_t;
        return impl.writers.instance(id);
    }

    InterchangeReader *InterchangeDrivers::readerForSuffix(const QString &suffix) const {
        stdc_impl_t;
        return forSuffix(impl.readers, suffix);
    }

    InterchangeWriter *InterchangeDrivers::writerForSuffix(const QString &suffix) const {
        stdc_impl_t;
        return forSuffix(impl.writers, suffix);
    }

}
