#include "ViewSettings.h"

#include <algorithm>

#include "AppSettings_p.h"

namespace hello::daw {

    namespace json = stdc::json;

    namespace {

        constexpr char KeyQuantization[] = "view/quantization";
        constexpr char KeyPitchVisible[] = "view/showPitch";
        constexpr char KeyRenderedPitchVisible[] = "view/showRenderedPitch";
        constexpr char KeyEnvelopesVisible[] = "view/showEnvelopes";
        constexpr char KeyParametersVisible[] = "view/showParameters";
        constexpr char KeyToolBarVisible[] = "view/showToolBar";
        constexpr char KeyPlayheadFollow[] = "view/playheadFollow";
        constexpr char PageFollowValue[] = "page";
        constexpr char SmoothFollowValue[] = "smooth";

    }

    ViewSettings::ViewSettings(AppSettingsStore &store) : m_store(store) {
    }

    ViewSettings::~ViewSettings() = default;

    int ViewSettings::quantization() const {
        return int(m_store.value(KeyQuantization).toInt(120));
    }

    void ViewSettings::setQuantization(int ticks) {
        m_store.setValue(KeyQuantization, int64_t(std::max(0, ticks)));
    }

    bool ViewSettings::isPitchVisible() const {
        return m_store.value(KeyPitchVisible).toBool(true);
    }

    void ViewSettings::setPitchVisible(bool visible) {
        m_store.setValue(KeyPitchVisible, visible);
    }

    bool ViewSettings::isRenderedPitchVisible() const {
        return m_store.value(KeyRenderedPitchVisible).toBool(true);
    }

    void ViewSettings::setRenderedPitchVisible(bool visible) {
        m_store.setValue(KeyRenderedPitchVisible, visible);
    }

    bool ViewSettings::areEnvelopesVisible() const {
        return m_store.value(KeyEnvelopesVisible).toBool(true);
    }

    void ViewSettings::setEnvelopesVisible(bool visible) {
        m_store.setValue(KeyEnvelopesVisible, visible);
    }

    bool ViewSettings::areParametersVisible() const {
        return m_store.value(KeyParametersVisible).toBool(false);
    }

    void ViewSettings::setParametersVisible(bool visible) {
        m_store.setValue(KeyParametersVisible, visible);
    }

    bool ViewSettings::isToolBarVisible() const {
        return m_store.value(KeyToolBarVisible).toBool(true);
    }

    void ViewSettings::setToolBarVisible(bool visible) {
        m_store.setValue(KeyToolBarVisible, visible);
    }

    ViewSettings::PlayheadFollow ViewSettings::playheadFollow() const {
        return m_store.value(KeyPlayheadFollow).toString() == SmoothFollowValue ? SmoothFollow
                                                                                : PageFollow;
    }

    void ViewSettings::setPlayheadFollow(PlayheadFollow follow) {
        m_store.setValue(
            KeyPlayheadFollow,
            json::Value(std::string(follow == SmoothFollow ? SmoothFollowValue : PageFollowValue)));
    }

}
