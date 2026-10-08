#include <memory>
#include <string>
#include <vector>

#include <QtTest/QTest>

#include <stdcorelib/adt/linked_map.h>

#include <hellokit/Support/RegistryInstanceList.h>

using namespace hello::kit;

namespace {

    struct Widget {
        explicit Widget(int tag) : tag(tag) {
        }

        int tag;
    };

    using WidgetRegistry =
        stdc::DynamicRegistry<Widget, stdc::dynamic_registry_traits<Widget>, stdc::linked_map>;
    using Instances = RegistryInstanceList<WidgetRegistry>;

    WidgetRegistry::Factory factoryOf(int tag) {
        return [tag] { return std::make_unique<Widget>(tag); };
    }

    std::vector<int> tagsOf(const Instances &instances) {
        std::vector<int> tags;
        for (const auto &item : instances.items()) {
            tags.push_back(item.instance->tag);
        }
        return tags;
    }

}

class test_RegistryInstanceList : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The entries registered before the object are instantiated at construction, and the later
    // entries as they are added, in the order of the registry.
    void every_entry_is_instantiated_in_the_order_of_the_registry() {
        WidgetRegistry registry;
        WidgetRegistry::AddFactory b(registry, "b", {}, factoryOf(2));
        WidgetRegistry::AddFactory a(registry, "a", {}, factoryOf(1));
        Instances instances(registry);
        QCOMPARE(tagsOf(instances), (std::vector<int>{2, 1}));

        WidgetRegistry::AddFactory c(registry, "c", {}, factoryOf(3));
        QCOMPARE(tagsOf(instances), (std::vector<int>{2, 1, 3}));
        QCOMPARE(instances.items().back().entry, c.entry());
    }

    // The release function receives the instance after its item left the list, and the instance
    // is destroyed afterwards.
    void a_removed_entry_releases_its_instance() {
        WidgetRegistry registry;
        WidgetRegistry::AddFactory a(registry, "a", {}, factoryOf(1));
        WidgetRegistry::AddFactory b(registry, "b", {}, factoryOf(2));
        std::vector<int> released;
        std::vector<size_t> sizes;
        Instances *pointer = nullptr;
        Instances instances(registry, {}, [&](Instances::Item &item) {
            released.push_back(item.instance->tag);
            sizes.push_back(pointer->items().size());
        });
        pointer = &instances;

        a = {};
        QCOMPARE(released, (std::vector<int>{1}));
        QCOMPARE(sizes, (std::vector<size_t>{1}));
        QCOMPARE(tagsOf(instances), (std::vector<int>{2}));
    }

    // A rejected item is not kept, and its removal does not reach the release function.
    void a_rejected_item_is_not_kept() {
        WidgetRegistry registry;
        int released = 0;
        Instances instances(
            registry, [](Instances::Item &item) { return item.instance->tag != 1; },
            [&released](Instances::Item &) { ++released; });
        WidgetRegistry::AddFactory a(registry, "a", {}, factoryOf(1));
        WidgetRegistry::AddFactory b(registry, "b", {}, factoryOf(2));
        QCOMPARE(tagsOf(instances), (std::vector<int>{2}));

        a = {};
        QCOMPARE(released, 0);
        b = {};
        QCOMPARE(released, 1);
        QVERIFY(instances.items().empty());
    }

    // The destruction of the object stops the notifications without releasing the instances.
    void the_destruction_releases_nothing() {
        WidgetRegistry registry;
        WidgetRegistry::AddFactory a(registry, "a", {}, factoryOf(1));
        int released = 0;
        {
            Instances instances(registry, {}, [&released](Instances::Item &) { ++released; });
        }
        a = {};
        WidgetRegistry::AddFactory b(registry, "b", {}, factoryOf(2));
        QCOMPARE(released, 0);
    }
};

QTEST_APPLESS_MAIN(test_RegistryInstanceList)

#include "test_RegistryInstanceList.moc"
