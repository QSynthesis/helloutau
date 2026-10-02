#include "AppSettings.h"
#include "AppSettings_p.h"

#include <algorithm>

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <stdcorelib/pimpl.h>

#include "SettingsJson_p.h"

namespace hello::daw {

    namespace json = stdc::json;

    namespace {

        constexpr char KeyUtauDirectory[] = "engines/utauDirectory";
        constexpr char KeyResampler[] = "engines/resampler";
        constexpr char KeyWavtool[] = "engines/wavtool";
        constexpr char KeyRenderLogAccumulated[] = "renderLog/accumulated";
        constexpr char KeyRenderLogLimit[] = "renderLog/limit";
        constexpr char KeyAudioOutputDevice[] = "audio/outputDevice";
        constexpr char KeyPlaybackMode[] = "playback/mode";
        constexpr char PrerenderValue[] = "prerender";
        constexpr char ThreadedValue[] = "threaded";
        constexpr char RealtimeValue[] = "realtime";
        constexpr char KeyRenderThreads[] = "playback/threads";
        constexpr char KeyQuantization[] = "view/quantization";
        constexpr char KeyUstExportCharset[] = "files/ustExportCharset";
        constexpr char KeyLanguage[] = "appearance/language";
        constexpr char KeyPitchVisible[] = "view/showPitch";
        constexpr char KeyRenderedPitchVisible[] = "view/showRenderedPitch";
        constexpr char KeyEnvelopesVisible[] = "view/showEnvelopes";
        constexpr char KeyParametersVisible[] = "view/showParameters";
        constexpr char KeyToolBarVisible[] = "view/showToolBar";
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

        QString recentTextOf(const std::filesystem::path &path) {
            return QDir::toNativeSeparators(
                QString::fromStdU16String(path.lexically_normal().u16string()));
        }

