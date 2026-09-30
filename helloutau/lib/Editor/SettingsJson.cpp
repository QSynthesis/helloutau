#include "SettingsJson_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>
#include <QtCore/QtDebug>

namespace hello::daw {

    namespace json = stdc::json;

    json::Object SettingsJson::read(const QString &fileName) {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        const auto text = file.readAll();
        json::ParseError error;
        auto document = json::Value::fromJson(std::string_view(text.data(), size_t(text.size())),
                                              false, &error);
        if (const auto object = document.asObject()) {
            return std::move(*object);
        }
        qWarning().noquote() << "The settings in" << fileName
                             << "cannot be read:" << QString::fromStdString(error.message());
        return {};
    }

    void SettingsJson::write(const QString &fileName, const json::Value &value) {
        QDir().mkpath(QFileInfo(fileName).absolutePath());
        const auto text = value.toJson(4);
        QSaveFile file(fileName);
        if (!file.open(QIODevice::WriteOnly) || file.write(text.data(), qint64(text.size())) < 0 ||
            !file.commit()) {
            qWarning().noquote() << "The settings cannot be written to" << fileName << ":"
                                 << file.errorString();
        }
    }

    const json::Value &SettingsJson::valueAt(const json::Object &object, std::string_view key) {
        static const json::Value null;
        const json::Object *group = &object;
        for (;;) {
            const auto slash = key.find('/');
            const auto it = group->find(key.substr(0, slash));
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
            key = key.substr(slash + 1);
        }
    }

    void SettingsJson::insertAt(json::Object &object, std::string_view key, json::Value value) {
        const auto slash = key.find('/');
        const auto name = key.substr(0, slash);
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
        insertAt(group, key.substr(slash + 1), std::move(value));
        if (group.empty()) {
            object.erase(it);
        }
    }

    json::Value SettingsJson::stdcOf(const QJsonValue &value) {
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
                    array.push_back(stdcOf(item));
                }
                return json::Value(std::move(array));
            }
            case QJsonValue::Object: {
                json::Object object;
                const auto from = value.toObject();
                for (auto it = from.begin(); it != from.end(); ++it) {
                    object.emplace(it.key().toStdString(), stdcOf(it.value()));
                }
                return json::Value(std::move(object));
            }
            case QJsonValue::Null:
            case QJsonValue::Undefined:
                break;
        }
        return {};
    }

    QJsonValue SettingsJson::qtOf(const json::Value &value) {
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
                    array.push_back(qtOf(item));
                }
                return array;
            }
            case json::Type::Object: {
                QJsonObject object;
                for (const auto &[key, item] : value.toObject()) {
                    object.insert(QString::fromStdString(key), qtOf(item));
                }
                return object;
            }
            case json::Type::Null:
            case json::Type::Binary:
                break;
        }
        return QJsonValue::Null;
    }

    SettingsFile::SettingsFile(QString fileName, std::function<json::Value()> content)
        : m_fileName(std::move(fileName)), m_content(std::move(content)) {
        m_timer.setSingleShot(true);
        m_timer.setInterval(0);
        QObject::connect(&m_timer, &QTimer::timeout, &m_timer, [this] { sync(); });
    }

    SettingsFile::~SettingsFile() {
        sync();
    }

    void SettingsFile::changed() {
        if (m_pending) {
            return;
        }
        m_pending = true;
        // Without an application instance, no event loop exists to wait for.
        if (QCoreApplication::instance()) {
            m_timer.start();
        } else {
            sync();
        }
    }

    void SettingsFile::sync() {
        if (!m_pending) {
            return;
        }
        m_pending = false;
        m_timer.stop();
        SettingsJson::write(m_fileName, m_content());
    }

}
