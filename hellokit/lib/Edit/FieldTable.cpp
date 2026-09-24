#include "FieldTable_p.h"

#include <cmath>
#include <limits>

namespace hello::kit {

    namespace {

        QJsonValue stringToJson(const QVariant &value) {
            return SlotValue<QString>::fromVariant(value);
        }

        std::optional<QVariant> stringFromJson(const QJsonValue &json) {
            if (!json.isString()) {
                return std::nullopt;
            }
            return SlotValue<QString>::toVariant(json.toString());
        }

        QJsonValue integerToJson(const QVariant &value) {
            return SlotValue<int>::fromVariant(value);
        }

        std::optional<QVariant> integerFromJson(const QJsonValue &json) {
            const auto number = json.toDouble();
            if (!json.isDouble() || std::trunc(number) != number ||
                number < std::numeric_limits<int>::min() ||
                number > std::numeric_limits<int>::max()) {
                return std::nullopt;
            }
            return SlotValue<int>::toVariant(int(number));
        }

        QJsonValue numberToJson(const QVariant &value) {
            return SlotValue<double>::fromVariant(value);
        }

        std::optional<QVariant> numberFromJson(const QJsonValue &json) {
            if (!json.isDouble()) {
                return std::nullopt;
            }
            return SlotValue<double>::toVariant(json.toDouble());
        }

        QJsonValue booleanToJson(const QVariant &value) {
            return SlotValue<bool>::fromVariant(value);
        }

        std::optional<QVariant> booleanFromJson(const QJsonValue &json) {
            if (!json.isBool()) {
                return std::nullopt;
            }
            return SlotValue<bool>::toVariant(json.toBool());
        }

        QJsonValue jsonToJson(const QVariant &value) {
            return value.value<QJsonValue>();
        }

        std::optional<QVariant> jsonFromJson(const QJsonValue &json) {
            return QVariant::fromValue(json);
        }

    }

    const ValueFormat ValueFormats::string{"string", stringToJson, stringFromJson};
    const ValueFormat ValueFormats::integer{"integer", integerToJson, integerFromJson};
    const ValueFormat ValueFormats::number{"number", numberToJson, numberFromJson};
    const ValueFormat ValueFormats::boolean{"boolean", booleanToJson, booleanFromJson};
    const ValueFormat ValueFormats::json{"JSON value", jsonToJson, jsonFromJson};

}