        QString normalizedPathText(const QString &text) {
            if (text.isEmpty()) {
                return {};
            }
            return recentTextOf(std::filesystem::path(text.toStdU16String()));
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

        QStringList recentTexts(const json::Value &value) {
            QStringList result;
            for (const auto &text : stringsOf(value)) {
                const auto normalized = recentTextOf(std::filesystem::path(text.toStdU16String()));
                if (!normalized.isEmpty() && !result.contains(normalized)) {
                    result.push_back(normalized);
                }
            }
            return result;
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
        const auto normalizeRecent = [this](std::string_view key) {
            const auto old = stringsOf(_impl->value(key));
            const auto normalized = recentTexts(_impl->value(key));
            if (old != normalized) {
                _impl->setValue(key, arrayOf(normalized));
            }
        };
        normalizeRecent(KeyRecentFiles);
        normalizeRecent(KeyRecentVoiceBanks);
        const auto normalizePath = [this](std::string_view key) {
            const auto old = textOf(_impl->value(key).toString());
            const auto normalized = normalizedPathText(old);
            if (old != normalized) {
                _impl->setValue(key, normalized.toStdString());
            }
        };
        normalizePath(KeyUtauDirectory);
        normalizePath(KeyResampler);
        normalizePath(KeyWavtool);
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
        impl.setValue(KeyUtauDirectory, recentTextOf(directory).toStdString());
    }

    QString AppSettings::resampler() const {
        stdc_impl_t;
        return textOf(impl.value(KeyResampler).toString());
    }

    void AppSettings::setResampler(const QString &path) {
        stdc_impl_t;
        impl.setValue(KeyResampler, normalizedPathText(path).toStdString());
    }

    QString AppSettings::wavtool() const {
        stdc_impl_t;
        return textOf(impl.value(KeyWavtool).toString());
    }

    void AppSettings::setWavtool(const QString &path) {
        stdc_impl_t;
        impl.setValue(KeyWavtool, normalizedPathText(path).toStdString());
    }

    bool AppSettings::isRenderLogAccumulated() const {
        stdc_impl_t;
        return impl.value(KeyRenderLogAccumulated).toBool(true);
    }

    void AppSettings::setRenderLogAccumulated(bool accumulated) {
        stdc_impl_t;
        impl.setValue(KeyRenderLogAccumulated, accumulated);
    }

    int AppSettings::renderLogLimit() const {
        stdc_impl_t;
        return std::max(1024, int(impl.value(KeyRenderLogLimit).toInt(1024 * 1024)));
    }

    void AppSettings::setRenderLogLimit(int bytes) {
        stdc_impl_t;
        impl.setValue(KeyRenderLogLimit, int64_t(std::max(1024, bytes)));
    }

    QByteArray AppSettings::audioOutputDevice() const {
        stdc_impl_t;
        return QByteArray::fromBase64(QByteArray::fromStdString(impl.value(KeyAudioOutputDevice).toString()));
    }

    void AppSettings::setAudioOutputDevice(const QByteArray &id) {
        stdc_impl_t;
        impl.setValue(KeyAudioOutputDevice, id.toBase64().toStdString());
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

    int AppSettings::quantization() const {
        stdc_impl_t;
        return int(impl.value(KeyQuantization).toInt(120));
    }

    void AppSettings::setQuantization(int ticks) {
        stdc_impl_t;
        impl.setValue(KeyQuantization, int64_t(std::max(0, ticks)));
    }

    bool AppSettings::isPitchVisible() const {
        stdc_impl_t;
        return impl.value(KeyPitchVisible).toBool(true);
    }

    void AppSettings::setPitchVisible(bool visible) {
        stdc_impl_t;
        impl.setValue(KeyPitchVisible, visible);
    }

    bool AppSettings::isRenderedPitchVisible() const {
        stdc_impl_t;
        return impl.value(KeyRenderedPitchVisible).toBool(true);
    }

    void AppSettings::setRenderedPitchVisible(bool visible) {
        stdc_impl_t;
        impl.setValue(KeyRenderedPitchVisible, visible);
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

    bool AppSettings::isToolBarVisible() const {
        stdc_impl_t;
        return impl.value(KeyToolBarVisible).toBool(true);
    }

    void AppSettings::setToolBarVisible(bool visible) {
        stdc_impl_t;
        impl.setValue(KeyToolBarVisible, visible);
    }

    QString AppSettings::language() const {
        stdc_impl_t;
        const auto language = impl.value(KeyLanguage).asString();
        return language ? textOf(*language) : QString();
    }

    void AppSettings::setLanguage(const QString &language) {
        stdc_impl_t;
        impl.setValue(KeyLanguage, language.toStdString());
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
        auto texts = recentTexts(impl.value(KeyRecentFiles));
        for (const auto &text : texts) {
            paths.push_back(std::filesystem::path(text.toStdU16String()));
        }
        return paths;
    }

    void AppSettings::addRecentFile(const std::filesystem::path &path) {
        stdc_impl_t;
        auto texts = recentTexts(impl.value(KeyRecentFiles));
        const auto normalized = recentTextOf(path);
        texts.removeAll(normalized);
        texts.prepend(normalized);
        impl.setValue(KeyRecentFiles, arrayOf(texts.mid(0, recentFileCount)));
    }

    void AppSettings::removeRecentFile(const std::filesystem::path &path) {
        stdc_impl_t;
        auto texts = recentTexts(impl.value(KeyRecentFiles));
        texts.removeAll(recentTextOf(path));
        impl.setValue(KeyRecentFiles, arrayOf(texts));
    }

    void AppSettings::clearRecentFiles() {
        stdc_impl_t;
        impl.setValue(KeyRecentFiles, {});
    }

    QList<std::filesystem::path> AppSettings::recentVoiceBanks() const {
        stdc_impl_t;
        QList<std::filesystem::path> paths;
        auto texts = recentTexts(impl.value(KeyRecentVoiceBanks));
        for (const auto &text : texts) {
            paths.push_back(std::filesystem::path(text.toStdU16String()));
        }
        return paths;
    }

    void AppSettings::addRecentVoiceBank(const std::filesystem::path &root) {
        stdc_impl_t;
        auto texts = recentTexts(impl.value(KeyRecentVoiceBanks));
        const auto normalized = recentTextOf(root);
        texts.removeAll(normalized);
        texts.prepend(normalized);
        impl.setValue(KeyRecentVoiceBanks, arrayOf(texts.mid(0, recentFileCount)));
    }

    void AppSettings::removeRecentVoiceBank(const std::filesystem::path &root) {
        stdc_impl_t;
        auto texts = recentTexts(impl.value(KeyRecentVoiceBanks));
        texts.removeAll(recentTextOf(root));
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
        if (key == QLatin1String(KeyRecentFiles) ||
            key == QLatin1String(KeyRecentVoiceBanks)) {
            impl.setValue(key.toStdString(), arrayOf(recentTexts(SettingsJson::stdcOf(value))));
            return;
        }
        if (key == QLatin1String(KeyUtauDirectory) || key == QLatin1String(KeyResampler) ||
            key == QLatin1String(KeyWavtool)) {
            impl.setValue(key.toStdString(),
                         normalizedPathText(value.toString()).toStdString());
            return;
        }
        impl.setValue(key.toStdString(), SettingsJson::stdcOf(value));
    }

}
