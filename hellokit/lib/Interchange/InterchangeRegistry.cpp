#include "InterchangeRegistry.h"

#include <algorithm>

#include <stdcorelib/pimpl.h>

#include "InterchangeRegistrations_p.h"

namespace hello::kit {

    namespace {

        InterchangeReader *driverOf(const InterchangeRegistration *registration,
                                    const InterchangeReader *) {
            return registration->reader();
        }

        InterchangeWriter *driverOf(const InterchangeRegistration *registration,
                                    const InterchangeWriter *) {
            return registration->writer();
        }

        // Returns the drivers of one kind in the order of registration, only the first of each
        // ID. The order determines the driver for a suffix claimed by several drivers, so a hash
        // table is not used.
        template <class T>
        QList<T *> driversOf() {
            QList<T *> result;
            for (const auto registration : InterchangeRegistrations::instance().registrations()) {
                const auto driver = driverOf(registration, static_cast<const T *>(nullptr));
                if (!driver) {
                    continue;
                }
                const auto id = driver->id();
                if (std::none_of(result.begin(), result.end(),
                                 [&id](const T *other) { return other->id() == id; })) {
                    result.push_back(driver);
                }
            }
            return result;
        }

        template <class T>
        T *driverForId(const QString &id) {
            for (const auto driver : driversOf<T>()) {
                if (driver->id() == id) {
                    return driver;
                }
            }
            return nullptr;
        }

        template <class T>
        T *driverForSuffix(const QString &suffix) {
            const QString wanted = suffix.startsWith(QLatin1Char('.')) ? suffix.mid(1) : suffix;
            for (const auto driver : driversOf<T>()) {
                for (const auto &candidate : driver->suffixes()) {
                    if (candidate.compare(wanted, Qt::CaseInsensitive) == 0) {
                        return driver;
                    }
                }
            }
            return nullptr;
        }

    }

    class InterchangeRegistry::Impl : public InterchangeRegistrations::Listener {
    public:
        explicit Impl(InterchangeRegistry *registry) : registry(registry) {
        }

        InterchangeRegistry *registry;

        void registrationAdded(InterchangeRegistration *registration) override {
            Q_UNUSED(registration);
            Q_EMIT registry->driversChanged();
        }

        void registrationRemoved(InterchangeRegistration *registration) override {
            Q_UNUSED(registration);
            Q_EMIT registry->driversChanged();
        }
    };

    InterchangeRegistry::InterchangeRegistry(QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(this)) {
        stdc_impl_t;
        InterchangeRegistrations::instance().addListener(&impl);
    }

    InterchangeRegistry::~InterchangeRegistry() {
        stdc_impl_t;
        InterchangeRegistrations::instance().removeListener(&impl);
    }

    QList<InterchangeReader *> InterchangeRegistry::readers() const {
        return driversOf<InterchangeReader>();
    }

    QList<InterchangeWriter *> InterchangeRegistry::writers() const {
        return driversOf<InterchangeWriter>();
    }

    InterchangeReader *InterchangeRegistry::readerForId(const QString &id) const {
        return driverForId<InterchangeReader>(id);
    }

    InterchangeWriter *InterchangeRegistry::writerForId(const QString &id) const {
        return driverForId<InterchangeWriter>(id);
    }

    InterchangeReader *InterchangeRegistry::readerForSuffix(const QString &suffix) const {
        return driverForSuffix<InterchangeReader>(suffix);
    }

    InterchangeWriter *InterchangeRegistry::writerForSuffix(const QString &suffix) const {
        return driverForSuffix<InterchangeWriter>(suffix);
    }

}
