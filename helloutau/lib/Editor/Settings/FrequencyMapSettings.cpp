#include "FrequencyMapSettings.h"

#include <algorithm>

#include "AppSettings_p.h"

namespace hello::daw {

    namespace json = stdc::json;

    namespace {

        constexpr char KeyThreads[] = "frequencyMaps/threads";
        constexpr char KeyRegeneration[] = "frequencyMaps/regeneration";
        constexpr char CachesOnlyValue[] = "cachesOnly";

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

}
