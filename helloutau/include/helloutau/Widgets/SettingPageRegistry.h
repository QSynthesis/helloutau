#ifndef HELLOUTAU_WIDGETS_SETTINGPAGEREGISTRY_H
#define HELLOUTAU_WIDGETS_SETTINGPAGEREGISTRY_H

#include <QtCore/QList>
#include <QtCore/QObject>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

namespace hello::daw {

    class SettingPageRegistration;

    /// The setting page registrations of a host, such as an editor, in the order of
    /// registration.
    ///
    /// The host creates and holds the registry, adds a page to its setting catalog for each
    /// registration, and follows the changes through registrationAdded() and
    /// registrationRemoved(). A SettingPageRegistration adds itself on construction and removes
    /// itself on destruction. The registry is used only on the application thread.
    class HELLOUTAU_WIDGETS_EXPORT SettingPageRegistry : public QObject {
        Q_OBJECT
    public:
        explicit SettingPageRegistry(QObject *parent = nullptr);
        ~SettingPageRegistry() override;

        QList<const SettingPageRegistration *> registrations() const;

        /// Appends \a registration and emits registrationAdded().
        void add(const SettingPageRegistration *registration);

        /// Removes \a registration and emits registrationRemoved(). Does nothing if the registry
        /// does not contain \a registration.
        void remove(const SettingPageRegistration *registration);

    Q_SIGNALS:
        void registrationAdded(const hello::daw::SettingPageRegistration *registration);
        void registrationRemoved(const hello::daw::SettingPageRegistration *registration);

    private:
        QList<const SettingPageRegistration *> m_registrations;
    };

}

#endif // HELLOUTAU_WIDGETS_SETTINGPAGEREGISTRY_H
