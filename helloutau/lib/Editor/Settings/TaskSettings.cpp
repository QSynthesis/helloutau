#include "TaskSettings.h"

#include <algorithm>

#include "AppSettings_p.h"

namespace hello::daw {

    namespace json = stdc::json;

    namespace {

        constexpr char keyThreads[] = "tasks/threads";

    }

    TaskSettings::TaskSettings(AppSettingsStore &store) : m_store(store) {
    }

    TaskSettings::~TaskSettings() = default;

    int TaskSettings::threadCount() const {
        return std::max(0, int(m_store.value(keyThreads).toInt(0)));
    }

    void TaskSettings::setThreadCount(int count) {
        // The default is not written, so that the file keeps no value that was never chosen.
        m_store.setValue(keyThreads, count > 0 ? json::Value(int64_t(count)) : json::Value());
    }

}
