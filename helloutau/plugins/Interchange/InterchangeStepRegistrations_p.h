#ifndef HELLOUTAU_INTERCHANGE_INTERCHANGESTEPREGISTRATIONS_P_H
#define HELLOUTAU_INTERCHANGE_INTERCHANGESTEPREGISTRATIONS_P_H

#include <QtCore/QList>

namespace hello::daw {

    class InterchangeStepRegistration;

    /// The process-wide list of custom step registrations in the order of registration.
    ///
    /// No listener is required because the import wizard looks up a page each time the page is
    /// shown.
    class InterchangeStepRegistrations {
    public:
        static InterchangeStepRegistrations &instance();

        QList<InterchangeStepRegistration *> registrations() const;

        void add(InterchangeStepRegistration *registration);
        void remove(InterchangeStepRegistration *registration);

    private:
        QList<InterchangeStepRegistration *> m_registrations;
    };

}

#endif // HELLOUTAU_INTERCHANGE_INTERCHANGESTEPREGISTRATIONS_P_H
