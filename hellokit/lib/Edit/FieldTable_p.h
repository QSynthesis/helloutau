#ifndef HELLOKIT_EDIT_FIELDTABLE_P_H
#define HELLOKIT_EDIT_FIELDTABLE_P_H

#include <memory>
#include <optional>

#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QString>
#include <QtCore/QStringView>
#include <QtCore/QVariant>

#include <substate/Node.h>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Edit/HelloKitEditGlobal.h>
#include <hellokit/Edit/Slot.h>

namespace hello::kit {

    /// The conversion of the values of a field between JSON, the notation of commands and logs,
    /// and the QVariant stored in a node.
    struct ValueFormat {
        /// The name of the type in messages, such as \c number.
        const char *typeName;

        QJsonValue (*toJson)(const QVariant &value);

        /// Returns the value written as \a json, or \c std::nullopt if \a json is not a value of
        /// this type.
        std::optional<QVariant> (*fromJson)(const QJsonValue &json);
    };

    /// The formats of the value types that the generic layer supports.
    struct HELLOKIT_EDIT_EXPORT ValueFormats {
        static const ValueFormat string;

        /// A number without a fractional part within the range of \c int.
        static const ValueFormat integer;

        static const ValueFormat number;
        static const ValueFormat boolean;

        /// Any JSON value, stored as a \c QJsonValue.
        static const ValueFormat json;
    };

    /// Returns the format of the values of type \a T. A document declares an explicit
    /// specialization for each value type of its own, before its field table.
    template <class T>
    constexpr const ValueFormat &formatOf();

    template <>
    inline constexpr const ValueFormat &formatOf<QString>() {
        return ValueFormats::string;
    }

    template <>
    inline constexpr const ValueFormat &formatOf<int>() {
        return ValueFormats::integer;
    }

    template <>
    inline constexpr const ValueFormat &formatOf<double>() {
        return ValueFormats::number;
    }

    template <>
    inline constexpr const ValueFormat &formatOf<bool>() {
        return ValueFormats::boolean;
    }

    struct RecordInfo;

    /// A field of a record: its slot, its name and the kind of its content. Commands address the
    /// fields by name, and logs name the slots of changes.
    struct FieldInfo {
        enum Kind {
            /// A value in the slot.
            Value,

            /// A record in the slot.
            Record,

            /// A list of records in the slot.
            List,

            /// A mapping from strings to values in the slot.
            Mapping,

            /// An array of numbers in the slot, an \c ss::ArrayNode<double>.
            Array,
        };

        Kind kind = Value;
        int index = 0;
        const char *name = nullptr;

        /// The format of the value of a \c Value field, of the values of a \c Mapping, or of the
        /// elements of an \c Array.
        const ValueFormat *format = nullptr;

        /// Whether a \c Value field may be empty, which is written as null, or a \c Record field
        /// may be absent.
        bool optional = false;

        /// The permitted values of a numeric \c Value field.
        std::optional<Range> range;

        /// The record of a \c Record field, or of the items of a \c List.
        const RecordInfo *record = nullptr;

        /// The node type of the array of an \c Array field.
        int arrayType = 0;
    };

    /// The fields of a record type, an array with static storage in the order of the slots.
    ///
    /// The array is not a QList, so that a field table is a constant expression, created by the
    /// compiler rather than when the program runs.
    class FieldList {
    public:
        template <int N>
        inline constexpr FieldList(const FieldInfo (&fields)[N]) : m_fields(fields), m_size(N) {
        }

        inline constexpr int size() const {
            return m_size;
        }

        inline constexpr const FieldInfo &at(int index) const {
            return m_fields[index];
        }

        inline constexpr const FieldInfo *begin() const {
            return m_fields;
        }

        inline constexpr const FieldInfo *end() const {
            return m_fields + m_size;
        }

    private:
        const FieldInfo *m_fields;
        int m_size;
    };

    /// The fields of a record type.
    struct RecordInfo {
        /// The name of the record type in messages and commands, such as \c note.
        const char *name;

        int nodeType;

        FieldList fields;

        /// Returns the tree of the record written as \a json, or \c nullptr with the reason in
        /// \a diagnostics. Null if no record of this type is created from JSON.
        std::unique_ptr<ss::Node> (*treeFromJson)(const QJsonObject &json,
                                                  DiagnosticList &diagnostics) = nullptr;

        /// Returns the JSON of the record in \a tree. Null if \c treeFromJson is null.
        QJsonObject (*treeToJson)(const ss::Node *tree) = nullptr;

        /// Returns the field named \a name, or \c nullptr if the record has no such field.
        inline const FieldInfo *field(QStringView name) const {
            for (const auto &field : fields) {
                if (name == QLatin1String(field.name)) {
                    return &field;
                }
            }
            return nullptr;
        }
    };

    // The fields of each kind, created from the slots of a schema.

    template <class T>
    inline constexpr FieldInfo valueField(Slot<T> slot) {
        FieldInfo field;
        field.index = slot.index;
        field.name = slot.name;
        field.format = &formatOf<T>();
        field.range = slot.range;
        return field;
    }

    template <class T>
    inline constexpr FieldInfo valueField(Slot<std::optional<T>> slot) {
        auto field = valueField(Slot<T>{slot.index, slot.name, slot.range});
        field.optional = true;
        return field;
    }

    inline constexpr FieldInfo childField(FieldInfo::Kind kind, ChildSlot slot) {
        FieldInfo field;
        field.kind = kind;
        field.index = slot.index;
        field.name = slot.name;
        return field;
    }

    inline constexpr FieldInfo recordField(ChildSlot slot, const RecordInfo &record,
                                           bool optional) {
        auto field = childField(FieldInfo::Record, slot);
        field.record = &record;
        field.optional = optional;
        return field;
    }

    inline constexpr FieldInfo listField(ChildSlot slot, const RecordInfo &item) {
        auto field = childField(FieldInfo::List, slot);
        field.record = &item;
        return field;
    }

    inline constexpr FieldInfo mappingField(ChildSlot slot, const ValueFormat &format) {
        auto field = childField(FieldInfo::Mapping, slot);
        field.format = &format;
        return field;
    }

    inline constexpr FieldInfo arrayField(ChildSlot slot, int arrayType,
                                          const ValueFormat &format) {
        auto field = childField(FieldInfo::Array, slot);
        field.arrayType = arrayType;
        field.format = &format;
        return field;
    }

}

#endif // HELLOKIT_EDIT_FIELDTABLE_P_H
