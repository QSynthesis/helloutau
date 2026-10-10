#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtTest/QTest>

#include <hellokit/Support/JsonInterop.h>

using namespace hello::kit;

namespace json = stdc::json;

class test_JsonInterop : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Qt stores every number as a double, and an integer of a settings file must read back as an
    // integer.
    void a_double_without_a_fraction_becomes_an_integer() {
        const auto integer = JsonInterop::fromQtJson(QJsonValue(3.0));
        QVERIFY(integer.isInt());
        QCOMPARE(integer.toInt(), 3);

        const auto negative = JsonInterop::fromQtJson(QJsonValue(-12.0));
        QVERIFY(negative.isInt());
        QCOMPARE(negative.toInt(), -12);

        const auto fraction = JsonInterop::fromQtJson(QJsonValue(2.5));
        QVERIFY(fraction.isDouble());
        QCOMPARE(fraction.toDouble(), 2.5);

        // Outside the range of a 64-bit integer
        const auto large = JsonInterop::fromQtJson(QJsonValue(1e20));
        QVERIFY(large.isDouble());
        QCOMPARE(large.toDouble(), 1e20);
    }

    void undefined_and_null_become_null() {
        QVERIFY(JsonInterop::fromQtJson(QJsonValue(QJsonValue::Undefined)).isNull());
        QVERIFY(JsonInterop::fromQtJson(QJsonValue(QJsonValue::Null)).isNull());
    }

    void arrays_and_objects_are_converted_recursively() {
        const QJsonObject from{
            {QStringLiteral("list"),  QJsonArray{1, QStringLiteral("x"), true}   },
            {QStringLiteral("group"), QJsonObject{{QStringLiteral("ratio"), 1.5}}},
        };
        const auto value = JsonInterop::fromQtJson(from);
        QVERIFY(value.isObject());
        const auto &object = value.toObject();

        const auto &list = object.at("list");
        QVERIFY(list.isArray());
        QCOMPARE(list.toArray().size(), size_t(3));
        QVERIFY(list.toArray()[0].isInt());
        QCOMPARE(list.toArray()[0].toInt(), 1);
        QCOMPARE(list.toArray()[1].toString(), std::string("x"));
        QCOMPARE(list.toArray()[2].toBool(), true);

        const auto &group = object.at("group");
        QVERIFY(group.isObject());
        QCOMPARE(group.toObject().at("ratio").toDouble(), 1.5);
    }

    void binary_data_becomes_null() {
        const json::Value binary(std::vector<uint8_t>{1, 2, 3});
        QVERIFY(JsonInterop::toQtJson(binary).isNull());
    }

    void a_value_survives_a_round_trip_through_qt() {
        json::Object object;
        object.emplace("flag", json::Value(true));
        object.emplace("count", json::Value(42));
        object.emplace("ratio", json::Value(0.25));
        object.emplace("name", json::Value(std::string("\xE6\xAD\x8C")));
        object.emplace("nothing", json::Value());
        object.emplace("list", json::Value(json::Array{json::Value(1), json::Value("a")}));
        json::Object group;
        group.emplace("depth", json::Value(-3));
        object.emplace("group", json::Value(std::move(group)));
        const json::Value value(std::move(object));

        const auto back = JsonInterop::fromQtJson(JsonInterop::toQtJson(value));
        QVERIFY(back == value);
    }

    void a_value_is_found_by_its_path() {
        json::Object inner;
        inner.emplace("resampler", json::Value("r.exe"));
        json::Object root;
        root.emplace("synthTools", json::Value(std::move(inner)));
        root.emplace("scalar", json::Value(1));

        QCOMPARE(JsonInterop::valueAt(root, "synthTools/resampler").toString(),
                 std::string("r.exe"));
        QVERIFY(JsonInterop::valueAt(root, "synthTools").isObject());
        QCOMPARE(JsonInterop::valueAt(root, "scalar").toInt(), 1);

        // Absent at either level
        QVERIFY(JsonInterop::valueAt(root, "synthTools/wavtool").isNull());
        QVERIFY(JsonInterop::valueAt(root, "missing/resampler").isNull());
        // A group on the path that is not an object
        QVERIFY(JsonInterop::valueAt(root, "scalar/resampler").isNull());
    }

    void inserting_a_value_creates_its_groups() {
        json::Object root;
        JsonInterop::insertAt(root, "a/b/c", json::Value(1));
        QCOMPARE(JsonInterop::valueAt(root, "a/b/c").toInt(), 1);

        JsonInterop::insertAt(root, "a/b/c", json::Value(2));
        QCOMPARE(JsonInterop::valueAt(root, "a/b/c").toInt(), 2);
        QCOMPARE(root.at("a").toObject().at("b").toObject().size(), size_t(1));
    }

    // A null value removes the entry, and with it each group that becomes empty, so that a
    // settings file does not accumulate empty groups.
    void inserting_null_removes_the_value_and_the_groups_left_empty() {
        json::Object root;
        JsonInterop::insertAt(root, "view/grid/visible", json::Value(true));
        JsonInterop::insertAt(root, "view/zoom", json::Value(2));

        JsonInterop::insertAt(root, "view/grid/visible", json::Value());
        QVERIFY(JsonInterop::valueAt(root, "view/grid").isNull());
        // The group that holds another value remains
        QCOMPARE(JsonInterop::valueAt(root, "view/zoom").toInt(), 2);

        JsonInterop::insertAt(root, "view/zoom", json::Value());
        QVERIFY(root.empty());

        // Removing an absent value creates nothing
        JsonInterop::insertAt(root, "x/y", json::Value());
        QVERIFY(root.empty());
    }
};

QTEST_APPLESS_MAIN(test_JsonInterop)

#include "test_JsonInterop.moc"
