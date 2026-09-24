#include "VoiceBankFields_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonArray>

#include "VoiceBankTree_p.h"

namespace hello::kit {

    namespace {

        struct VoiceBankFields {
            Q_DECLARE_TR_FUNCTIONS(hello::kit::VoiceBankFields)
        };

        QJsonValue stringListToJson(const QVariant &value) {
            return QJsonArray::fromStringList(edit::SlotValue<QStringList>::fromVariant(value));
        }

        std::optional<QVariant> stringListFromJson(const QJsonValue &json) {
            if (!json.isArray()) {
                return std::nullopt;
            }
            QStringList list;
            for (const auto item : json.toArray()) {
                if (!item.isString()) {
                    return std::nullopt;
                }
                list.push_back(item.toString());
            }
            return edit::SlotValue<QStringList>::toVariant(list);
        }

        QJsonValue prefixToJson(const QVariant &value) {
            const auto prefix = edit::SlotValue<VoicePrefix>::fromVariant(value);
            return QJsonObject{
                {QStringLiteral("prefix"), prefix.prefix},
                {QStringLiteral("suffix"), prefix.suffix},
            };
        }

        // An absent member is read as empty. A member of another type or an unknown member makes
        // the whole value one of another type, because the entry of a mapping is set without
        // reading the value back.
        std::optional<QVariant> prefixFromJson(const QJsonValue &json) {
            if (!json.isObject()) {
                return std::nullopt;
            }
            const auto object = json.toObject();
            const auto prefix = object.value(QStringLiteral("prefix"));
            const auto suffix = object.value(QStringLiteral("suffix"));
            if ((!prefix.isUndefined() && !prefix.isString()) ||
                (!suffix.isUndefined() && !suffix.isString()) ||
                object.size() != qsizetype(!prefix.isUndefined()) + !suffix.isUndefined()) {
                return std::nullopt;
            }
            return edit::SlotValue<VoicePrefix>::toVariant(
                VoicePrefix{prefix.toString(), suffix.toString()});
        }

        // The spellings as an array of five strings, with null for a number that was not read.
        QJsonValue spellingsToJson(const QVariant &value) {
            QJsonArray array;
            for (const auto &spelling : edit::SlotValue<OtoSpellings>::fromVariant(value)) {
                array.push_back(spelling ? QJsonValue(QString::fromStdString(*spelling))
                                         : QJsonValue(QJsonValue::Null));
            }
            return array;
        }

        std::optional<QVariant> spellingsFromJson(const QJsonValue &json) {
            const auto array = json.toArray();
            OtoSpellings spellings;
            if (!json.isArray() || array.size() != qsizetype(spellings.size())) {
                return std::nullopt;
            }
            for (qsizetype i = 0; i < array.size(); ++i) {
                const auto item = array.at(i);
                if (item.isString()) {
                    spellings[size_t(i)] = item.toString().toStdString();
                } else if (!item.isNull()) {
                    return std::nullopt;
                }
            }
            return edit::SlotValue<OtoSpellings>::toVariant(spellings);
        }

        constexpr edit::ValueFormat stringListFormat{"string list", stringListToJson,
                                                     stringListFromJson};
        constexpr edit::ValueFormat prefixFormat{"prefix", prefixToJson, prefixFromJson};
        constexpr edit::ValueFormat spellingsFormat{"spellings", spellingsToJson,
                                                    spellingsFromJson};

    }

    template <>
    constexpr const edit::ValueFormat &edit::formatOf<QStringList>() {
        return stringListFormat;
    }

    template <>
    constexpr const edit::ValueFormat &edit::formatOf<OtoSpellings>() {
        return spellingsFormat;
    }

    namespace {

        // The JSON of the records that commands create, with the field names of the slots.
        // Each field has been checked by the caller, see NodeCommands::treeOf().

        QJsonObject entryToJson(const ss::Node *tree) {
            const auto entry = edit::fromTree<VoiceOtoEntry>(tree);
            return QJsonObject{
                {QLatin1String(OtoEntrySlots::FileName.name),     entry.fileName    },
                {QLatin1String(OtoEntrySlots::Alias.name),        entry.alias       },
                {QLatin1String(OtoEntrySlots::Offset.name),       entry.offset      },
                {QLatin1String(OtoEntrySlots::Consonant.name),    entry.consonant   },
                {QLatin1String(OtoEntrySlots::Cutoff.name),       entry.cutoff      },
                {QLatin1String(OtoEntrySlots::PreUtterance.name), entry.preUtterance},
                {QLatin1String(OtoEntrySlots::VoiceOverlap.name), entry.voiceOverlap},
            };
        }

        // An entry requires its file name. The other fields default to empty and zero, as in a
        // line of an oto.ini that ends early.
        std::unique_ptr<ss::Node> entryFromJson(const QJsonObject &json,
                                                DiagnosticList &diagnostics) {
            const auto field = [&json](const auto &slot) {
                return json.value(QLatin1String(slot.name));
            };
            if (!field(OtoEntrySlots::FileName).isString()) {
                Diagnostic diagnostic;
                diagnostic.severity = DiagnosticSeverity::Error;
                diagnostic.message = VoiceBankFields::tr("An oto entry requires the field %1.")
                                         .arg(QLatin1String(OtoEntrySlots::FileName.name));
                diagnostics.push_back(diagnostic);
                return nullptr;
            }
            VoiceOtoEntry entry;
            entry.fileName = field(OtoEntrySlots::FileName).toString();
            entry.alias = field(OtoEntrySlots::Alias).toString();
            entry.offset = field(OtoEntrySlots::Offset).toDouble();
            entry.consonant = field(OtoEntrySlots::Consonant).toDouble();
            entry.cutoff = field(OtoEntrySlots::Cutoff).toDouble();
            entry.preUtterance = field(OtoEntrySlots::PreUtterance).toDouble();
            entry.voiceOverlap = field(OtoEntrySlots::VoiceOverlap).toDouble();
            return treeOf(entry);
        }

