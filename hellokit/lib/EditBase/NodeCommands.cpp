#include "NodeCommands_p.h"

#include <vector>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>

#include <substate/ArrayNode.h>
#include <substate/VectorNode.h>
#include <qsubstate/MappingNode.h>

#include "EditSession_p.h"

namespace hello::kit::edit {

    namespace {

        using Target = NodeCommands::Target;

        bool fail(DiagnosticList &diagnostics, const QString &message) {
            return NodeCommands::fail(diagnostics, message);
        }

        QString nameOf(const FieldInfo &field) {
            return QString::fromLatin1(field.name);
        }

        // Refuses a command on a mapping that is absent from its slot. Only the document layer
        // creates it, because an absent mapping differs from an empty one, as a missing file
        // differs from an empty file.
        bool absent(const FieldInfo &field, DiagnosticList &diagnostics) {
            return fail(diagnostics, NodeCommands::tr("The %1 is absent.").arg(nameOf(field)));
        }

        bool usage(DiagnosticList &diagnostics, const char *form) {
            return fail(diagnostics, NodeCommands::tr("Usage: %1").arg(QLatin1String(form)));
        }

        // Returns the index written as decimal digits, or std::nullopt.
        std::optional<int> indexOf(QStringView text) {
            if (text.isEmpty()) {
                return std::nullopt;
            }
            for (const auto c : text) {
                if (c < QLatin1Char('0') || c > QLatin1Char('9')) {
                    return std::nullopt;
                }
            }
            bool ok = false;
            const auto index = text.toInt(&ok);
            return ok ? std::optional<int>(index) : std::nullopt;
        }

        QString jsonTypeName(const QJsonValue &value) {
            switch (value.type()) {
                case QJsonValue::Bool:
                    return NodeCommands::tr("boolean");
                case QJsonValue::Double:
                    return NodeCommands::tr("number");
                case QJsonValue::String:
                    return NodeCommands::tr("string");
                case QJsonValue::Array:
                    return NodeCommands::tr("array");
                case QJsonValue::Object:
                    return NodeCommands::tr("object");
                default:
                    break;
            }
            return NodeCommands::tr("null");
        }

        // Returns the value of field written as json. A whole value is read as from a file, which
        // ignores unknown members and reads a member of another type as a default. Each member
        // that json states must therefore read back unchanged.
        std::optional<QVariant> variantOf(const FieldInfo &field, const QJsonValue &json,
                                          DiagnosticList &diagnostics) {
            if (json.isNull() && field.optional) {
                return QVariant();
            }
            const auto variant = field.format->fromJson(json);
            if (!variant) {
                fail(diagnostics, (field.optional ? NodeCommands::tr("The %1 must be a %2 or null.")
                                                  : NodeCommands::tr("The %1 must be a %2."))
                                      .arg(nameOf(field), QLatin1String(field.format->typeName)));
                return std::nullopt;
            }
            if (json.isObject()) {
                const auto object = json.toObject();
                const auto back = field.format->toJson(*variant).toObject();
                for (auto it = object.begin(); it != object.end(); ++it) {
                    if (it.value() != back.value(it.key())) {
                        fail(diagnostics, NodeCommands::tr("The member %1 of the %2 is not valid.")
                                              .arg(it.key(), nameOf(field)));
                        return std::nullopt;
                    }
                }
            }
            return variant;
        }

        // Replaces the member of json at members with value. The member must exist, and value
        // must have the JSON type of its current value.
        bool replaceMember(QJsonValue &json, const QStringList &members, qsizetype depth,
                           const QJsonValue &value, const FieldInfo &field,
                           DiagnosticList &diagnostics) {
            if (depth == members.size()) {
                if (json.type() != value.type()) {
                    return fail(diagnostics,
                                NodeCommands::tr("The member %1 of the %2 must be a %3.")
                                    .arg(members.join(QLatin1Char('/')), nameOf(field),
                                         jsonTypeName(json)));
                }
                json = value;
                return true;
            }
            const auto &member = members.at(depth);
            const auto missing = [&] {
                return fail(
                    diagnostics,
                    NodeCommands::tr("The %1 has no member %2.")
                        .arg(nameOf(field), members.mid(0, depth + 1).join(QLatin1Char('/'))));
            };
            if (json.isObject()) {
                auto object = json.toObject();
                if (!object.contains(member)) {
                    return missing();
                }
                auto child = object.value(member);
                if (!replaceMember(child, members, depth + 1, value, field, diagnostics)) {
                    return false;
                }
                object.insert(member, child);
                json = object;
                return true;
            }
            if (json.isArray()) {
                auto array = json.toArray();
                const auto index = indexOf(member);
                if (!index || *index >= array.size()) {
                    return missing();
                }
                auto child = array.at(*index);
                if (!replaceMember(child, members, depth + 1, value, field, diagnostics)) {
                    return false;
                }
                array.replace(*index, child);
                json = array;
                return true;
            }
            return missing();
        }

