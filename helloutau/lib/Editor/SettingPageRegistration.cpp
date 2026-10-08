#include "SettingPageRegistration.h"
#include "SettingPageRegistrations_p.h"

#include <utility>

#include <helloutau/Widgets/SettingPage.h>

#include "Editor.h"

namespace hello::daw {

    SettingPageRegistration::SettingPageRegistration(Factory factory, const QString &parent,
                                                     const QString &before)
        : m_factory(std::move(factory)), m_parent(parent), m_before(before) {
        SettingPageRegistrations::instance().add(this);
    }

    SettingPageRegistration::~SettingPageRegistration() {
        SettingPageRegistrations::instance().remove(this);
    }

    SettingPage *SettingPageRegistration::addTo(Editor *editor) const {
        const auto page = m_factory(editor);
        if (!page) {
            return nullptr;
        }
        const auto catalog = editor->settingCatalog();
        if (const auto parent = m_parent.isEmpty() ? nullptr : catalog->page(m_parent)) {
            parent->addPage(page, m_before);
        } else {
            catalog->addPage(page, m_before);
        }
        return page;
    }

    SettingPageRegistrations &SettingPageRegistrations::instance() {
        static SettingPageRegistrations registrations;
        return registrations;
    }

    QList<const SettingPageRegistration *> SettingPageRegistrations::registrations() const {
        return m_registrations;
    }

    void SettingPageRegistrations::add(const SettingPageRegistration *registration) {
        m_registrations.push_back(registration);
        // Iterates over a copy because a listener may remove itself
        for (const auto listener : QList<Listener *>(m_listeners)) {
            listener->registrationAdded(registration);
        }
    }

    void SettingPageRegistrations::remove(const SettingPageRegistration *registration) {
        if (!m_registrations.removeOne(registration)) {
            return;
        }
        for (const auto listener : QList<Listener *>(m_listeners)) {
            listener->registrationRemoved(registration);
        }
    }

    void SettingPageRegistrations::addListener(Listener *listener) {
        m_listeners.push_back(listener);
    }

    void SettingPageRegistrations::removeListener(Listener *listener) {
        m_listeners.removeOne(listener);
    }

}
