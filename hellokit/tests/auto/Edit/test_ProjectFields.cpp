#include <map>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtTest/QTest>

#include <qsubstate/StructNode.h>

#include "ProjectFields_p.h"
#include "ProjectSamples.h"
#include "ProjectTree_p.h"

using namespace hello::kit;

class test_ProjectFields : public QObject {
    Q_OBJECT

private:
    static const RecordInfo &recordOf(int nodeType) {
        const auto record = projectRecordOf(nodeType);
        Q_ASSERT(record);
        return *record;
    }

    // The formats of the value types of a project, which the field table holds.

    static const ValueFormat &envelopeFormat() {
        return *recordOf(NoteType).field(u"envelope")->format;
    }

    static const ValueFormat &vibratoFormat() {
        return *recordOf(NoteType).field(u"vibrato")->format;
    }

    static const ValueFormat &portamentoTypeFormat() {
        return *recordOf(PortamentoPointType).field(u"type")->format;
    }

    static QStringList fieldNames(const RecordInfo &record) {
        QStringList names;
        for (const auto &field : record.fields) {
            names.push_back(QString::fromLatin1(field.name));
        }
        names.sort();
        return names;
    }

    static QStringList sortedKeys(const QJsonObject &object) {
        auto keys = object.keys();
        keys.sort();
        return keys;
    }

    // Appends record and every record reachable from it through its fields.
    static void collect(const RecordInfo &record, QList<const RecordInfo *> &records) {
        if (records.contains(&record)) {
            return;
        }
        records.push_back(&record);
        for (const auto &field : record.fields) {
            if (field.record) {
                collect(*field.record, records);
            }
        }
    }

    static void verifyRoundTrip(const ValueFormat &format, const QJsonValue &json) {
        const auto value = format.fromJson(json);
        QVERIFY2(value.has_value(), format.typeName);
        QCOMPARE(format.toJson(*value), json);
    }

private Q_SLOTS:
    // A field is found by the index of its slot as well as by its name.
    void each_record_lists_its_slots_in_order() {
        const std::map<int, int> slotCounts{
            {ProjectType,         ProjectSlots::count   },
            {SettingsType,        SettingsSlots::count  },
            {TrackType,           TrackSlots::count     },
            {NoteType,            NoteSlots::count      },
            {PortamentoPointType, PortamentoSlots::count},
            {PitchBendType,       PitchBendSlots::count },
        };

        QList<const RecordInfo *> records;
        collect(projectRecord(), records);
        QCOMPARE(int(records.size()), int(slotCounts.size()));
        for (const auto record : records) {
            QCOMPARE(projectRecordOf(record->nodeType), record);
            QCOMPARE(int(record->fields.size()), slotCounts.at(record->nodeType));
            for (int i = 0; i < record->fields.size(); ++i) {
                QCOMPARE(record->fields.at(i).index, i);
            }
        }
        QVERIFY(!projectRecordOf(PitchValuesType));
    }

    // Commands and logs use the names of the fields of .usth, therefore a project written with
    // every field has exactly the fields of the table in each record.
    void the_field_names_are_those_of_usth() {
        const auto root = QJsonDocument::fromJson(richProject().toJson()).object();
        const auto track = root.value(QStringLiteral("tracks")).toArray().at(0).toObject();
        const auto note = track.value(QStringLiteral("notes")).toArray().at(0).toObject();

        QCOMPARE(sortedKeys(root.value(QStringLiteral("settings")).toObject()),
                 fieldNames(recordOf(SettingsType)));
        QCOMPARE(sortedKeys(track), fieldNames(recordOf(TrackType)));
        QCOMPARE(sortedKeys(note), fieldNames(recordOf(NoteType)));
        QCOMPARE(sortedKeys(note.value(QStringLiteral("portamento")).toArray().at(0).toObject()),
                 fieldNames(recordOf(PortamentoPointType)));
        QCOMPARE(sortedKeys(note.value(QStringLiteral("pitchBend")).toObject()),
                 fieldNames(recordOf(PitchBendType)));
    }

