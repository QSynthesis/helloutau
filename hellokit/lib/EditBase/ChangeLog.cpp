#include "ChangeLog_p.h"

#include <QtCore/QJsonValue>

#include <qsubstate/StructNode.h>

#include "FieldTable_p.h"

namespace hello::kit::edit {

    namespace {

        // Returns the field of record in slot, or nullptr if the record type is unknown.
        const FieldInfo *fieldOf(const ss::Node *record, int slot, RecordLookup lookup) {
            const auto info = record ? lookup(record->type()) : nullptr;
            return info && slot >= 0 && slot < info->fields.size() ? &info->fields.at(slot)
                                                                   : nullptr;
        }

        void insertValues(QJsonObject &entry, const QVariant &before, const QVariant &after) {
            // An absent value is omitted rather than written as null, because null is a value of
            // a JSON entry.
            if (before.isValid()) {
                entry.insert(QStringLiteral("before"), QJsonValue::fromVariant(before));
            }
            if (after.isValid()) {
                entry.insert(QStringLiteral("after"), QJsonValue::fromVariant(after));
            }
        }

        std::optional<QJsonObject> valueEntry(const EditSession &session, const Change &change,
                                              RecordLookup lookup) {
            const auto &value = static_cast<const ValueChange &>(change);
            const auto node = EditSessionPrivate::find(&session, change.node());
            QJsonObject entry{
                {QStringLiteral("shape"), QStringLiteral("set")}
            };
            const auto field = fieldOf(node, value.slot(), lookup);
            if (!field) {
                entry.insert(QStringLiteral("slot"), value.slot());
                insertValues(entry, value.oldValue(), value.newValue());
                return entry;
            }
            entry.insert(QStringLiteral("slot"), QLatin1String(field->name));
            if (field->kind == FieldInfo::Value) {
                // An empty optional value is written as null, as in a command.
                entry.insert(QStringLiteral("before"), value.oldValue().isValid()
                                                           ? field->format->toJson(value.oldValue())
                                                           : QJsonValue(QJsonValue::Null));
                entry.insert(QStringLiteral("after"), value.newValue().isValid()
                                                          ? field->format->toJson(value.newValue())
                                                          : QJsonValue(QJsonValue::Null));
                return entry;
            }
            // The previous child is no longer in the tree, therefore only the record after the
            // change is written.
            const auto child = static_cast<const ss::StructNodeBase &>(*node).child(field->index);
            if (field->kind == FieldInfo::Record && field->record->treeToJson) {
                entry.insert(QStringLiteral("after"),
                             child ? QJsonValue(field->record->treeToJson(child))
                                   : QJsonValue(QJsonValue::Null));
            }
            return entry;
        }

        std::optional<QJsonObject> entryEntry(const EditSession &, const Change &change,
                                              RecordLookup) {
            const auto &mapping = static_cast<const EntryChange &>(change);
            QJsonObject entry{
                {QStringLiteral("shape"), QStringLiteral("entry")},
                {QStringLiteral("key"),   mapping.key()          },
            };
            insertValues(entry, mapping.oldValue(), mapping.newValue());
            return entry;
        }

        std::optional<QJsonObject> arrayEntry(const EditSession &, const Change &, RecordLookup) {
            return QJsonObject{
                {QStringLiteral("shape"), QStringLiteral("array")}
            };
        }

        // A removal is also reported before it is applied, which the log omits, so that each
        // removal is logged once.
        std::optional<QJsonObject> listEntry(const EditSession &, const Change &change,
                                             RecordLookup) {
            const auto &list = static_cast<const ListChange &>(change);
            if (list.type() == ListChange::AboutToBeRemoved) {
                return std::nullopt;
            }
            const auto shape = list.type() == ListChange::Inserted ? QStringLiteral("insert")
                                                                   : QStringLiteral("remove");
            return QJsonObject{
                {QStringLiteral("shape"), shape       },
                {QStringLiteral("index"), list.index()},
                {QStringLiteral("count"), list.count()},
            };
        }

        std::optional<QJsonObject> moveEntry(const EditSession &, const Change &change,
                                             RecordLookup) {
            const auto &move = static_cast<const MoveChange &>(change);
            return QJsonObject{
                {QStringLiteral("shape"),       QStringLiteral("move")},
                {QStringLiteral("index"),       move.index()          },
                {QStringLiteral("count"),       move.count()          },
                {QStringLiteral("destination"), move.destination()    },
            };
        }

    }

    std::optional<QJsonObject> ChangeLog::entryOf(const EditSession &session, const Change &change,
                                                  RecordLookup lookup) {
        const auto &writers = EditSessionPrivate::impl(session).logWriters;
        const auto it = writers.find(change.kind());
        std::optional<QJsonObject> entry;
        if (it != writers.end()) {
            entry = it->second(session, change, lookup);
        } else {
            // Recorded rather than omitted, so that the log shows every change.
            entry = QJsonObject{
                {QStringLiteral("kind"), change.kind()}
            };
        }
        if (entry) {
            entry->insert(QStringLiteral("node"), qint64(change.node()));
        }
        return entry;
    }

    void ChangeLog::registerBuiltInWriters(EditSession &session) {
        EditSessionPrivate::registerLogWriter(session, ValueChange::Kind, valueEntry);
        EditSessionPrivate::registerLogWriter(session, EntryChange::Kind, entryEntry);
        EditSessionPrivate::registerLogWriter(session, ArrayChange::Kind, arrayEntry);
        EditSessionPrivate::registerLogWriter(session, ListChange::Kind, listEntry);
        EditSessionPrivate::registerLogWriter(session, MoveChange::Kind, moveEntry);
    }

}
