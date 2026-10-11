#include "RenderingSettings.h"

#include <algorithm>

#include "AppSettings_p.h"

namespace hello::daw {

    namespace json = stdc::json;

    namespace {

        constexpr char KeyLogAccumulated[] = "renderLog/accumulated";
        constexpr char KeyLogLimit[] = "renderLog/limit";
        constexpr char KeyPlaybackMode[] = "playback/mode";
        constexpr char PrerenderValue[] = "prerender";
        constexpr char ThreadedValue[] = "threaded";
        constexpr char RealtimeValue[] = "realtime";
        constexpr char KeyThreads[] = "playback/threads";

    }

    RenderingSettings::RenderingSettings(AppSettingsStore &store) : m_store(store) {
    }

    RenderingSettings::~RenderingSettings() = default;

    QString RenderingSettings::resampler() const {
        return m_store.text(AppSettingsKeys::resampler);
    }

    void RenderingSettings::setResampler(const QString &path) {
        m_store.setValue(AppSettingsKeys::resampler,
                         AppSettingsStore::normalizedPathText(path).toStdString());
    }

    QString RenderingSettings::wavtool() const {
        return m_store.text(AppSettingsKeys::wavtool);
    }

    void RenderingSettings::setWavtool(const QString &path) {
        m_store.setValue(AppSettingsKeys::wavtool,
                         AppSettingsStore::normalizedPathText(path).toStdString());
    }

    bool RenderingSettings::isLogAccumulated() const {
        return m_store.value(KeyLogAccumulated).toBool(true);
    }

    void RenderingSettings::setLogAccumulated(bool accumulated) {
        m_store.setValue(KeyLogAccumulated, accumulated);
    }

    int RenderingSettings::logLimit() const {
        return std::max(1024, int(m_store.value(KeyLogLimit).toInt(1024 * 1024)));
    }

    void RenderingSettings::setLogLimit(int bytes) {
        m_store.setValue(KeyLogLimit, int64_t(std::max(1024, bytes)));
    }

    RenderingSettings::PlaybackMode RenderingSettings::playbackMode() const {
        const auto value = m_store.value(KeyPlaybackMode).toString();
        if (value == RealtimeValue) {
            return Realtime;
        }
        return value == ThreadedValue ? ThreadedPrerender : Prerender;
    }

    void RenderingSettings::setPlaybackMode(PlaybackMode mode) {
        const char *value = PrerenderValue;
        if (mode == ThreadedPrerender) {
            value = ThreadedValue;
        } else if (mode == Realtime) {
            value = RealtimeValue;
        }
        m_store.setValue(KeyPlaybackMode, std::string(value));
    }

    int RenderingSettings::threadCount() const {
        return std::max(0, int(m_store.value(KeyThreads).toInt(0)));
    }

    void RenderingSettings::setThreadCount(int count) {
        // The default is not written, so that the file keeps no value that was never chosen.
        m_store.setValue(KeyThreads, count > 0 ? json::Value(int64_t(count)) : json::Value());
    }

}
