#include "InterchangeRegistration.h"
#include "InterchangeRegistrations_p.h"

namespace hello::kit {

    InterchangeRegistration::InterchangeRegistration(std::unique_ptr<InterchangeReader> reader)
        : m_reader(std::move(reader)) {
        InterchangeRegistrations::instance().add(this);
    }

    InterchangeRegistration::InterchangeRegistration(std::unique_ptr<InterchangeWriter> writer)
        : m_writer(std::move(writer)) {
        InterchangeRegistrations::instance().add(this);
    }

    InterchangeRegistration::~InterchangeRegistration() {
        InterchangeRegistrations::instance().remove(this);
    }

    InterchangeReader *InterchangeRegistration::reader() const {
        return m_reader.get();
    }

    InterchangeWriter *InterchangeRegistration::writer() const {
        return m_writer.get();
    }

    InterchangeRegistrations &InterchangeRegistrations::instance() {
        static InterchangeRegistrations registrations;
        return registrations;
    }

    QList<InterchangeRegistration *> InterchangeRegistrations::registrations() const {
        return m_registrations;
    }

    void InterchangeRegistrations::add(InterchangeRegistration *registration) {
        m_registrations.push_back(registration);
        // Iterates over a copy because a listener may remove itself during the notification.
        for (const auto listener : QList<Listener *>(m_listeners)) {
            listener->registrationAdded(registration);
        }
    }

    void InterchangeRegistrations::remove(InterchangeRegistration *registration) {
        if (!m_registrations.removeOne(registration)) {
            return;
        }
        for (const auto listener : QList<Listener *>(m_listeners)) {
            listener->registrationRemoved(registration);
        }
    }

    void InterchangeRegistrations::addListener(Listener *listener) {
        m_listeners.push_back(listener);
    }

    void InterchangeRegistrations::removeListener(Listener *listener) {
        m_listeners.removeOne(listener);
    }

}
