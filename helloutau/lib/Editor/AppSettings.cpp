#include "AppSettings.h"
#include "AppSettings_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QStandardPaths>
#include <QtCore/QtDebug>

#include <stdcorelib/pimpl.h>

#include <hellokit/Support/JsonInterop.h>
#include <hellokit/Support/SettingsFile.h>

namespace hello::daw {

    namespace json = stdc::json;

    namespace {

        // Returns the settings in fileName, or no settings if the file cannot be read.
        json::Object readSettings(const QString &fileName) {
            kit::DiagnosticList diagnostics;
            auto object = kit::SettingsFile::read(fileName, diagnostics);
            // TODO: Show the diagnostics in a list of the errors of the application, which does
            // not exist yet.
            for (const auto &diagnostic : std::as_const(diagnostics)) {
                qWarning().noquote() << diagnostic.message;
            }
            return object ? std::move(*object) : json::Object();
        }

        bool isRecentListKey(std::string_view key) {
            return key == AppSettingsKeys::recentFiles || key == AppSettingsKeys::recentVoiceBanks;
        }

        bool isPathKey(std::string_view key) {
            return key == AppSettingsKeys::utauDirectory || key == AppSettingsKeys::resampler ||
                   key == AppSettingsKeys::wavtool;
        }

    }

    AppSettingsStore::AppSettingsStore(const QString &fileName)
        : root(readSettings(fileName)), file(fileName, [this] { return json::Value(root); }) {
        for (const std::string_view key :
             {AppSettingsKeys::recentFiles, AppSettingsKeys::recentVoiceBanks}) {
            const auto normalized = recentTextsOf(value(key));
            if (stringsOf(value(key)) != normalized) {
                setValue(key, arrayOf(normalized));
            }
        }
        for (const std::string_view key : {AppSettingsKeys::utauDirectory,
                                           AppSettingsKeys::resampler, AppSettingsKeys::wavtool}) {
            const auto old = text(key);
            const auto normalized = normalizedPathText(old);
            if (old != normalized) {
                setValue(key, normalized.toStdString());
            }
        }
    }

    const json::Value &AppSettingsStore::value(std::string_view key) const {
        return kit::JsonInterop::valueAt(root, key);
    }

    void AppSettingsStore::setValue(std::string_view key, json::Value value) {
        kit::JsonInterop::insertAt(root, key, std::move(value));
        file.syncLater();
    }

    QString AppSettingsStore::text(std::string_view key) const {
        return QString::fromStdString(value(key).toString());
    }

    QString AppSettingsStore::pathText(const std::filesystem::path &path) {
        return QDir::toNativeSeparators(
            QString::fromStdU16String(path.lexically_normal().u16string()));
    }

    QString AppSettingsStore::normalizedPathText(const QString &text) {
        if (text.isEmpty()) {
            return {};
        }
        return pathText(std::filesystem::path(text.toStdU16String()));
    }

    QStringList AppSettingsStore::stringsOf(const json::Value &value) {
        QStringList strings;
        for (const auto &item : value.toArray()) {
            strings.push_back(QString::fromStdString(item.toString()));
        }
        return strings;
    }

    json::Value AppSettingsStore::arrayOf(const QStringList &strings) {
        json::Array array;
        array.reserve(size_t(strings.size()));
        for (const auto &string : strings) {
            array.emplace_back(string.toStdString());
        }
        return json::Value(std::move(array));
    }

    QStringList AppSettingsStore::recentTextsOf(const json::Value &value) {
        QStringList result;
        for (const auto &text : stringsOf(value)) {
            const auto normalized = pathText(std::filesystem::path(text.toStdU16String()));
            if (!normalized.isEmpty() && !result.contains(normalized)) {
                result.push_back(normalized);
            }
        }
        return result;
    }

    AppSettings::AppSettings() : AppSettings(defaultFileName(), defaultUserDirectory()) {
    }

    AppSettings::AppSettings(const QString &fileName)
        : AppSettings(fileName, QFileInfo(fileName).absolutePath()) {
    }

    AppSettings::AppSettings(const QString &fileName, const QString &userDirectory)
        : _impl(std::make_unique<Impl>(fileName)) {
        stdc_impl_t;
        impl.userDirectory = std::filesystem::path(userDirectory.toStdU16String());
        // The constructors of the groups are private, and only this class constructs them.
        impl.utau.reset(new UtauSettings(impl.store));
        impl.voiceBanks.reset(new VoiceBankSettings(*this, impl.store));
        impl.rendering.reset(new RenderingSettings(impl.store));
        impl.frequencyMaps.reset(new FrequencyMapSettings(impl.store));
        impl.audio.reset(new AudioSettings(impl.store));
        impl.appearance.reset(new AppearanceSettings(impl.store));
        impl.view.reset(new ViewSettings(impl.store));
        impl.pitchControl.reset(new PitchControlSettings(impl.store));
        impl.files.reset(new FileSettings(impl.store));
        impl.commandPalette.reset(new CommandPaletteSettings(impl.store));
        impl.updates.reset(new UpdateSettings(impl.store));
    }

