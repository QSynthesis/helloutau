#include "JsonInterop.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>

namespace hello::kit {

    namespace json = stdc::json;

    json::Value JsonInterop::fromQtJson(const QJsonValue &value) {
        switch (value.type()) {
            case QJsonValue::Bool:
                return json::Value(value.toBool());
            case QJsonValue::Double: {
                const auto integer = value.toInteger();
                return double(integer) == value.toDouble() ? json::Value(int64_t(integer))
                                                           : json::Value(value.toDouble());
            }
            case QJsonValue::String:
                return json::Value(value.toString().toStdString());
            case QJsonValue::Array: {
                json::Array array;
                for (const auto &item : value.toArray()) {
                    array.push_back(fromQtJson(item));
                }
                return json::Value(std::move(array));
            }
            case QJsonValue::Object: {
                json::Object object;
                const auto from = value.toObject();
                for (auto it = from.begin(); it != from.end(); ++it) {
                    object.emplace(it.key().toStdString(), fromQtJson(it.value()));
                }
                return json::Value(std::move(object));
            }
            case QJsonValue::Null:
            case QJsonValue::Undefined:
                break;
        }
        return {};
    }

    QJsonValue JsonInterop::toQtJson(const json::Value &value) {
        switch (value.type()) {
            case json::Type::Bool:
                return value.toBool();
            case json::Type::Int:
                return qint64(value.toInt());
            case json::Type::Double:
                return value.toDouble();
            case json::Type::String:
                return QString::fromStdString(value.toString());
            case json::Type::Array: {
                QJsonArray array;
                for (const auto &item : value.toArray()) {
                    array.push_back(toQtJson(item));
                }
                return array;
            }
            case json::Type::Object: {
                QJsonObject object;
                for (const auto &[key, item] : value.toObject()) {
                    object.insert(QString::fromStdString(key), toQtJson(item));
                }
                return object;
            }
            case json::Type::Null:
            case json::Type::Binary:
                break;
        }
        return QJsonValue::Null;
    }

    const json::Value &JsonInterop::valueAt(const json::Object &object, std::string_view path) {
        static const json::Value null;
        const json::Object *group = &object;
        for (;;) {
            const auto slash = path.find('/');
            const auto it = group->find(path.substr(0, slash));
            if (it == group->end()) {
                return null;
            }
            if (slash == std::string_view::npos) {
                return it->second;
            }
            group = it->second.asObject();
            if (!group) {
                return null;
            }
            path = path.substr(slash + 1);
        }
    }

    void JsonInterop::insertAt(json::Object &object, std::string_view path, json::Value value) {
        const auto slash = path.find('/');
        const auto name = path.substr(0, slash);
        auto it = object.find(name);
        if (slash == std::string_view::npos) {
            if (!value.isNull()) {
                object.insert_or_assign(std::string(name), std::move(value));
            } else if (it != object.end()) {
                object.erase(it);
            }
            return;
        }
        if (it == object.end() || !it->second.isObject()) {
            if (value.isNull()) {
                return;
            }
            it = object.insert_or_assign(std::string(name), json::Value(json::Object())).first;
        }
        auto &group = *it->second.asObject();
        insertAt(group, path.substr(slash + 1), std::move(value));
        if (group.empty()) {
            object.erase(it);
        }
    }

}
