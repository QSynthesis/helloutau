#ifndef HELLOKIT_SUPPORT_ORDEREDOBJECTREGISTRYKEEPER_H
#define HELLOKIT_SUPPORT_ORDEREDOBJECTREGISTRYKEEPER_H

#include <functional>
#include <utility>

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QtGlobal>

#include <stdcorelib/adt/linked_map.h>
#include <stdcorelib/support/dynamicregistry.h>

#include <hellokit/Support/RegistryInstantiator.h>

namespace hello::kit {

    /// A registry of the instances of \a T in the order of registration, the name of each entry
    /// being the ID of its instance. \a T has a member function <tt>QString id() const</tt>.
    /// OrderedObjectRegistryKeeper holds such a registry and checks the IDs. See the registration
    /// interfaces in docs/Plugins.md.
    template <class T>
    using OrderedObjectRegistry =
        stdc::DynamicRegistry<T, stdc::dynamic_registry_traits<T>, stdc::linked_map>;

    /// An OrderedObjectRegistry of \a T together with one instance of each of its entries, in the
    /// order of registration, which a RegistryInstantiator creates. A host such as an editor holds
    /// one object for each kind of registration, such as the frequency table formats, and builds
    /// its domain queries on instances().
    ///
    /// A null instance, and an instance whose ID differs from the name of its entry, is rejected
    /// with a warning. The object is used on the thread on which entries are registered.
    template <class T>
    class OrderedObjectRegistryKeeper {
    public:
        /// Creates the empty registry. \a kind names an instance in the warning, such as
        /// <tt>frequency table format</tt>. \a changed is called after an instance was added
        /// or removed.
        explicit OrderedObjectRegistryKeeper(const char *kind, std::function<void()> changed = {})
            : m_kind(kind), m_changed(std::move(changed)),
              m_instantiator(
                  m_registry, [this](Item &item) { return take(item); },
                  [this](Item &) { notify(); }) {
        }

        /// Returns the registry, in which plugins register with \c Add and \c AddFactory
        /// objects. Registering does not change this object as a value.
        OrderedObjectRegistry<T> &registry() const {
            return m_registry;
        }

        /// Returns the instances in the order of registration.
        QList<T *> instances() const {
            QList<T *> result;
            for (const auto &item : m_instantiator.items()) {
                result.push_back(item.instance.get());
            }
            return result;
        }

        /// Returns the instance with the ID \a id , or null if none is registered.
        T *instance(const QString &id) const {
            for (const auto &item : m_instantiator.items()) {
                if (item.instance->id() == id) {
                    return item.instance.get();
                }
            }
            return nullptr;
        }

    private:
        using Instantiator = RegistryInstantiator<OrderedObjectRegistry<T>>;
        using Item = typename Instantiator::Item;

        bool take(Item &item) {
            if (!item.instance) {
                return false;
            }
            const auto name = QString::fromStdString(item.entry->name());
            if (item.instance->id() != name) {
                qWarning("The %s \"%s\" is registered under the name \"%s\".", m_kind,
                         qUtf8Printable(item.instance->id()), qUtf8Printable(name));
                return false;
            }
            notify();
            return true;
        }

        void notify() {
            if (m_changed) {
                m_changed();
            }
        }

        const char *m_kind;
        std::function<void()> m_changed;
        // The registry is declared before the instantiator, so that it is destroyed after it.
        mutable OrderedObjectRegistry<T> m_registry;
        Instantiator m_instantiator;

        Q_DISABLE_COPY_MOVE(OrderedObjectRegistryKeeper)
    };

}

#endif // HELLOKIT_SUPPORT_ORDEREDOBJECTREGISTRYKEEPER_H
