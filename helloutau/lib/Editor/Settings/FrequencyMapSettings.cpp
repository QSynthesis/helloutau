#include "FrequencyMapSettings.h"

#include <algorithm>

#include "AppSettings_p.h"
#include "FrequencyMapView.h"

namespace hello::daw {

    namespace json = stdc::json;

    namespace {

        constexpr char KeyThreads[] = "frequencyMaps/threads";
        constexpr char KeyRegeneration[] = "frequencyMaps/regeneration";
        constexpr char CachesOnlyValue[] = "cachesOnly";
        constexpr char KeyRemovesCachesOnSave[] = "frequencyMaps/removesCachesOnSave";
        constexpr char KeyFollowsRenderLog[] = "frequencyMaps/log/followsRenderLog";
        constexpr char KeyLogAccumulated[] = "frequencyMaps/log/accumulated";
        constexpr char KeyLogLimit[] = "frequencyMaps/log/limit";
        constexpr char KeySpectrogram[] = "frequencyMaps/editor/spectrogram";
        constexpr char KeyBrightness[] = "frequencyMaps/editor/brightness";
        constexpr char KeyColorScheme[] = "frequencyMaps/editor/colorScheme";
        constexpr char KeyCheckNote[] = "frequencyMaps/check/note";
        constexpr char KeyCheckPitchChange[] = "frequencyMaps/check/allowsPitchChange";
        constexpr char KeyCheckChangeAtStart[] = "frequencyMaps/check/allowsChangeAtStart";
        constexpr char KeyCheckChangeAtEnd[] = "frequencyMaps/check/allowsChangeAtEnd";

        // The notes of the check, C1 to B7
        constexpr int lowestCheckNote = 24;
        constexpr int highestCheckNote = 107;
        constexpr int defaultCheckNote = 60;

    }

    FrequencyMapSettings::FrequencyMapSettings(AppSettingsStore &store) : m_store(store) {
    }

    FrequencyMapSettings::~FrequencyMapSettings() = default;

    int FrequencyMapSettings::threadCount() const {
        return std::max(0, int(m_store.value(KeyThreads).toInt(0)));
    }

    void FrequencyMapSettings::setThreadCount(int count) {
        // The default is not written, so that the file keeps no value that was never chosen.
        m_store.setValue(KeyThreads, count > 0 ? json::Value(int64_t(count)) : json::Value());
    }

    FrequencyMapSettings::Regeneration FrequencyMapSettings::regeneration() const {
        return m_store.value(KeyRegeneration).toString() == CachesOnlyValue ? CachesOnly
                                                                            : MapsAndCaches;
    }

    void FrequencyMapSettings::setRegeneration(Regeneration regeneration) {
        m_store.setValue(KeyRegeneration, regeneration == CachesOnly
                                              ? json::Value(std::string(CachesOnlyValue))
                                              : json::Value());
    }

    bool FrequencyMapSettings::removesCachesOnSave() const {
        return m_store.value(KeyRemovesCachesOnSave).toBool(true);
    }

    void FrequencyMapSettings::setRemovesCachesOnSave(bool removes) {
        m_store.setValue(KeyRemovesCachesOnSave, removes ? json::Value() : json::Value(false));
    }

    bool FrequencyMapSettings::showsSpectrogram() const {
        return m_store.value(KeySpectrogram).toBool(false);
    }

    void FrequencyMapSettings::setShowsSpectrogram(bool shows) {
        m_store.setValue(KeySpectrogram, shows ? json::Value(true) : json::Value());
    }

    int FrequencyMapSettings::spectrogramBrightness() const {
        return std::clamp(int(m_store.value(KeyBrightness).toInt(0)),
                          FrequencyMapView::lowestBrightness, FrequencyMapView::highestBrightness);
    }

    void FrequencyMapSettings::setSpectrogramBrightness(int brightness) {
        brightness = std::clamp(brightness, FrequencyMapView::lowestBrightness,
                                FrequencyMapView::highestBrightness);
        m_store.setValue(KeyBrightness,
                         brightness != 0 ? json::Value(int64_t(brightness)) : json::Value());
    }

    int FrequencyMapSettings::spectrogramColorScheme() const {
        return std::clamp(int(m_store.value(KeyColorScheme).toInt(0)), 0,
                          FrequencyMapView::colorSchemeCount - 1);
    }

    void FrequencyMapSettings::setSpectrogramColorScheme(int scheme) {
        scheme = std::clamp(scheme, 0, FrequencyMapView::colorSchemeCount - 1);
        m_store.setValue(KeyColorScheme,
                         scheme != 0 ? json::Value(int64_t(scheme)) : json::Value());
    }

    kit::FrequencyMapCheck::Settings FrequencyMapSettings::checkSettings() const {
        kit::FrequencyMapCheck::Settings settings;
        settings.note = std::clamp(int(m_store.value(KeyCheckNote).toInt(defaultCheckNote)),
                                   lowestCheckNote, highestCheckNote);
        settings.allowsPitchChange = m_store.value(KeyCheckPitchChange).toBool(false);
        settings.allowsChangeAtStart = m_store.value(KeyCheckChangeAtStart).toBool(false);
        settings.allowsChangeAtEnd = m_store.value(KeyCheckChangeAtEnd).toBool(false);
        return settings;
    }

    void FrequencyMapSettings::setCheckSettings(const kit::FrequencyMapCheck::Settings &settings) {
        const int note = std::clamp(settings.note, lowestCheckNote, highestCheckNote);
        m_store.setValue(KeyCheckNote,
                         note != defaultCheckNote ? json::Value(int64_t(note)) : json::Value());
        const auto flag = [](bool on) { return on ? json::Value(true) : json::Value(); };
        m_store.setValue(KeyCheckPitchChange, flag(settings.allowsPitchChange));
        m_store.setValue(KeyCheckChangeAtStart, flag(settings.allowsChangeAtStart));
        m_store.setValue(KeyCheckChangeAtEnd, flag(settings.allowsChangeAtEnd));
    }

    bool FrequencyMapSettings::followsRenderLog() const {
        return m_store.value(KeyFollowsRenderLog).toBool(true);
    }

    void FrequencyMapSettings::setFollowsRenderLog(bool follows) {
        m_store.setValue(KeyFollowsRenderLog, follows ? json::Value() : json::Value(false));
    }

    bool FrequencyMapSettings::isLogAccumulated() const {
        return m_store.value(KeyLogAccumulated).toBool(true);
    }

    void FrequencyMapSettings::setLogAccumulated(bool accumulated) {
        m_store.setValue(KeyLogAccumulated, accumulated);
    }

    int FrequencyMapSettings::logLimit() const {
        return std::max(1024, int(m_store.value(KeyLogLimit).toInt(1024 * 1024)));
    }

    void FrequencyMapSettings::setLogLimit(int bytes) {
        m_store.setValue(KeyLogLimit, int64_t(std::max(1024, bytes)));
    }

}
