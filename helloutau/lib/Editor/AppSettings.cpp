#include "AppSettings.h"
#include "AppSettings_p.h"

#include <algorithm>

#include <QtCore/QStandardPaths>

#include <stdcorelib/pimpl.h>

#include "SettingsJson_p.h"

namespace hello::daw {

    namespace json = stdc::json;

    namespace {

        constexpr char KeyUtauDirectory[] = "engines/utauDirectory";
        constexpr char KeyResampler[] = "engines/resampler";
        constexpr char KeyWavtool[] = "engines/wavtool";
        constexpr char KeyPlaybackMode[] = "playback/mode";
        constexpr char PrerenderValue[] = "prerender";
        constexpr char ThreadedValue[] = "threaded";
        constexpr char RealtimeValue[] = "realtime";
        constexpr char KeyRenderThreads[] = "playback/threads";
        constexpr char KeyUstExportCharset[] = "files/ustExportCharset";
        constexpr char KeyPitchVisible[] = "view/showPitch";
        constexpr char KeyEnvelopesVisible[] = "view/showEnvelopes";
        constexpr char KeyParametersVisible[] = "view/showParameters";
        constexpr char KeyRecentCommands[] = "commandPalette/recent";
        constexpr char KeyRecentFiles[] = "files/recent";
        constexpr char KeyRecentVoiceBanks[] = "files/recentVoiceBanks";

        QString textOf(const std::string &utf8) {
            return QString::fromStdString(utf8);
        }

        // A path in the form stored in the settings, converted without loss
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

    }

    AppSettings::Impl::Impl(const QString &fileName)
        : root(SettingsJson::read(fileName)), file(fileName, [this] { return json::Value(root); }) {
    }

    const json::Value &AppSettings::Impl::value(std::string_view key) const {
        return SettingsJson::valueAt(root, key);
    }

    void AppSettings::Impl::setValue(std::string_view key, json::Value value) {
        SettingsJson::insertAt(root, key, std::move(value));
        file.changed();
    }

    AppSettings::AppSettings() : AppSettings(defaultFileName()) {
    }

    AppSettings::AppSettings(const QString &fileName) : _impl(std::make_unique<Impl>(fileName)) {
    }

    AppSettings::~AppSettings() = default;

    QString AppSettings::defaultDirectory() {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }

    QString AppSettings::defaultFileName() {
        return defaultDirectory() + QStringLiteral("/settings.json");
    }

    QString AppSettings::fileName() const {
        stdc_impl_t;
        return impl.file.fileName();
    }

    void AppSettings::sync() {
        stdc_impl_t;
        impl.file.sync();
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
        const auto value = impl.value(KeyPlaybackMode).toString();
        if (value == RealtimeValue) {
            return Realtime;
        }
        return value == ThreadedValue ? ThreadedPrerender : Prerender;
    }

    void AppSettings::setPlaybackMode(PlaybackMode mode) {
        stdc_impl_t;
        const char *value = PrerenderValue;
        if (mode == ThreadedPrerender) {
            value = ThreadedValue;
        } else if (mode == Realtime) {
            value = RealtimeValue;
        }
        impl.setValue(KeyPlaybackMode, std::string(value));
    }

    int AppSettings::renderThreadCount() const {
        stdc_impl_t;
        return std::max(0, int(impl.value(KeyRenderThreads).toInt(0)));
    }

    void AppSettings::setRenderThreadCount(int count) {
        stdc_impl_t;
        // The default is not written, so that the file keeps no value that was never chosen.
        impl.setValue(KeyRenderThreads, count > 0 ? json::Value(int64_t(count)) : json::Value());
    }

    bool AppSettings::isPitchVisible() const {
        stdc_impl_t;
        return impl.value(KeyPitchVisible).toBool(true);
    }

    void AppSettings::setPitchVisible(bool visible) {
        stdc_impl_t;
        impl.setValue(KeyPitchVisible, visible);
    }

    bool AppSettings::areEnvelopesVisible() const {
        stdc_impl_t;
        return impl.value(KeyEnvelopesVisible).toBool(true);
    }

    void AppSettings::setEnvelopesVisible(bool visible) {
        stdc_impl_t;
        impl.setValue(KeyEnvelopesVisible, visible);
    }

    bool AppSettings::areParametersVisible() const {
        stdc_impl_t;
        return impl.value(KeyParametersVisible).toBool(false);
    }

    void AppSettings::setParametersVisible(bool visible) {
        stdc_impl_t;
        impl.setValue(KeyParametersVisible, visible);
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
        // An absent value reads as null and is indistinguishable from a stored null. setValue()
        // never stores null.
        return value.isNull() ? QJsonValue(QJsonValue::Undefined) : SettingsJson::qtOf(value);
    }

    void AppSettings::setValue(const QString &key, const QJsonValue &value) {
        stdc_impl_t;
        impl.setValue(key.toStdString(), SettingsJson::stdcOf(value));
    }

}
