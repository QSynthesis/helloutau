#include "SettingPageRegistry.h"

namespace hello::daw {

    SettingPageRegistry::SettingPageRegistry(QObject *parent) : QObject(parent) {
    }

    SettingPageRegistry::~SettingPageRegistry() = default;

    QList<const SettingPageRegistration *> SettingPageRegistry::registrations() const {
        return m_registrations;
    }

    void SettingPageRegistry::add(const SettingPageRegistration *registration) {
        m_registrations.push_back(registration);
        Q_EMIT registrationAdded(registration);
    }

    void SettingPageRegistry::remove(const SettingPageRegistration *registration) {
        if (m_registrations.removeOne(registration)) {
            Q_EMIT registrationRemoved(registration);
        }
    }

}
