#ifndef HELLOKIT_INTERCHANGE_INTERCHANGEREGISTRATIONS_P_H
#define HELLOKIT_INTERCHANGE_INTERCHANGEREGISTRATIONS_P_H

#include <QtCore/QList>

#include <hellokit/Interchange/InterchangeRegistration.h>

namespace hello::kit {

    /// The drivers registered in the process, in the order of registration, and the registries
    /// that follow them.
    ///
    /// Kept here rather than in a stdc::DynamicRegistry, whose entries are ordered by name,
    /// since the first of two drivers for a suffix takes precedence.
    class InterchangeRegistrations {
    public:
        /// Told of each registration added or removed after it was added.
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
