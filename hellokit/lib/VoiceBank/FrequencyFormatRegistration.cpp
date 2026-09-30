#include "FrequencyFormatRegistration.h"
#include "FrequencyFormatRegistrations_p.h"

namespace hello::kit {

    FrequencyFormatRegistration::FrequencyFormatRegistration(
        std::unique_ptr<FrequencyFormat> format)
        : m_format(std::move(format)) {
        FrequencyFormatRegistrations::instance().add(m_format.get());
    }

    FrequencyFormatRegistration::~FrequencyFormatRegistration() {
        FrequencyFormatRegistrations::instance().remove(m_format.get());
    }

    FrequencyFormat *FrequencyFormatRegistration::format() const {
        return m_format.get();
    }

    FrequencyFormatRegistrations &FrequencyFormatRegistrations::instance() {
        static FrequencyFormatRegistrations registrations;
        return registrations;
    }

    QList<FrequencyFormat *> FrequencyFormatRegistrations::formats() const {
        return m_formats;
    }

    void FrequencyFormatRegistrations::add(FrequencyFormat *format) {
        m_formats.push_back(format);
        // Iterates over a copy because a listener may remove itself during the notification.
        for (const auto listener : QList<Listener *>(m_listeners)) {
            listener->formatAdded(format);
        }
    }

    void FrequencyFormatRegistrations::remove(FrequencyFormat *format) {
        if (!m_formats.removeOne(format)) {
            return;
        }
        for (const auto listener : QList<Listener *>(m_listeners)) {
            listener->formatRemoved(format);
        }
    }

    void FrequencyFormatRegistrations::addListener(Listener *listener) {
        m_listeners.push_back(listener);
    }

    void FrequencyFormatRegistrations::removeListener(Listener *listener) {
        m_listeners.removeOne(listener);
    }

}