        // Returns whether json holds only fields of info, each with a value of its field.
        bool checkRecord(const RecordInfo &info, const QJsonObject &json,
                         DiagnosticList &diagnostics) {
            for (auto it = json.begin(); it != json.end(); ++it) {
                const auto field = info.field(it.key());
                if (!field) {
                    return fail(diagnostics, NodeCommands::tr("The %1 has no field %2.")
                                                 .arg(QLatin1String(info.name), it.key()));
                }
                const auto value = it.value();
                switch (field->kind) {
                    case FieldInfo::Value:
                        if (!variantOf(*field, value, diagnostics)) {
                            return false;
                        }
                        break;
                    case FieldInfo::Record:
                        if (value.isNull() && field->optional) {
                            break;
                        }
                        if (!value.isObject()) {
                            return fail(
                                diagnostics,
                                NodeCommands::tr("The %1 must be an object.").arg(nameOf(*field)));
                        }
                        if (!checkRecord(*field->record, value.toObject(), diagnostics)) {
                            return false;
                        }
                        break;
                    case FieldInfo::List:
                        if (!value.isArray()) {
                            return fail(
                                diagnostics,
                                NodeCommands::tr("The %1 must be an array.").arg(nameOf(*field)));
                        }
                        for (const auto item : value.toArray()) {
                            if (!item.isObject()) {
                                return fail(diagnostics,
                                            NodeCommands::tr("The items of the %1 must be objects.")
                                                .arg(nameOf(*field)));
                            }
                            if (!checkRecord(*field->record, item.toObject(), diagnostics)) {
                                return false;
                            }
                        }
                        break;
                    case FieldInfo::Mapping:
                        if (!value.isObject()) {
                            return fail(
                                diagnostics,
                                NodeCommands::tr("The %1 must be an object.").arg(nameOf(*field)));
                        }
                        for (const auto entry : value.toObject()) {
                            if (!field->format->fromJson(entry)) {
                                return fail(diagnostics,
                                            NodeCommands::tr("The values of the %1 must be %2s.")
                                                .arg(nameOf(*field),
                                                     QLatin1String(field->format->typeName)));
                            }
                        }
                        break;
                    case FieldInfo::Array:
                        if (!value.isArray()) {
                            return fail(
                                diagnostics,
                                NodeCommands::tr("The %1 must be an array.").arg(nameOf(*field)));
                        }
                        for (const auto element : value.toArray()) {
                            if (!field->format->fromJson(element)) {
                                return fail(diagnostics,
                                            NodeCommands::tr("The elements of the %1 must be %2s.")
                                                .arg(nameOf(*field),
                                                     QLatin1String(field->format->typeName)));
                            }
                        }
                        break;
                }
            }
            return true;
        }

        // Returns whether index and count denote items within a sequence of size items.
        bool checkRange(int index, int count, int size, const FieldInfo &field,
                        DiagnosticList &diagnostics) {
            if (count < 1) {
                return fail(diagnostics, NodeCommands::tr("The count must be at least 1."));
            }
            if (index < 0 || index + count > size) {
                return fail(diagnostics,
                            NodeCommands::tr("The %1 has %2 items, not an item %3 to %4.")
                                .arg(nameOf(field))
                                .arg(size)
                                .arg(index)
                                .arg(index + count - 1));
            }
            return true;
        }

        // Returns whether index is a position to insert at in a sequence of size items.
        bool checkPosition(int index, int size, const FieldInfo &field,
                           DiagnosticList &diagnostics) {
            if (index < 0 || index > size) {
                return fail(diagnostics,
                            NodeCommands::tr("The %1 has %2 items, therefore %3 is not a position.")
                                .arg(nameOf(field))
                                .arg(size)
                                .arg(index));
            }
            return true;
        }

