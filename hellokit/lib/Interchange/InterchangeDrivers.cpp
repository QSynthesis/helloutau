#include "InterchangeDrivers.h"

#include <stdcorelib/pimpl.h>

#include <hellokit/Support/RegistryInstanceList.h>

namespace hello::kit {

    namespace {

        // The instances of the drivers of one registry. A driver whose ID differs from the name
        // of its entry is rejected.
        template <class Registry>
        class Drivers {
        public:
            using Instances = RegistryInstanceList<Registry>;
            using Driver = typename Registry::type;

            explicit Drivers(InterchangeDrivers *decl)
                : instances(
                      registry,
                      [decl](typename Instances::Item &item) {
                          if (!item.instance) {
                              return false;
                          }
                          const auto name = QString::fromStdString(item.entry->name());
                          if (item.instance->id() != name) {
                              qWarning("The interchange driver \"%s\" is registered under the "
                                       "name \"%s\".",
                                       qUtf8Printable(item.instance->id()), qUtf8Printable(name));
                              return false;
                          }
                          Q_EMIT decl->driversChanged();
                          return true;
                      },
                      [decl](typename Instances::Item &) { Q_EMIT decl->driversChanged(); }) {
            }

            QList<Driver *> list() const {
                QList<Driver *> result;
                for (const auto &item : instances.items()) {
                    result.push_back(item.instance.get());
                }
                return result;
            }

            Driver *forId(const QString &id) const {
                for (const auto &item : instances.items()) {
                    if (item.instance->id() == id) {
                        return item.instance.get();
                    }
                }
                return nullptr;
            }

            // The order determines the driver for a suffix claimed by several drivers, so a hash
            // table is not used.
            Driver *forSuffix(const QString &suffix) const {
                const QString wanted = suffix.startsWith(QLatin1Char('.')) ? suffix.mid(1) : suffix;
                for (const auto &item : instances.items()) {
                    for (const auto &candidate : item.instance->suffixes()) {
                        if (candidate.compare(wanted, Qt::CaseInsensitive) == 0) {
                            return item.instance.get();
                        }
                    }
                }
                return nullptr;
            }

            // The instances are declared after their registry, so that they are destroyed first.
            mutable Registry registry;
            Instances instances;
        };

    }

    class InterchangeDrivers::Impl {
    public:
        explicit Impl(InterchangeDrivers *decl) : readers(decl), writers(decl) {
        }

        Drivers<InterchangeReaderRegistry> readers;
        Drivers<InterchangeWriterRegistry> writers;
    };

    InterchangeDrivers::InterchangeDrivers(QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(this)) {
    }

    InterchangeDrivers::~InterchangeDrivers() = default;

    InterchangeReaderRegistry &InterchangeDrivers::readerRegistry() const {
        stdc_impl_t;
        return impl.readers.registry;
    }

    InterchangeWriterRegistry &InterchangeDrivers::writerRegistry() const {
        stdc_impl_t;
        return impl.writers.registry;
    }

    QList<InterchangeReader *> InterchangeDrivers::readers() const {
        stdc_impl_t;
        return impl.readers.list();
    }

    QList<InterchangeWriter *> InterchangeDrivers::writers() const {
        stdc_impl_t;
        return impl.writers.list();
    }

    InterchangeReader *InterchangeDrivers::readerForId(const QString &id) const {
        stdc_impl_t;
        return impl.readers.forId(id);
    }

    InterchangeWriter *InterchangeDrivers::writerForId(const QString &id) const {
        stdc_impl_t;
        return impl.writers.forId(id);
    }

    InterchangeReader *InterchangeDrivers::readerForSuffix(const QString &suffix) const {
        stdc_impl_t;
        return impl.readers.forSuffix(suffix);
    }

    InterchangeWriter *InterchangeDrivers::writerForSuffix(const QString &suffix) const {
        stdc_impl_t;
        return impl.writers.forSuffix(suffix);
    }

}
