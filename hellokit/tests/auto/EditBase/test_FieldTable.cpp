#include <QtCore/QJsonArray>
#include <QtTest/QTest>

#include <hellokit/EditBase/private/FieldTable_p.h>

using namespace hello::kit;
using namespace hello::kit::edit;

// The formats and the field constructors of the generic layer. The field table of a project is
// tested in test_ProjectFields.
class test_FieldTable : public QObject {
    Q_OBJECT

private:
    static void verifyRoundTrip(const ValueFormat &format, const QJsonValue &json) {
        const auto value = format.fromJson(json);
        QVERIFY2(value.has_value(), format.typeName);
        QCOMPARE(format.toJson(*value), json);
    }

private Q_SLOTS:
    void each_format_reads_back_the_json_it_writes() {
        verifyRoundTrip(ValueFormats::string, QStringLiteral("a"));
        verifyRoundTrip(ValueFormats::integer, -3);
        verifyRoundTrip(ValueFormats::number, 1.5);
        verifyRoundTrip(ValueFormats::boolean, false);
        verifyRoundTrip(ValueFormats::json, QJsonArray{1, QStringLiteral("b")});
    }

    void an_integer_has_no_fractional_part_and_fits_an_int() {
        QCOMPARE(ValueFormats::integer.fromJson(3)->toInt(), 3);
        QVERIFY(!ValueFormats::integer.fromJson(1.5));
        QVERIFY(!ValueFormats::integer.fromJson(1e10));
        QVERIFY(!ValueFormats::integer.fromJson(-1e10));
        QVERIFY(!ValueFormats::integer.fromJson(QStringLiteral("3")));
    }

    void a_value_of_another_type_is_refused() {
        QVERIFY(!ValueFormats::string.fromJson(1));
        QVERIFY(!ValueFormats::number.fromJson(QStringLiteral("1")));
        QVERIFY(!ValueFormats::boolean.fromJson(1));
    }

    // A value field takes the format, the range and the emptiness of its slot.
    void a_value_field_describes_its_slot() {
        const auto length = valueField(Slot<int>{1, "length", Range<int>::atLeast(1)});
        QCOMPARE(length.kind, FieldInfo::Value);
        QCOMPARE(length.index, 1);
        QCOMPARE(QLatin1String(length.name), QLatin1String("length"));
        QCOMPARE(QLatin1String(length.format->typeName), QLatin1String("integer"));
        QVERIFY(!length.optional);
        QCOMPARE(length.range->minimum, 1.0);
        QVERIFY(!length.range->maximum);

        const auto tempo = valueField(Slot<std::optional<double>>{9, "tempo"});
        QVERIFY(tempo.optional);
        QVERIFY(!tempo.range);
        QCOMPARE(QLatin1String(tempo.format->typeName), QLatin1String("number"));
    }

    // A record finds its fields by name in the order of its slots.
    void a_record_finds_its_fields_by_name() {
        static const FieldInfo fields[] = {
            valueField(Slot<QString>{0, "name"}),
            mappingField(ChildSlot{1, "tags"}, ValueFormats::json),
        };
        const RecordInfo record{"item", 0, fields};
        QCOMPARE(record.fields.size(), 2);
        QCOMPARE(record.field(u"tags"), &record.fields.at(1));
        QCOMPARE(record.field(u"tags")->kind, FieldInfo::Mapping);
        QVERIFY(!record.field(u"missing"));

        int index = 0;
        for (const auto &field : record.fields) {
            QCOMPARE(field.index, index++);
        }
        QCOMPARE(index, 2);
    }
};

QTEST_APPLESS_MAIN(test_FieldTable)

#include "test_FieldTable.moc"