        // Returns the numbers of arguments, or std::nullopt with the reason in diagnostics.
        std::optional<std::vector<double>> numbersOf(const QList<CommandArgument> &arguments,
                                                     const FieldInfo &field,
                                                     DiagnosticList &diagnostics) {
            std::vector<double> numbers;
            for (const auto &argument : arguments) {
                const auto variant = field.format->fromJson(CommandSyntax::valueOf(argument));
                if (!variant) {
                    fail(diagnostics,
                         NodeCommands::tr("The elements of the %1 must be %2s.")
                             .arg(nameOf(field), QLatin1String(field.format->typeName)));
                    return std::nullopt;
                }
                numbers.push_back(variant->toDouble());
            }
            return numbers;
        }

        ss::ArrayView<double> viewOf(const std::vector<double> &numbers) {
            return ss::ArrayView<double>(numbers.data(), numbers.size());
        }

        bool setCommand(const Target &target, const QList<CommandArgument> &arguments,
                        DiagnosticList &diagnostics) {
            if (!target.field) {
                return fail(diagnostics,
                            NodeCommands::tr("A %1 is not set as a whole. Set its fields instead.")
                                .arg(QLatin1String(target.info->name)));
            }
            const auto &field = *target.field;
            switch (field.kind) {
                case FieldInfo::Value: {
                    if (arguments.size() != 1) {
                        return usage(diagnostics, "set <path> <value>");
                    }
                    auto json = CommandSyntax::valueOf(arguments[0]);
                    if (!target.members.isEmpty()) {
                        const auto current = target.record->variant(field.index);
                        if (!current.isValid()) {
                            return fail(diagnostics,
                                        NodeCommands::tr("The %1 is empty. Set it as a whole.")
                                            .arg(nameOf(field)));
                        }
                        auto whole = field.format->toJson(current);
                        if (!replaceMember(whole, target.members, 0, json, field, diagnostics)) {
                            return false;
                        }
                        json = whole;
                    }
                    const auto variant = variantOf(field, json, diagnostics);
                    if (!variant) {
                        return false;
                    }
                    target.record->setAt(field.index, *variant);
                    return true;
                }
                case FieldInfo::Record: {
                    if (arguments.size() != 1) {
                        return usage(diagnostics, "set <path> <object|null>");
                    }
                    if (!field.optional) {
                        return fail(diagnostics,
                                    NodeCommands::tr(
                                        "The %1 is not set as a whole. Set its fields instead.")
                                        .arg(nameOf(field)));
                    }
                    const auto json = CommandSyntax::valueOf(arguments[0]);
                    if (json.isNull()) {
                        target.record->setAt(field.index, ss::Property());
                        return true;
                    }
                    if (!json.isObject()) {
                        return fail(diagnostics,
                                    NodeCommands::tr("The %1 must be an object or null.")
                                        .arg(nameOf(field)));
                    }
                    auto tree = NodeCommands::treeOf(*field.record, json.toObject(), diagnostics);
                    if (!tree) {
                        return false;
                    }
                    // A new tree is never equal to the current child, therefore their JSON is
                    // compared, so that an equal value creates no change.
                    const auto current = target.child();
                    if (current &&
                        field.record->treeToJson(current) == field.record->treeToJson(tree.get())) {
                        return true;
                    }
                    target.record->setAt(field.index, ss::Property(std::move(tree)));
                    return true;
                }
                case FieldInfo::Mapping: {
                    if (arguments.size() != 2) {
                        return usage(diagnostics, "set <path> <key> <value>");
                    }
                    if (!target.child()) {
                        return absent(field, diagnostics);
                    }
                    const auto key =
                        NodeCommands::stringOf(arguments[0], NodeCommands::tr("key"), diagnostics);
                    if (!key) {
                        return false;
                    }
                    const auto variant =
                        field.format->fromJson(CommandSyntax::valueOf(arguments[1]));
                    if (!variant) {
                        return fail(diagnostics,
                                    NodeCommands::tr("The values of the %1 must be %2s.")
                                        .arg(nameOf(field), QLatin1String(field.format->typeName)));
                    }
                    static_cast<ss::MappingNode *>(target.child())->setProperty(*key, *variant);
                    return true;
                }
                case FieldInfo::List:
                case FieldInfo::Array:
                    break;
            }
            return fail(
                diagnostics,
                NodeCommands::tr("The %1 is not set as a whole. Insert, remove or move its items.")
                    .arg(nameOf(field)));
        }

