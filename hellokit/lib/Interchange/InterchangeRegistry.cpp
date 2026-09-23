#include "InterchangeRegistry.h"

#include <QtCore/QHash>

#include <stdcorelib/pimpl.h>

namespace hello::kit {

    // One list per driver kind, in registration order, plus an index by ID. Registration order
    // resolves a suffix registered by several drivers, so it must be preserved rather than
    // replaced by the order of a hash table.
    template <class T>
    struct Table {
        std::vector<std::unique_ptr<T>> owned;
        QHash<QString, T *> byId;

        bool add(std::unique_ptr<T> item) {
            if (!item || byId.contains(item->id())) {
                return false;
            }
            byId.insert(item->id(), item.get());
            owned.push_back(std::move(item));
            return true;
        }

        QList<T *> all() const {
            QList<T *> result;
            result.reserve(int(owned.size()));
            for (const auto &item : owned) {
                result.push_back(item.get());
            }
            return result;
        }

        T *forSuffix(const QString &suffix) const {
            const QString wanted = suffix.startsWith(QLatin1Char('.')) ? suffix.mid(1) : suffix;
            for (const auto &item : owned) {
                for (const auto &candidate : item->suffixes()) {
                    if (candidate.compare(wanted, Qt::CaseInsensitive) == 0) {
                        return item.get();
                    }
                }
            }
            return nullptr;
        }
    };

    class InterchangeRegistry::Impl {
    public:
        Table<InterchangeReader> readers;
        Table<InterchangeWriter> writers;
    };

    InterchangeRegistry::InterchangeRegistry() : _impl(std::make_unique<Impl>()) {
    }

    InterchangeRegistry::~InterchangeRegistry() = default;

    bool InterchangeRegistry::addReader(std::unique_ptr<InterchangeReader> reader) {
        stdc_impl_t;
        return impl.readers.add(std::move(reader));
    }

    bool InterchangeRegistry::addWriter(std::unique_ptr<InterchangeWriter> writer) {
        stdc_impl_t;
        return impl.writers.add(std::move(writer));
    }

    QList<InterchangeReader *> InterchangeRegistry::readers() const {
        stdc_impl_t;
        return impl.readers.all();
    }

    QList<InterchangeWriter *> InterchangeRegistry::writers() const {
        stdc_impl_t;
        return impl.writers.all();
    }

    InterchangeReader *InterchangeRegistry::readerForId(const QString &id) const {
        stdc_impl_t;
        return impl.readers.byId.value(id);
    }

    InterchangeWriter *InterchangeRegistry::writerForId(const QString &id) const {
        stdc_impl_t;
        return impl.writers.byId.value(id);
    }

    InterchangeReader *InterchangeRegistry::readerForSuffix(const QString &suffix) const {
        stdc_impl_t;
        return impl.readers.forSuffix(suffix);
    }

    InterchangeWriter *InterchangeRegistry::writerForSuffix(const QString &suffix) const {
        stdc_impl_t;
        return impl.writers.forSuffix(suffix);
    }

}
