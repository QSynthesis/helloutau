#include <map>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtTest/QTest>

#include <qsubstate/StructNode.h>

#include "VoiceBankFields_p.h"
#include "VoiceBankTree_p.h"

using namespace hello::kit;

class test_VoiceBankFields : public QObject {
    Q_OBJECT

private:
    static const edit::RecordInfo &recordOf(int nodeType) {
        const auto record = voiceBankRecordOf(nodeType);
        Q_ASSERT(record);
        return *record;
    }

    // The formats of the value types of a voice bank, which the field table holds.

    static const edit::ValueFormat &stringListFormat() {
        return *recordOf(VoiceCharacterType).field(u"extraLines")->format;
    }

    static const edit::ValueFormat &prefixFormat() {
        return *voiceBankRecord().field(u"prefixMap")->format;
    }

    // Found by its slot, because an internal field is not found by name.
    static const edit::ValueFormat &spellingsFormat() {
        return *recordOf(OtoEntryType).fields.at(OtoEntrySlots::Spellings.index).format;
    }

    // Appends record and every record reachable from it through its fields.
    static void collect(const edit::RecordInfo &record, QList<const edit::RecordInfo *> &records) {
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

    static void verifyRoundTrip(const edit::ValueFormat &format, const QJsonValue &json) {
        const auto value = format.fromJson(json);
        QVERIFY2(value.has_value(), format.typeName);
        QCOMPARE(format.toJson(*value), json);
    }

private Q_SLOTS:
    // A field is found by the index of its slot as well as by its name.
    void each_record_lists_its_slots_in_order() {
        const std::map<int, int> slotCounts{
            {VoiceBankType,      VoiceBankSlots::count     },
            {VoiceCharacterType, VoiceCharacterSlots::count},
            {VoiceDirectoryType, VoiceDirectorySlots::count},
            {OtoEntryType,       OtoEntrySlots::count      },
        };

        QList<const edit::RecordInfo *> records;
        collect(voiceBankRecord(), records);
        QCOMPARE(int(records.size()), int(slotCounts.size()));
        for (const auto record : records) {
            QCOMPARE(voiceBankRecordOf(record->nodeType), record);
            QCOMPARE(int(record->fields.size()), slotCounts.at(record->nodeType));
            for (int i = 0; i < record->fields.size(); ++i) {
                QCOMPARE(record->fields.at(i).index, i);
            }
        }
        QVERIFY(!voiceBankRecordOf(ss::Node::User));
    }

    // The facts of the disk and the encodings are modified by the document layer alone, and the
    // spellings belong to it.
    void the_facts_of_the_disk_are_read_only_and_the_spellings_internal() {
        const auto directories = voiceBankRecord().field(u"directories");
        QCOMPARE(directories->kind, edit::FieldInfo::List);
        QVERIFY(directories->readOnly);

        const auto &directory = recordOf(VoiceDirectoryType);
        for (const auto name : {u"path", u"charset"}) {
            QVERIFY2(directory.field(name)->readOnly, qPrintable(QString::fromUtf16(name)));
        }
        QVERIFY(!directory.field(u"otoEntries")->readOnly);

        const auto &entry = recordOf(OtoEntryType);
        QVERIFY(!entry.field(u"spellings"));
        QVERIFY(entry.fields.at(OtoEntrySlots::Spellings.index).internal);
        for (const auto &field : entry.fields) {
            QVERIFY(!field.readOnly);
        }
        QVERIFY(!voiceBankRecord().field(u"readme")->readOnly);
    }

    void each_field_has_the_kind_of_its_slot() {
        const auto character = voiceBankRecord().field(u"character");
        QCOMPARE(character->kind, edit::FieldInfo::Record);
        QVERIFY(character->optional);
        QCOMPARE(character->record, &recordOf(VoiceCharacterType));

        const auto prefixMap = voiceBankRecord().field(u"prefixMap");
        QCOMPARE(prefixMap->kind, edit::FieldInfo::Mapping);
        QCOMPARE(QLatin1String(prefixMap->format->typeName), QLatin1String("prefix"));

        const auto entries = recordOf(VoiceDirectoryType).field(u"otoEntries");
        QCOMPARE(entries->kind, edit::FieldInfo::List);
        QCOMPARE(entries->record, &recordOf(OtoEntryType));

        QCOMPARE(QLatin1String(recordOf(OtoEntryType).field(u"offset")->format->typeName),
                 QLatin1String(edit::ValueFormats::number.typeName));
        QCOMPARE(QLatin1String(recordOf(VoiceDirectoryType).field(u"path")->format->typeName),
                 QLatin1String(edit::ValueFormats::string.typeName));
        QCOMPARE(QLatin1String(stringListFormat().typeName), QLatin1String("string list"));
    }

    void each_format_reads_back_the_json_it_writes() {
        verifyRoundTrip(stringListFormat(), QJsonArray{QStringLiteral("a"), QStringLiteral("b")});
        verifyRoundTrip(prefixFormat(), QJsonObject{
                                            {QStringLiteral("prefix"), QStringLiteral("p")},
                                            {QStringLiteral("suffix"), QStringLiteral("s")},
        });
        verifyRoundTrip(spellingsFormat(),
                        QJsonArray{QStringLiteral("41.0"), QString(), QJsonValue::Null,
                                   QStringLiteral("04.457"), QStringLiteral("-1")});
    }

    // An absent member of a prefix is empty, as an absent column of prefix.map is.
    void an_absent_part_of_a_prefix_is_empty() {
        const auto value = prefixFormat().fromJson(QJsonObject{
            {QStringLiteral("suffix"), QStringLiteral("s")}
        });
        QVERIFY(value);
        const auto prefix = edit::SlotValue<VoicePrefix>::fromVariant(*value);
        QCOMPARE(prefix.prefix, QString());
        QCOMPARE(prefix.suffix, QStringLiteral("s"));
    }

    void a_value_of_another_type_is_refused() {
        QVERIFY(!stringListFormat().fromJson(QStringLiteral("a")));
        QVERIFY(!stringListFormat().fromJson(QJsonArray{1}));
        QVERIFY(!prefixFormat().fromJson(QStringLiteral("p")));
        QVERIFY(!prefixFormat().fromJson(QJsonObject{
            {QStringLiteral("prefix"), 1}
        }));
        QVERIFY(!spellingsFormat().fromJson(QJsonArray{1, 2, 3, 4, 5}));
    }

    // The value of a mapping entry is not read back, so the format itself refuses what it would
    // ignore. Five numbers have five spellings.
    void a_malformed_whole_value_is_refused() {
        QVERIFY(!prefixFormat().fromJson(QJsonObject{
            {QStringLiteral("prefix"), QStringLiteral("p")},
            {QStringLiteral("extra"),  QStringLiteral("x")},
        }));
        QVERIFY(!spellingsFormat().fromJson(QJsonArray{QStringLiteral("1")}));
    }

    // A command states the public fields of an entry. Its spellings are absent, so its numbers
    // are written anew, including zeros.
    void an_entry_is_created_from_its_json() {
        const auto &record = recordOf(OtoEntryType);
        const QJsonObject json{
            {QStringLiteral("fileName"),     QStringLiteral("a.wav")},
            {QStringLiteral("alias"),        QStringLiteral("a")    },
            {QStringLiteral("offset"),       1                      },
            {QStringLiteral("consonant"),    2                      },
            {QStringLiteral("cutoff"),       -3                     },
            {QStringLiteral("preUtterance"), 4                      },
            {QStringLiteral("voiceOverlap"), 5                      },
        };
        DiagnosticList diagnostics;
        const auto tree = record.treeFromJson(json, diagnostics);
        QVERIFY(tree);
        QVERIFY(diagnostics.isEmpty());
        QCOMPARE(record.treeToJson(tree.get()), json);
        const auto entry = edit::fromTree<VoiceOtoEntry>(tree.get());
        QCOMPARE(entry.cutoff, -3.0);
        for (const auto &spelling : entry.spellings) {
            QVERIFY(!spelling.has_value());
        }

        const auto bare = record.treeFromJson(
            QJsonObject{
                {QStringLiteral("fileName"), QStringLiteral("b.wav")}
        },
            diagnostics);
        QVERIFY(bare);
        QCOMPARE(edit::fromTree<VoiceOtoEntry>(bare.get()).alias, QString());

        QVERIFY(!record.treeFromJson(QJsonObject{}, diagnostics));
        QVERIFY(hasError(diagnostics));
    }

    void a_character_is_created_from_its_json() {
        const auto &record = recordOf(VoiceCharacterType);
        const QJsonObject json{
            {QStringLiteral("name"),       QStringLiteral("n")                                   },
            {QStringLiteral("image"),      QStringLiteral("i.bmp")                               },
            {QStringLiteral("sample"),     QStringLiteral("s.wav")                               },
            {QStringLiteral("author"),     QStringLiteral("a")                                   },
            {QStringLiteral("web"),        QStringLiteral("w")                                   },
            {QStringLiteral("extraLines"), QJsonArray{QStringLiteral("x:1"), QStringLiteral("y")}},
        };
        DiagnosticList diagnostics;
        const auto tree = record.treeFromJson(json, diagnostics);
        QVERIFY(tree);
        QCOMPARE(record.treeToJson(tree.get()), json);

        const auto empty = record.treeFromJson(QJsonObject{}, diagnostics);
        QVERIFY(empty);
        QVERIFY(diagnostics.isEmpty());
        QVERIFY(edit::fromTree<VoiceCharacter>(empty.get()) == VoiceCharacter());
    }

    // Only reading from disk adds a directory, and the voice bank itself is not replaced.
    void only_entries_and_the_character_are_created_from_json() {
        QVERIFY(recordOf(OtoEntryType).treeFromJson);
        QVERIFY(recordOf(VoiceCharacterType).treeFromJson);
        QVERIFY(!recordOf(VoiceDirectoryType).treeFromJson);
        QVERIFY(!voiceBankRecord().treeFromJson);
    }
};

QTEST_APPLESS_MAIN(test_VoiceBankFields)

#include "test_VoiceBankFields.moc"
