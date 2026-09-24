#include <limits>
#include <optional>
#include <type_traits>

#include <QtCore/QString>
#include <QtTest/QTest>

#include <hellokit/EditBase/Slot.h>

using namespace hello::kit::edit;

namespace {

    // A value type of a document, stored as one value, and an enumeration.
    struct WholeValue {
        int part = 0;
    };

    enum Kind {
        First,
        Second,
    };

}

// The range of a slot has the type of its values, and a slot of a type that no range bounds
// accepts no range. Both are properties of the types, therefore they are checked when this file
// compiles.
static_assert(std::is_same_v<RangeOf<int>, Range<int>>);
static_assert(std::is_same_v<RangeOf<std::optional<double>>, Range<double>>);
static_assert(std::is_same_v<RangeOf<QString>, NoRange>);
static_assert(std::is_same_v<RangeOf<bool>, NoRange>);
static_assert(std::is_same_v<RangeOf<Kind>, NoRange>);
static_assert(std::is_same_v<RangeOf<std::optional<WholeValue>>, NoRange>);
static_assert(!std::is_constructible_v<decltype(Slot<QString>::range), Range<int>>);
static_assert(std::is_constructible_v<decltype(Slot<int>::range), Range<int>>);

// A range is a constant expression, so that a slot table is.
static_assert(Range<int>::between(0, 127).contains(127));
static_assert(!Range<int>::between(0, 127).contains(128));

class test_Slot : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void between_includes_both_bounds() {
        const auto range = Range<int>::between(0, 127);
        QVERIFY(range.contains(0));
        QVERIFY(range.contains(127));
        QVERIFY(!range.contains(-1));
        QVERIFY(!range.contains(128));
    }

    void at_least_has_no_upper_bound() {
        const auto range = Range<int>::atLeast(1);
        QVERIFY(range.contains(1));
        QVERIFY(range.contains(std::numeric_limits<int>::max()));
        QVERIFY(!range.contains(0));
    }

    void greater_than_excludes_its_minimum() {
        const auto range = Range<double>::greaterThan(0);
        QVERIFY(!range.contains(0));
        QVERIFY(range.contains(0.001));
        QVERIFY(range.contains(1e300));
    }

    void a_range_converts_its_bounds() {
        const auto range = Range<int>::between(0, 127).to<double>();
        QCOMPARE(range.minimum, 0.0);
        QCOMPARE(range.maximum, std::optional<double>(127.0));
        QVERIFY(!range.minimumExclusive);

        const auto open = Range<int>::greaterThan(1).to<double>();
        QVERIFY(!open.maximum);
        QVERIFY(open.minimumExclusive);
    }
};

QTEST_APPLESS_MAIN(test_Slot)

#include "test_Slot.moc"