        QJsonObject characterToJson(const ss::Node *tree) {
            const auto character = edit::fromTree<VoiceCharacter>(tree);
            return QJsonObject{
                {QLatin1String(VoiceCharacterSlots::Name.name),       character.name  },
                {QLatin1String(VoiceCharacterSlots::Image.name),      character.image },
                {QLatin1String(VoiceCharacterSlots::Sample.name),     character.sample},
                {QLatin1String(VoiceCharacterSlots::Author.name),     character.author},
                {QLatin1String(VoiceCharacterSlots::Web.name),        character.web   },
                {QLatin1String(VoiceCharacterSlots::ExtraLines.name),
                 QJsonArray::fromStringList(character.extraLines)                     },
            };
        }

        // Every field of a character is optional, as every line of character.txt is.
        std::unique_ptr<ss::Node> characterFromJson(const QJsonObject &json, DiagnosticList &) {
            const auto field = [&json](const auto &slot) {
                return json.value(QLatin1String(slot.name));
            };
            VoiceCharacter character;
            character.name = field(VoiceCharacterSlots::Name).toString();
            character.image = field(VoiceCharacterSlots::Image).toString();
            character.sample = field(VoiceCharacterSlots::Sample).toString();
            character.author = field(VoiceCharacterSlots::Author).toString();
            character.web = field(VoiceCharacterSlots::Web).toString();
            for (const auto line : field(VoiceCharacterSlots::ExtraLines).toArray()) {
                character.extraLines.push_back(line.toString());
            }
            return treeOf(character);
        }

        // The records are defined before the records that refer to them.

        constexpr edit::FieldInfo otoEntryFields[] = {
            edit::valueField(OtoEntrySlots::FileName),
            edit::valueField(OtoEntrySlots::Alias),
            edit::valueField(OtoEntrySlots::Offset),
            edit::valueField(OtoEntrySlots::Consonant),
            edit::valueField(OtoEntrySlots::Cutoff),
            edit::valueField(OtoEntrySlots::PreUtterance),
            edit::valueField(OtoEntrySlots::VoiceOverlap),
            edit::internalField(edit::valueField(OtoEntrySlots::Spellings)),
        };

        constexpr edit::RecordInfo otoEntryRecord{"otoEntry", OtoEntryType, otoEntryFields,
                                                  entryFromJson, entryToJson};

        // Every field but the entries is read-only, see VoiceDirectorySlots.
        constexpr edit::FieldInfo directoryFields[] = {
            edit::readOnlyField(edit::valueField(VoiceDirectorySlots::Path)),
            edit::readOnlyField(edit::valueField(VoiceDirectorySlots::Charset)),
            edit::readOnlyField(edit::valueField(VoiceDirectorySlots::OtoCharset)),
            edit::listField(VoiceDirectorySlots::OtoEntries, otoEntryRecord),
        };

        // A directory is not created from JSON, because only reading from disk adds one.
        constexpr edit::RecordInfo directoryRecord{"directory", VoiceDirectoryType,
                                                   directoryFields};

        constexpr edit::FieldInfo characterFields[] = {
            edit::valueField(VoiceCharacterSlots::Name),
            edit::valueField(VoiceCharacterSlots::Image),
            edit::valueField(VoiceCharacterSlots::Sample),
            edit::valueField(VoiceCharacterSlots::Author),
            edit::valueField(VoiceCharacterSlots::Web),
            edit::valueField(VoiceCharacterSlots::ExtraLines),
        };

        constexpr edit::RecordInfo characterRecord{"character", VoiceCharacterType, characterFields,
                                                   characterFromJson, characterToJson};

        constexpr edit::FieldInfo voiceBankFields[] = {
            edit::recordField(VoiceBankSlots::Character, characterRecord, true),
            edit::mappingField(VoiceBankSlots::PrefixMap, prefixFormat),
            edit::valueField(VoiceBankSlots::Readme),
            edit::readOnlyField(edit::listField(VoiceBankSlots::Directories, directoryRecord)),
        };

        constexpr edit::RecordInfo rootRecord{"voiceBank", VoiceBankType, voiceBankFields};

        // Returns whether the fields of record are its count slots in order.
        constexpr bool listsSlotsInOrder(const edit::RecordInfo &record, int count) {
            if (record.fields.size() != count) {
                return false;
            }
            for (int i = 0; i < count; ++i) {
                if (record.fields.at(i).index != i) {
                    return false;
                }
            }
            return true;
        }

        static_assert(listsSlotsInOrder(rootRecord, VoiceBankSlots::count));
        static_assert(listsSlotsInOrder(characterRecord, VoiceCharacterSlots::count));
        static_assert(listsSlotsInOrder(directoryRecord, VoiceDirectorySlots::count));
        static_assert(listsSlotsInOrder(otoEntryRecord, OtoEntrySlots::count));

        constexpr const edit::RecordInfo *records[] = {
            &rootRecord,
            &characterRecord,
            &directoryRecord,
            &otoEntryRecord,
        };

    }

    const edit::RecordInfo &voiceBankRecord() {
        return rootRecord;
    }

    const edit::RecordInfo *voiceBankRecordOf(int nodeType) {
        for (const auto record : records) {
            if (record->nodeType == nodeType) {
                return record;
            }
        }
        return nullptr;
    }

}
