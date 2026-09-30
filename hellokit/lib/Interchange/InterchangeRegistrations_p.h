#ifndef HELLOKIT_INTERCHANGE_INTERCHANGEREGISTRATIONS_P_H
#define HELLOKIT_INTERCHANGE_INTERCHANGEREGISTRATIONS_P_H

#include <QtCore/QList>

#include <hellokit/Interchange/InterchangeRegistration.h>

namespace hello::kit {

    /// The process-wide list of registrations in the order of registration, and the listeners
    /// notified of changes.
    ///
    /// A stdc::DynamicRegistry is not used because it orders entries by name, and the
    /// precedence of drivers for a suffix depends on the order of registration.
    class InterchangeRegistrations {
    public:
        /// Receives a notification for each registration added or removed after the listener
        /// was added.
        class Listener {
        public:
            virtual ~Listener() = default;

            virtual void registrationAdded(InterchangeRegistration *registration) = 0;
            virtual void registrationRemoved(InterchangeRegistration *registration) = 0;
        };

        static InterchangeRegistrations &instance();

        QList<InterchangeRegistration *> registrations() const;

        void add(InterchangeRegistration *registration);
        void remove(InterchangeRegistration *registration);

        void addListener(Listener *listener);
        void removeListener(Listener *listener);

    private:
        QList<InterchangeRegistration *> m_registrations;
        QList<Listener *> m_listeners;
    };

}

#endif // HELLOKIT_INTERCHANGE_INTERCHANGEREGISTRATIONS_P_H
