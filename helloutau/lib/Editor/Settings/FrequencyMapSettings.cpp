#include "FrequencyMapSettings.h"

#include <algorithm>

#include "AppSettings_p.h"

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
