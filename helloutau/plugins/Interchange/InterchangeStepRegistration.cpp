#include "InterchangeStepRegistration.h"

#include "InterchangeStepPage.h"
#include "InterchangeStepRegistrations_p.h"
#include "InterchangeStepRegistry.h"

namespace hello::daw {

    InterchangeStepPage::InterchangeStepPage(QWidget *parent) : QWidget(parent) {
    }

    InterchangeStepPage::~InterchangeStepPage() = default;

    bool InterchangeStepPage::isComplete() const {
        return true;
    }

    InterchangeStepRegistration::InterchangeStepRegistration(const QString &id, Factory factory)
        : m_id(id), m_factory(std::move(factory)) {
        InterchangeStepRegistrations::instance().add(this);
    }

    InterchangeStepRegistration::~InterchangeStepRegistration() {
        InterchangeStepRegistrations::instance().remove(this);
    }

    QString InterchangeStepRegistration::id() const {
        return m_id;
    }

    InterchangeStepPage *InterchangeStepRegistration::create() const {
        return m_factory ? m_factory() : nullptr;
    }

    InterchangeStepRegistrations &InterchangeStepRegistrations::instance() {
        static InterchangeStepRegistrations registrations;
        return registrations;
    }

    QList<InterchangeStepRegistration *> InterchangeStepRegistrations::registrations() const {
        return m_registrations;
    }

    void InterchangeStepRegistrations::add(InterchangeStepRegistration *registration) {
        m_registrations.push_back(registration);
    }

    void InterchangeStepRegistrations::remove(InterchangeStepRegistration *registration) {
        m_registrations.removeOne(registration);
    }

    bool InterchangeStepRegistry::contains(const QString &id) {
        const auto registrations = InterchangeStepRegistrations::instance().registrations();
        return std::any_of(registrations.begin(), registrations.end(),
                           [&id](const auto registration) { return registration->id() == id; });
    }

    InterchangeStepPage *InterchangeStepRegistry::create(const QString &id, QWidget *parent) {
        for (const auto registration : InterchangeStepRegistrations::instance().registrations()) {
            if (registration->id() == id) {
                const auto page = registration->create();
                if (page && parent) {
                    page->setParent(parent);
                }
                return page;
            }
        }
        return nullptr;
    }

}
