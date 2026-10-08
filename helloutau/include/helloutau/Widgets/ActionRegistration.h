#ifndef HELLOUTAU_WIDGETS_ACTIONREGISTRATION_H
#define HELLOUTAU_WIDGETS_ACTIONREGISTRATION_H

#include <memory>

#include <QtCore/QPointer>
#include <QtCore/QtGlobal>

#include <helloutau/Widgets/ActionContribution.h>
#include <helloutau/Widgets/ActionContributionRegistry.h>
#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

namespace hello::daw {

    /// Registers a contribution with the registry of a host, such as an editor, for the lifetime
    /// of the registration. The host adds the extension and the actions of the contribution to
    /// the open windows and to windows opened later, and removes them when the registration is
    /// destroyed. See docs/Plugins.md.
    ///
    /// A plugin obtains the registry from the editor of AppLoader::editor(), creates its
    /// registrations in initialize() and destroys them in aboutToShutdown(), before its library
    /// is unloaded. A registration whose registry has been destroyed removes nothing.
    /// Registrations are used only on the application thread.
    class HELLOUTAU_WIDGETS_EXPORT ActionRegistration {
    public:
        ActionRegistration(ActionContributionRegistry *registry,
                           std::unique_ptr<ActionContribution> contribution);
        ~ActionRegistration();

        ActionContribution *contribution() const;

    private:
        QPointer<ActionContributionRegistry> m_registry;
        std::unique_ptr<ActionContribution> m_contribution;

        Q_DISABLE_COPY_MOVE(ActionRegistration)
    };

}

#endif // HELLOUTAU_WIDGETS_ACTIONREGISTRATION_H
