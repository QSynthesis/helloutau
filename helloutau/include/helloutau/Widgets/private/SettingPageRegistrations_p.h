#ifndef HELLOUTAU_WIDGETS_SETTINGPAGEREGISTRATIONS_P_H
#define HELLOUTAU_WIDGETS_SETTINGPAGEREGISTRATIONS_P_H

#include <QtCore/QList>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>
#include <helloutau/Widgets/SettingPageRegistration.h>

namespace hello::daw {

    /// Registry of the setting page registrations of the process, in the order of registration,
    /// and of the hosts that apply them. The interface for a host, such as the editor, and not
    /// for a plugin, which uses SettingPageRegistration.
    class HELLOUTAU_WIDGETS_EXPORT SettingPageRegistrations {
    public:
        /// Receives a notification of each registration made or destroyed after the listener
        /// was added.
        class Listener {
        public:
            virtual ~Listener() = default;

            virtual void registrationAdded(const SettingPageRegistration *registration) = 0;
            virtual void registrationRemoved(const SettingPageRegistration *registration) = 0;
        };

        static SettingPageRegistrations &instance();

        QList<const SettingPageRegistration *> registrations() const;

        void add(const SettingPageRegistration *registration);
        void remove(const SettingPageRegistration *registration);

        void addListener(Listener *listener);
        void removeListener(Listener *listener);

    private:
        QList<const SettingPageRegistration *> m_registrations;
        QList<Listener *> m_listeners;
    };

}

#endif // HELLOUTAU_WIDGETS_SETTINGPAGEREGISTRATIONS_P_H