    void each_field_has_the_kind_of_its_slot() {
        const auto &note = recordOf(NoteType);
        QVERIFY(!note.field(u"missing"));

        const auto length = note.field(u"length");
        QCOMPARE(length->kind, FieldInfo::Value);
        QCOMPARE(length->format, &ValueFormats::integer);
        QVERIFY(!length->optional);
        QVERIFY(length->range.has_value());

        const auto intensity = note.field(u"intensity");
        QCOMPARE(intensity->format, &ValueFormats::number);
        QVERIFY(intensity->optional);

        QCOMPARE(QLatin1String(note.field(u"envelope")->format->typeName),
                 QLatin1String("envelope"));
        QVERIFY(note.field(u"envelope")->optional);

        const auto portamento = note.field(u"portamento");
        QCOMPARE(portamento->kind, FieldInfo::List);
        QCOMPARE(portamento->record, &recordOf(PortamentoPointType));

        const auto pitchBend = note.field(u"pitchBend");
        QCOMPARE(pitchBend->kind, FieldInfo::Record);
        QVERIFY(pitchBend->optional);

        const auto values = recordOf(PitchBendType).field(u"values");
        QCOMPARE(values->kind, FieldInfo::Array);
        QCOMPARE(values->arrayType, int(PitchValuesType));

        QCOMPARE(note.field(u"userData")->kind, FieldInfo::Mapping);
        QCOMPARE(note.field(u"userData")->format, &ValueFormats::string);

        const auto settings = projectRecord().field(u"settings");
        QCOMPARE(settings->kind, FieldInfo::Record);
        QVERIFY(!settings->optional);
        QCOMPARE(projectRecord().field(u"unknownFields")->format, &ValueFormats::json);
    }

    void each_format_reads_back_the_json_it_writes() {
        const auto project = richProject();
        const auto note = project.tracks.first().notes.first().toJson();

        verifyRoundTrip(ValueFormats::string, QStringLiteral("a"));
        verifyRoundTrip(ValueFormats::integer, -3);
        verifyRoundTrip(ValueFormats::number, 1.5);
        verifyRoundTrip(ValueFormats::boolean, false);
        verifyRoundTrip(ValueFormats::json, QJsonArray{1, QStringLiteral("b")});
        verifyRoundTrip(envelopeFormat(), note.value(QStringLiteral("envelope")));
        verifyRoundTrip(vibratoFormat(), note.value(QStringLiteral("vibrato")));
        verifyRoundTrip(portamentoTypeFormat(), QStringLiteral("Linear"));
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
        QVERIFY(!envelopeFormat().fromJson(QStringLiteral("envelope")));
        QVERIFY(!vibratoFormat().fromJson(180));
        QVERIFY(!portamentoTypeFormat().fromJson(1));
    }

    // An envelope has four or five anchors. The UST letter of a curve type is not its name.
    void a_malformed_whole_value_is_refused() {
        const QJsonObject threeAnchors{
            {QStringLiteral("anchors"),
             QJsonArray{QJsonObject{{QStringLiteral("x"), 0}, {QStringLiteral("y"), 0}},
                        QJsonObject{{QStringLiteral("x"), 5}, {QStringLiteral("y"), 100}},
                        QJsonObject{{QStringLiteral("x"), 0}, {QStringLiteral("y"), 0}}}},
        };
        QVERIFY(!envelopeFormat().fromJson(threeAnchors));
        QVERIFY(!portamentoTypeFormat().fromJson(QStringLiteral("s")));
    }

    void a_note_is_created_from_its_json() {
        const auto &record = recordOf(NoteType);
        DiagnosticList diagnostics;
        const auto tree = record.treeFromJson(
            QJsonObject{
                {QStringLiteral("lyric"),   QStringLiteral("a")},
                {QStringLiteral("length"),  480                },
                {QStringLiteral("noteNum"), 60                 },
        },
            diagnostics);
        QVERIFY(tree);
        QVERIFY(diagnostics.isEmpty());
        QCOMPARE(tree->type(), int(NoteType));
        QCOMPARE(static_cast<const ss::StructNodeBase &>(*tree)
                     .variant(NoteSlots::Lyric.index)
                     .toString(),
                 QStringLiteral("a"));

        QVERIFY(!record.treeFromJson(QJsonObject{}, diagnostics));
        QVERIFY(hasError(diagnostics));
    }

    // A project holds exactly one track, and its settings and the project itself are not
    // replaced as a whole.
    void only_the_records_in_lists_and_optional_slots_are_created_from_json() {
        QVERIFY(recordOf(NoteType).treeFromJson);
        QVERIFY(recordOf(PortamentoPointType).treeFromJson);
        QVERIFY(recordOf(PitchBendType).treeFromJson);
        QVERIFY(!recordOf(TrackType).treeFromJson);
        QVERIFY(!recordOf(SettingsType).treeFromJson);
        QVERIFY(!recordOf(ProjectType).treeFromJson);
    }
};

QTEST_APPLESS_MAIN(test_ProjectFields)

#include "test_ProjectFields.moc"
