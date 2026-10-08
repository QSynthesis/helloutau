#include "SettingPageRegistration.h"

#include <utility>

#include "SettingPage.h"

namespace hello::daw {

    SettingPageRegistration::SettingPageRegistration(SettingPageRegistry *registry, Factory factory,
                                                     const QString &parent, const QString &before)
        : m_registry(registry), m_factory(std::move(factory)), m_parent(parent), m_before(before) {
        if (m_registry) {
            m_registry->add(this);
        }
    }

    SettingPageRegistration::~SettingPageRegistration() {
        if (m_registry) {
            m_registry->remove(this);
        }
    }

    SettingPage *SettingPageRegistration::addTo(SettingCatalog *catalog, QObject *host) const {
        const auto page = m_factory(host);
        if (!page) {
            return nullptr;
        }
        if (const auto parent = m_parent.isEmpty() ? nullptr : catalog->page(m_parent)) {
            parent->addPage(page, m_before);
        } else {
            catalog->addPage(page, m_before);
        }
        return page;
    }

}
