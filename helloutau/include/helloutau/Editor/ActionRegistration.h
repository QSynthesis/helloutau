#ifndef HELLOUTAU_EDITOR_ACTIONREGISTRATION_H
#define HELLOUTAU_EDITOR_ACTIONREGISTRATION_H

#include <memory>

#include <QtCore/QtGlobal>

#include <helloutau/Editor/ActionContribution.h>
#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// Registers a contribution with every editor for the lifetime of the registration. Each
    /// editor adds the extension and the actions of the contribution to the open windows and to
    /// windows opened later, and removes them when the registration is destroyed. See
    /// docs/Plugins.md.
    ///
    /// A plugin creates its registrations in initialize() and destroys them in
    /// aboutToShutdown(), before its library is unloaded. Registrations and editors are used
    /// only on the application thread.
    class HELLOUTAU_EDITOR_EXPORT ActionRegistration {
    public:
        explicit ActionRegistration(std::unique_ptr<ActionContribution> contribution);
        ~ActionRegistration();

        ActionContribution *contribution() const;

    private:
        std::unique_ptr<ActionContribution> m_contribution;

        Q_DISABLE_COPY_MOVE(ActionRegistration)
    };

}

#endif // HELLOUTAU_EDITOR_ACTIONREGISTRATION_H