    AppSettings::~AppSettings() = default;

    QString AppSettings::defaultDirectory() {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }

    QString AppSettings::defaultFileName() {
        return defaultDirectory() + QStringLiteral("/settings.json");
    }

    QString AppSettings::defaultUserDirectory() {
        return QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
                               QLatin1Char('/') + QCoreApplication::organizationName() +
                               QLatin1Char('/') + QCoreApplication::applicationName());
    }

    QString AppSettings::fileName() const {
        stdc_impl_t;
        return impl.store.file.fileName();
    }

    std::filesystem::path AppSettings::userDirectory() const {
        stdc_impl_t;
        return impl.userDirectory;
    }

    void AppSettings::sync() {
        stdc_impl_t;
        impl.store.file.sync();
    }

    UtauSettings &AppSettings::utau() {
        stdc_impl_t;
        return *impl.utau;
    }

    const UtauSettings &AppSettings::utau() const {
        stdc_impl_t;
        return *impl.utau;
    }

    VoiceBankSettings &AppSettings::voiceBanks() {
        stdc_impl_t;
        return *impl.voiceBanks;
    }

    const VoiceBankSettings &AppSettings::voiceBanks() const {
        stdc_impl_t;
        return *impl.voiceBanks;
    }

    RenderingSettings &AppSettings::rendering() {
        stdc_impl_t;
        return *impl.rendering;
    }

    const RenderingSettings &AppSettings::rendering() const {
        stdc_impl_t;
        return *impl.rendering;
    }

    FrequencyMapSettings &AppSettings::frequencyMaps() {
        stdc_impl_t;
        return *impl.frequencyMaps;
    }

    const FrequencyMapSettings &AppSettings::frequencyMaps() const {
        stdc_impl_t;
        return *impl.frequencyMaps;
    }

    AudioSettings &AppSettings::audio() {
        stdc_impl_t;
        return *impl.audio;
    }

    const AudioSettings &AppSettings::audio() const {
        stdc_impl_t;
        return *impl.audio;
    }

    AppearanceSettings &AppSettings::appearance() {
        stdc_impl_t;
        return *impl.appearance;
    }

    const AppearanceSettings &AppSettings::appearance() const {
        stdc_impl_t;
        return *impl.appearance;
    }

    ViewSettings &AppSettings::view() {
        stdc_impl_t;
        return *impl.view;
    }

    const ViewSettings &AppSettings::view() const {
        stdc_impl_t;
        return *impl.view;
    }

    PitchControlSettings &AppSettings::pitchControl() {
        stdc_impl_t;
        return *impl.pitchControl;
    }

    const PitchControlSettings &AppSettings::pitchControl() const {
        stdc_impl_t;
        return *impl.pitchControl;
    }

    FileSettings &AppSettings::files() {
        stdc_impl_t;
        return *impl.files;
    }

    const FileSettings &AppSettings::files() const {
        stdc_impl_t;
        return *impl.files;
    }

    CommandPaletteSettings &AppSettings::commandPalette() {
        stdc_impl_t;
        return *impl.commandPalette;
    }

    const CommandPaletteSettings &AppSettings::commandPalette() const {
        stdc_impl_t;
        return *impl.commandPalette;
    }

    UpdateSettings &AppSettings::updates() {
        stdc_impl_t;
        return *impl.updates;
    }

    const UpdateSettings &AppSettings::updates() const {
        stdc_impl_t;
        return *impl.updates;
    }

    QJsonValue AppSettings::value(const QString &key) const {
        stdc_impl_t;
        const auto path = key.toStdString();
        const auto &value = impl.store.value(path);
        // An absent value reads as null and is indistinguishable from a stored null. setValue()
        // never stores null.
        return value.isNull() ? QJsonValue(QJsonValue::Undefined)
                              : kit::JsonInterop::toQtJson(value);
    }

    void AppSettings::setValue(const QString &key, const QJsonValue &value) {
        stdc_impl_t;
        const auto path = key.toStdString();
        if (isRecentListKey(path)) {
            impl.store.setValue(path, AppSettingsStore::arrayOf(AppSettingsStore::recentTextsOf(
                                          kit::JsonInterop::fromQtJson(value))));
            return;
        }
        if (isPathKey(path)) {
            impl.store.setValue(
                path, AppSettingsStore::normalizedPathText(value.toString()).toStdString());
            return;
        }
        impl.store.setValue(path, kit::JsonInterop::fromQtJson(value));
    }

}
