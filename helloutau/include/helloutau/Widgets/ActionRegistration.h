#ifndef HELLOUTAU_WIDGETS_ACTIONREGISTRATION_H
#define HELLOUTAU_WIDGETS_ACTIONREGISTRATION_H

#include <memory>

#include <QtCore/QtGlobal>

#include <helloutau/Widgets/ActionContribution.h>
#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

namespace hello::daw {

    /// Registers a contribution with every host, such as each editor, for the lifetime of the
    /// registration. Each host adds the extension and the actions of the contribution to the
    /// open windows and to windows opened later, and removes them when the registration is
    /// destroyed. See docs/Plugins.md.
    ///
    /// A plugin creates its registrations in initialize() and destroys them in
    /// aboutToShutdown(), before its library is unloaded. Registrations and hosts are used only
    /// on the application thread.
    class HELLOUTAU_WIDGETS_EXPORT ActionRegistration {
    public:
        explicit ActionRegistration(std::unique_ptr<ActionContribution> contribution);
        ~ActionRegistration();

        ActionContribution *contribution() const;

    private:
        std::unique_ptr<ActionContribution> m_contribution;

        Q_DISABLE_COPY_MOVE(ActionRegistration)
    };

}

#endif // HELLOUTAU_WIDGETS_ACTIONREGISTRATION_H
