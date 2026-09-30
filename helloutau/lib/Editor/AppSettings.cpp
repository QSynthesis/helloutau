#include "AppSettings.h"
#include "AppSettings_p.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>
#include <QtCore/QStandardPaths>
#include <QtCore/QtDebug>

#include <stdcorelib/pimpl.h>

namespace hello::daw {

    namespace json = stdc::json;

    namespace {

        constexpr char KeyUtauDirectory[] = "engines/utauDirectory";
        constexpr char KeyResampler[] = "engines/resampler";
        constexpr char KeyWavtool[] = "engines/wavtool";
        constexpr char KeyPlaybackMode[] = "playback/mode";
        constexpr char PrerenderValue[] = "prerender";
        constexpr char RealtimeValue[] = "realtime";
        constexpr char KeyUstExportCharset[] = "files/ustExportCharset";
        constexpr char KeyRecentCommands[] = "commandPalette/recent";
        constexpr char KeyRecentFiles[] = "files/recent";
        constexpr char KeyRecentVoiceBanks[] = "files/recentVoiceBanks";
        constexpr char KeyPluginData[] = "plugins/userData/";

        QString textOf(const std::string &utf8) {
            return QString::fromStdString(utf8);
        }

        // A path as the settings keep it, whole
        QString textOf(const std::filesystem::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

        QStringList stringsOf(const json::Value &value) {
            QStringList strings;
            for (const auto &item : value.toArray()) {
                strings.push_back(textOf(item.toString()));
            }
            return strings;
        }

        json::Value arrayOf(const QStringList &strings) {
            json::Array array;
            array.reserve(size_t(strings.size()));
            for (const auto &string : strings) {
                array.emplace_back(string.toStdString());
            }
            return json::Value(std::move(array));
        }

        // The value in the JSON of Qt, at the boundary of the public interface. A number without
        // a fractional part that fits an integer becomes one, as the JSON of Qt keeps it.
        json::Value stdcOf(const QJsonValue &value) {
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

        // The other way. Binary data, which no setting holds, has no counterpart and reads as
        // null.
        QJsonValue qtOf(const json::Value &value) {
            switch (value.type()) {
                case json::Type::Bool:
                    return value.toBool();
                case json::Type::Int:
                    return qint64(value.toInt());
                case json::Type::Double:
                    return value.toDouble();
                case json::Type::String:
                    return textOf(value.toString());
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
                        object.insert(textOf(key), qtOf(item));
                    }
                    return object;
                }
                case json::Type::Null:
                case json::Type::Binary:
                    break;
            }
            return QJsonValue::Null;
        }

        // Replaces the value at \a key in \a object, or removes it if \a value is null, and with
        // it each group that it leaves empty.
        void insertAt(json::Object &object, std::string_view key, json::Value &&value) {
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

    }

    AppSettings::Impl::Impl(const QString &fileName) : fileName(fileName) {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }
        const auto text = file.readAll();
        json::ParseError error;
        auto document = json::Value::fromJson(std::string_view(text.data(), size_t(text.size())),
                                              false, &error);
        if (const auto object = document.asObject()) {
            root = std::move(*object);
            return;
        }
        // Replaced by the next change, as a file of another program would be
        qWarning().noquote() << "The settings in" << fileName
                             << "cannot be read:" << QString::fromStdString(error.message());
    }

    const json::Value &AppSettings::Impl::value(std::string_view key) const {
        static const json::Value null;
        const json::Object *object = &root;
        for (;;) {
            const auto slash = key.find('/');
            const auto it = object->find(key.substr(0, slash));
            if (it == object->end()) {
                return null;
            }
            if (slash == std::string_view::npos) {
                return it->second;
            }
            object = it->second.asObject();
            if (!object) {
                return null;
            }
            key = key.substr(slash + 1);
        }
    }

    void AppSettings::Impl::setValue(std::string_view key, json::Value value) {
        insertAt(root, key, std::move(value));
        save();
    }

    void AppSettings::Impl::save() const {
        QDir().mkpath(QFileInfo(fileName).absolutePath());
        const auto text = json::Value(root).toJson(4);
        QSaveFile file(fileName);
        if (!file.open(QIODevice::WriteOnly) || file.write(text.data(), qint64(text.size())) < 0 ||
            !file.commit()) {
            qWarning().noquote() << "The settings cannot be written to" << fileName << ":"
                                 << file.errorString();
        }
    }

    AppSettings::AppSettings() : AppSettings(defaultFileName()) {
    }

    AppSettings::AppSettings(const QString &fileName) : _impl(std::make_unique<Impl>(fileName)) {
    }

    AppSettings::~AppSettings() = default;

    QString AppSettings::defaultFileName() {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
               QStringLiteral("/settings.json");
    }

    QString AppSettings::fileName() const {
        stdc_impl_t;
        return impl.fileName;
    }

    std::filesystem::path AppSettings::utauDirectory() const {
        stdc_impl_t;
        return std::filesystem::path(
            textOf(impl.value(KeyUtauDirectory).toString()).toStdU16String());
    }

