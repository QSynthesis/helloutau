#ifndef HELLOUTAU_INTERCHANGE_INTERCHANGESTEPREGISTRATION_H
#define HELLOUTAU_INTERCHANGE_INTERCHANGESTEPREGISTRATION_H

#include <functional>

#include <QtCore/QString>

#include <Interchange/InterchangePluginGlobal.h>

namespace hello::daw {

    class InterchangeStepPage;

    /// Registration of a custom step page under the ID that an import driver returns from
    /// \c customStepId(). The page is available to every InterchangeStepRegistry for the
    /// lifetime of the registration. See docs/Plugins.md.
    ///
    /// A plugin creates its registrations in initialize() and destroys them in
    /// aboutToShutdown(), before its library is unloaded. Registrations are used on the
    /// application thread only.
    class INTERCHANGEPLUGIN_EXPORT InterchangeStepRegistration {
    public:
        /// Creates a page without a parent. The caller owns the page.
        using Factory = std::function<InterchangeStepPage *()>;

        InterchangeStepRegistration(const QString &id, Factory factory);
        ~InterchangeStepRegistration();

        QString id() const;

        /// Returns a new page from the factory.
        InterchangeStepPage *create() const;

    private:
        QString m_id;
        Factory m_factory;

        Q_DISABLE_COPY_MOVE(InterchangeStepRegistration)
    };

}

#endif // HELLOUTAU_INTERCHANGE_INTERCHANGESTEPREGISTRATION_H
