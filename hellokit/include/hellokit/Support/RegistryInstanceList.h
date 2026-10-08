#ifndef HELLOKIT_SUPPORT_REGISTRYINSTANCELIST_H
#define HELLOKIT_SUPPORT_REGISTRYINSTANCELIST_H

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

#include <stdcorelib/support/dynamicregistry.h>

namespace hello::kit {

    /// The instances that a host creates from the entries of \a Registry, a
    /// \c stdc::DynamicRegistry, one for each entry, in the order of the registry. An instance is
    /// created when its entry is added and destroyed when its entry is removed. See the
    /// registration interfaces in docs/Plugins.md.
    ///
    /// The object follows the registry through a listener. It must be used on the thread on which
    /// entries are registered, and the registry must outlive it.
    template <class Registry>
    class RegistryInstanceList : private Registry::Listener {
    public:
        using EntryPointer = typename Registry::EntryPointer;
        using Instance = typename Registry::result_type;

        struct Item {
            EntryPointer entry;
            Instance instance;
        };

        /// Called with each new item. Returns whether the host keeps the item. A rejected item is
        /// destroyed at once.
        using Take = std::function<bool(Item &item)>;

        /// Called with an item after its entry was removed, before the item is destroyed.
        using Release = std::function<void(Item &item)>;

        /// Creates an instance for each entry of \a registry, and follows the changes of the
        /// registry. Without \a take, every item is kept.
        explicit RegistryInstanceList(Registry &registry, Take take = {}, Release release = {})
            : m_registry(registry), m_take(std::move(take)), m_release(std::move(release)) {
            m_registry.add_listener(this);
            for (const auto &entry : m_registry.entries()) {
                entry_added(entry);
            }
        }

        /// Destroys the instances without calling the release function.
        ~RegistryInstanceList() override {
            m_registry.remove_listener(this);
        }

        /// Returns the kept items in the order of the registry.
        const std::vector<Item> &items() const {
            return m_items;
        }

    private:
        void entry_added(const EntryPointer &entry) override {
            Item item{entry, entry->instantiate()};
            if (!m_take || m_take(item)) {
                m_items.push_back(std::move(item));
            }
        }

        void entry_removed(const EntryPointer &entry) override {
            const auto it =
                std::find_if(m_items.begin(), m_items.end(),
                             [&entry](const Item &item) { return item.entry == entry; });
            if (it == m_items.end()) {
                return;
            }
            auto item = std::move(*it);
            m_items.erase(it);
            if (m_release) {
                m_release(item);
            }
        }

        Registry &m_registry;
        Take m_take;
        Release m_release;
        std::vector<Item> m_items;

        RegistryInstanceList(const RegistryInstanceList &) = delete;
        RegistryInstanceList &operator=(const RegistryInstanceList &) = delete;
    };

}

#endif // HELLOKIT_SUPPORT_REGISTRYINSTANCELIST_H