    void AppSettings::setUtauDirectory(const std::filesystem::path &directory) {
        stdc_impl_t;
        impl.setValue(KeyUtauDirectory, textOf(directory).toStdString());
    }

    QString AppSettings::resampler() const {
        stdc_impl_t;
        return textOf(impl.value(KeyResampler).toString());
    }

    void AppSettings::setResampler(const QString &path) {
        stdc_impl_t;
        impl.setValue(KeyResampler, path.toStdString());
    }

    QString AppSettings::wavtool() const {
        stdc_impl_t;
        return textOf(impl.value(KeyWavtool).toString());
    }

    void AppSettings::setWavtool(const QString &path) {
        stdc_impl_t;
        impl.setValue(KeyWavtool, path.toStdString());
    }

    AppSettings::PlaybackMode AppSettings::playbackMode() const {
        stdc_impl_t;
        return impl.value(KeyPlaybackMode).toString() == RealtimeValue ? Realtime : Prerender;
    }

    void AppSettings::setPlaybackMode(PlaybackMode mode) {
        stdc_impl_t;
        impl.setValue(KeyPlaybackMode,
                      std::string(mode == Realtime ? RealtimeValue : PrerenderValue));
    }

    QString AppSettings::ustExportCharset() const {
        stdc_impl_t;
        const auto charset = impl.value(KeyUstExportCharset).asString();
        return charset ? textOf(*charset) : QStringLiteral("UTF-8");
    }

    void AppSettings::setUstExportCharset(const QString &charset) {
        stdc_impl_t;
        impl.setValue(KeyUstExportCharset, charset.toStdString());
    }

    QStringList AppSettings::recentCommands() const {
        stdc_impl_t;
        return stringsOf(impl.value(KeyRecentCommands));
    }

    void AppSettings::addRecentCommand(const QString &id) {
        stdc_impl_t;
        auto ids = recentCommands();
        ids.removeAll(id);
        ids.prepend(id);
        impl.setValue(KeyRecentCommands, arrayOf(ids.mid(0, recentCommandCount)));
    }

    QList<std::filesystem::path> AppSettings::recentFiles() const {
        stdc_impl_t;
        QList<std::filesystem::path> paths;
        for (const auto &text : stringsOf(impl.value(KeyRecentFiles))) {
            paths.push_back(std::filesystem::path(text.toStdU16String()));
        }
        return paths;
    }

    void AppSettings::addRecentFile(const std::filesystem::path &path) {
        stdc_impl_t;
        auto texts = stringsOf(impl.value(KeyRecentFiles));
        texts.removeAll(textOf(path));
        texts.prepend(textOf(path));
        impl.setValue(KeyRecentFiles, arrayOf(texts.mid(0, recentFileCount)));
    }

    void AppSettings::removeRecentFile(const std::filesystem::path &path) {
        stdc_impl_t;
        auto texts = stringsOf(impl.value(KeyRecentFiles));
        texts.removeAll(textOf(path));
        impl.setValue(KeyRecentFiles, arrayOf(texts));
    }

    void AppSettings::clearRecentFiles() {
        stdc_impl_t;
        impl.setValue(KeyRecentFiles, {});
    }

    QList<std::filesystem::path> AppSettings::recentVoiceBanks() const {
        stdc_impl_t;
        QList<std::filesystem::path> paths;
        for (const auto &text : stringsOf(impl.value(KeyRecentVoiceBanks))) {
            paths.push_back(std::filesystem::path(text.toStdU16String()));
        }
        return paths;
    }

    void AppSettings::addRecentVoiceBank(const std::filesystem::path &root) {
        stdc_impl_t;
        auto texts = stringsOf(impl.value(KeyRecentVoiceBanks));
        texts.removeAll(textOf(root));
        texts.prepend(textOf(root));
        impl.setValue(KeyRecentVoiceBanks, arrayOf(texts.mid(0, recentFileCount)));
    }

    void AppSettings::removeRecentVoiceBank(const std::filesystem::path &root) {
        stdc_impl_t;
        auto texts = stringsOf(impl.value(KeyRecentVoiceBanks));
        texts.removeAll(textOf(root));
        impl.setValue(KeyRecentVoiceBanks, arrayOf(texts));
    }

    void AppSettings::clearRecentVoiceBanks() {
        stdc_impl_t;
        impl.setValue(KeyRecentVoiceBanks, {});
    }

    QJsonValue AppSettings::value(const QString &key) const {
        stdc_impl_t;
        const auto path = key.toStdString();
        const auto &value = impl.value(path);
        // A value that is absent reads as null, which a present null does not come apart from:
        // setValue() never writes one.
        return value.isNull() ? QJsonValue(QJsonValue::Undefined) : qtOf(value);
    }

    void AppSettings::setValue(const QString &key, const QJsonValue &value) {
        stdc_impl_t;
        impl.setValue(key.toStdString(), stdcOf(value));
    }

    QString AppSettings::pluginKey(const QString &id) {
        return QLatin1String(KeyPluginData) + id;
    }

}