        bool insertCommand(const Target &target, const QList<CommandArgument> &arguments,
                           DiagnosticList &diagnostics) {
            const auto field = target.field;
            if (!field || !target.members.isEmpty() ||
                (field->kind != FieldInfo::List && field->kind != FieldInfo::Array)) {
                return fail(diagnostics,
                            NodeCommands::tr("Only a list or an array accepts insert."));
            }
            if (arguments.size() < 2) {
                return usage(diagnostics, "insert <path> <index> <value>...");
            }
            const auto index =
                NodeCommands::integerOf(arguments[0], NodeCommands::tr("index"), diagnostics);
            if (!index) {
                return false;
            }
            const auto values = arguments.mid(1);

            if (field->kind == FieldInfo::Array) {
                const auto array = static_cast<ss::ArrayNode<double> *>(target.child());
                const auto numbers = numbersOf(values, *field, diagnostics);
                if (!numbers || !checkPosition(*index, array->size(), *field, diagnostics)) {
                    return false;
                }
                array->insert(*index, viewOf(*numbers));
                return true;
            }

            const auto list = static_cast<ss::VectorNode *>(target.child());
            if (!checkPosition(*index, list->size(), *field, diagnostics)) {
                return false;
            }
            std::vector<std::unique_ptr<ss::Node>> items;
            for (const auto &value : values) {
                const auto json = CommandSyntax::valueOf(value);
                if (!json.isObject()) {
                    return fail(diagnostics,
                                NodeCommands::tr("The items of the %1 must be objects.")
                                    .arg(nameOf(*field)));
                }
                auto item = NodeCommands::treeOf(*field->record, json.toObject(), diagnostics);
                if (!item) {
                    return false;
                }
                items.push_back(std::move(item));
            }
            list->insert(*index, std::move(items));
            return true;
        }

        bool removeCommand(const Target &target, const QList<CommandArgument> &arguments,
                           DiagnosticList &diagnostics) {
            const auto field = target.field;
            if (field && target.members.isEmpty() && field->kind == FieldInfo::Mapping) {
                if (arguments.size() != 1) {
                    return usage(diagnostics, "remove <path> <key>");
                }
                if (!target.child()) {
                    return absent(*field, diagnostics);
                }
                const auto key =
                    NodeCommands::stringOf(arguments[0], NodeCommands::tr("key"), diagnostics);
                if (!key) {
                    return false;
                }
                const auto mapping = static_cast<ss::MappingNode *>(target.child());
                if (!mapping->contains(*key)) {
                    return fail(
                        diagnostics,
                        NodeCommands::tr("The %1 has no entry %2.").arg(nameOf(*field), *key));
                }
                mapping->setProperty(*key, ss::Property());
                return true;
            }

            if (!field || !target.members.isEmpty() ||
                (field->kind != FieldInfo::List && field->kind != FieldInfo::Array)) {
                return fail(diagnostics,
                            NodeCommands::tr("Only a list, an array or a mapping accepts remove."));
            }
            if (arguments.isEmpty() || arguments.size() > 2) {
                return usage(diagnostics, "remove <path> <index> [<count>]");
            }
            const auto index =
                NodeCommands::integerOf(arguments[0], NodeCommands::tr("index"), diagnostics);
            const auto count =
                arguments.size() == 2
                    ? NodeCommands::integerOf(arguments[1], NodeCommands::tr("count"), diagnostics)
                    : std::optional<int>(1);
            if (!index || !count) {
                return false;
            }
            if (field->kind == FieldInfo::Array) {
                const auto array = static_cast<ss::ArrayNode<double> *>(target.child());
                if (!checkRange(*index, *count, array->size(), *field, diagnostics)) {
                    return false;
                }
                array->remove(*index, *count);
                return true;
            }
            const auto list = static_cast<ss::VectorNode *>(target.child());
            if (!checkRange(*index, *count, list->size(), *field, diagnostics)) {
                return false;
            }
            list->remove(*index, *count);
            return true;
        }

