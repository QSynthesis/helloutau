#include <memory>

#include <QtTest/QTest>

#include <hellokit/Support/OrderedObjectRegistryKeeper.h>

using namespace hello::kit;

namespace {

    struct Widget {
        explicit Widget(QString id) : m_id(std::move(id)) {
        }

        QString id() const {
            return m_id;
        }

        QString m_id;
    };

    using WidgetRegistry = OrderedObjectRegistry<Widget>;

    WidgetRegistry::Factory factoryOf(const char *id) {
        return [id] { return std::make_unique<Widget>(QString::fromLatin1(id)); };
    }

    QStringList idsOf(const OrderedObjectRegistryKeeper<Widget> &widgets) {
        QStringList ids;
        for (const auto widget : widgets.instances()) {
            ids.push_back(widget->id());
        }
        return ids;
    }

}

class test_OrderedObjectRegistryKeeper : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The instances follow the registry in the order of registration, and are found by ID.
    void the_instances_follow_the_registry() {
        OrderedObjectRegistryKeeper<Widget> widgets("widget");
        WidgetRegistry::AddFactory b(widgets.registry(), "b", {}, factoryOf("b"));
        WidgetRegistry::AddFactory a(widgets.registry(), "a", {}, factoryOf("a"));
        QCOMPARE(idsOf(widgets), (QStringList{QStringLiteral("b"), QStringLiteral("a")}));
        QCOMPARE(widgets.instance(QStringLiteral("a")), widgets.instances().at(1));
        QCOMPARE(widgets.instance(QStringLiteral("c")), nullptr);

        b = {};
        QCOMPARE(idsOf(widgets), QStringList{QStringLiteral("a")});
        QCOMPARE(widgets.instance(QStringLiteral("b")), nullptr);
    }

    // An instance whose ID differs from the name of its entry would be found under another name
    // than the one it was registered under. A null instance cannot be used.
    void an_instance_under_another_name_and_a_null_instance_are_rejected() {
        OrderedObjectRegistryKeeper<Widget> widgets("widget");
        QTest::ignoreMessage(QtWarningMsg,
                             "The widget \"other\" is registered under the name \"a\".");
        WidgetRegistry::AddFactory a(widgets.registry(), "a", {}, factoryOf("other"));
        WidgetRegistry::AddFactory null(widgets.registry(), "null", {},
                                        [] { return std::unique_ptr<Widget>(); });
        QVERIFY(widgets.instances().isEmpty());
        QCOMPARE(widgets.instance(QStringLiteral("other")), nullptr);
    }

    // The function is called once for each instance added or removed, and not for a rejected
    // instance.
    void a_change_of_the_instances_is_reported() {
        int changes = 0;
        OrderedObjectRegistryKeeper<Widget> widgets("widget", [&changes] { ++changes; });
        WidgetRegistry::AddFactory a(widgets.registry(), "a", {}, factoryOf("a"));
        QCOMPARE(changes, 1);
        QTest::ignoreMessage(QtWarningMsg,
                             "The widget \"other\" is registered under the name \"b\".");
        WidgetRegistry::AddFactory b(widgets.registry(), "b", {}, factoryOf("other"));
        QCOMPARE(changes, 1);
        a = {};
        QCOMPARE(changes, 2);
        b = {};
        QCOMPARE(changes, 2);
    }
};

QTEST_APPLESS_MAIN(test_OrderedObjectRegistryKeeper)

#include "test_OrderedObjectRegistryKeeper.moc"