        bool moveCommand(const Target &target, const QList<CommandArgument> &arguments,
                         DiagnosticList &diagnostics) {
            const auto field = target.field;
            if (!field || !target.members.isEmpty() || field->kind != FieldInfo::List) {
                return fail(diagnostics, NodeCommands::tr("Only a list accepts move."));
            }
            if (arguments.size() != 3) {
                return usage(diagnostics, "move <path> <index> <count> <destination>");
            }
            const auto index =
                NodeCommands::integerOf(arguments[0], NodeCommands::tr("index"), diagnostics);
            const auto count =
                NodeCommands::integerOf(arguments[1], NodeCommands::tr("count"), diagnostics);
            const auto destination =
                NodeCommands::integerOf(arguments[2], NodeCommands::tr("destination"), diagnostics);
            if (!index || !count || !destination) {
                return false;
            }
            const auto list = static_cast<ss::VectorNode *>(target.child());
            if (!checkRange(*index, *count, list->size(), *field, diagnostics)) {
                return false;
            }
            // The destination is the index of the first item after the move.
            if (*destination < 0 || *destination > list->size() - *count) {
                return fail(diagnostics,
                            NodeCommands::tr("The %1 has %2 items, therefore %3 items cannot start "
                                             "at %4.")
                                .arg(nameOf(*field))
                                .arg(list->size())
                                .arg(*count)
                                .arg(*destination));
            }
            if (*destination != *index) {
                list->move(*index, *count, *destination);
            }
            return true;
        }

        bool replaceCommand(const Target &target, const QList<CommandArgument> &arguments,
                            DiagnosticList &diagnostics) {
            const auto field = target.field;
            if (!field || !target.members.isEmpty() || field->kind != FieldInfo::Array) {
                return fail(diagnostics, NodeCommands::tr("Only an array accepts replace."));
            }
            if (arguments.size() < 2) {
                return usage(diagnostics, "replace <path> <index> <number>...");
            }
            const auto index =
                NodeCommands::integerOf(arguments[0], NodeCommands::tr("index"), diagnostics);
            if (!index) {
                return false;
            }
            const auto array = static_cast<ss::ArrayNode<double> *>(target.child());
            const auto numbers = numbersOf(arguments.mid(1), *field, diagnostics);
            // The elements may extend beyond the end, but must begin within the array or at its
            // end.
            if (!numbers || !checkPosition(*index, array->size(), *field, diagnostics)) {
                return false;
            }
            array->replace(*index, viewOf(*numbers));
            return true;
        }

    }

    std::optional<Target> NodeCommands::resolve(const EditSession &session, const RecordInfo &root,
                                                QStringView path, DiagnosticList &diagnostics) {
        if (!path.startsWith(QLatin1Char('/'))) {
            fail(diagnostics,
                 NodeCommands::tr("The path %1 does not begin with a slash.").arg(path.toString()));
            return std::nullopt;
        }
        Target target;
        target.record =
            static_cast<ss::StructNodeBase *>(EditSessionPrivate::find(&session, session.root()));
        target.info = &root;
        Q_ASSERT(target.record && target.record->type() == root.nodeType);
        if (path.size() == 1) {
            return target;
        }

        const auto segments = path.mid(1).split(QLatin1Char('/'));
        for (qsizetype i = 0; i < segments.size();) {
            // An empty segment names no field and no index, and is refused as either.
            const auto segment = segments[i++];
            if (target.field) {
                target.members.push_back(segment.toString());
                continue;
            }
            const auto field = target.info->field(segment);
            if (!field) {
                fail(diagnostics, NodeCommands::tr("The %1 has no field %2.")
                                      .arg(QLatin1String(target.info->name), segment.toString()));
                return std::nullopt;
            }
            target.field = field;
            if (i == segments.size()) {
                break;
            }
            switch (field->kind) {
                case FieldInfo::Value:
                    // The following segments are members of the value.
                    break;
                case FieldInfo::Record: {
                    const auto child = target.child();
                    if (!child) {
                        fail(diagnostics, NodeCommands::tr("The %1 is empty.").arg(nameOf(*field)));
                        return std::nullopt;
                    }
                    target.record = static_cast<ss::StructNodeBase *>(child);
                    target.info = field->record;
                    target.field = nullptr;
                    break;
                }
                case FieldInfo::List: {
                    const auto list = static_cast<const ss::VectorNode *>(target.child());
                    const auto item = segments[i++];
                    const auto index = indexOf(item);
                    if (!index || *index >= list->size()) {
                        fail(diagnostics, NodeCommands::tr("The %1 has %2 items, not an item %3.")
                                              .arg(nameOf(*field))
                                              .arg(list->size())
                                              .arg(item.toString()));
                        return std::nullopt;
                    }
                    target.record = static_cast<ss::StructNodeBase *>(list->at(*index));
                    target.info = field->record;
                    target.field = nullptr;
                    break;
                }
                case FieldInfo::Mapping:
                case FieldInfo::Array:
                    fail(diagnostics,
                         NodeCommands::tr(
                             "The path %1 continues after the %2, which has no fields. A key of "
                             "a mapping is an argument of the command.")
                             .arg(path.toString(), nameOf(*field)));
                    return std::nullopt;
            }
        }
        return target;
    }

    namespace {

        using Command = bool (*)(const Target &, const QList<CommandArgument> &, DiagnosticList &);

        constexpr std::pair<const char *, Command> commands[] = {
            {"set",     setCommand    },
            {"insert",  insertCommand },
            {"remove",  removeCommand },
            {"move",    moveCommand   },
            {"replace", replaceCommand},
        };

    }

    QStringList NodeCommands::names() {
        QStringList names;
        for (const auto &[name, command] : commands) {
            Q_UNUSED(command)
            names.push_back(QLatin1String(name));
        }
        return names;
    }

    bool NodeCommands::execute(EditSession &session, const RecordInfo &root, QStringView name,
                               const QList<CommandArgument> &arguments,
                               DiagnosticList &diagnostics) {
        Q_ASSERT(session.inTransaction());
        for (const auto &[commandName, command] : commands) {
            if (name != QLatin1String(commandName)) {
                continue;
            }
            if (arguments.isEmpty()) {
                return fail(
                    diagnostics,
                    NodeCommands::tr("The command %1 requires a path.").arg(name.toString()));
            }
            const auto path = stringOf(arguments[0], NodeCommands::tr("path"), diagnostics);
            if (!path) {
                return false;
            }
            const auto target = resolve(session, root, *path, diagnostics);
            if (!target) {
                return false;
            }
            if (target->field && target->field->readOnly) {
                return fail(diagnostics,
                            NodeCommands::tr("The %1 is read-only.").arg(nameOf(*target->field)));
            }
            return command(*target, arguments.mid(1), diagnostics);
        }
        return fail(diagnostics, NodeCommands::tr("%1 is not a command.").arg(name.toString()));
    }

    std::optional<int> NodeCommands::integerOf(const CommandArgument &argument, const QString &what,
                                               DiagnosticList &diagnostics) {
        const auto value = ValueFormats::integer.fromJson(CommandSyntax::valueOf(argument));
        if (!value) {
            fail(diagnostics, NodeCommands::tr("The %1 must be an integer.").arg(what));
            return std::nullopt;
        }
        return value->toInt();
    }

    std::optional<double> NodeCommands::numberOf(const CommandArgument &argument,
                                                 const QString &what, DiagnosticList &diagnostics) {
        const auto value = ValueFormats::number.fromJson(CommandSyntax::valueOf(argument));
        if (!value) {
            fail(diagnostics, NodeCommands::tr("The %1 must be a number.").arg(what));
            return std::nullopt;
        }
        return value->toDouble();
    }

    std::optional<QString> NodeCommands::stringOf(const CommandArgument &argument,
                                                  const QString &what,
                                                  DiagnosticList &diagnostics) {
        const auto value = CommandSyntax::valueOf(argument);
        if (!value.isString()) {
            fail(diagnostics, NodeCommands::tr("The %1 must be a string.").arg(what));
            return std::nullopt;
        }
        return value.toString();
    }

    std::unique_ptr<ss::Node> NodeCommands::treeOf(const RecordInfo &info, const QJsonObject &json,
                                                   DiagnosticList &diagnostics) {
        if (!info.treeFromJson) {
            fail(diagnostics, NodeCommands::tr("A %1 is not created from a value.")
                                  .arg(QLatin1String(info.name)));
            return nullptr;
        }
        if (!checkRecord(info, json, diagnostics)) {
            return nullptr;
        }
        // Every field has been checked, therefore the conversion reports only a missing field that
        // the record requires.
        return info.treeFromJson(json, diagnostics);
    }

    bool NodeCommands::fail(DiagnosticList &diagnostics, const QString &message) {
        Diagnostic diagnostic;
        diagnostic.severity = DiagnosticSeverity::Error;
        diagnostic.message = message;
        diagnostics.push_back(diagnostic);
        return false;
    }

}
