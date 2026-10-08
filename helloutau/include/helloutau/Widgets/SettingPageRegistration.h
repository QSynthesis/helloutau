#ifndef HELLOUTAU_WIDGETS_SETTINGPAGEREGISTRATION_H
#define HELLOUTAU_WIDGETS_SETTINGPAGEREGISTRATION_H

#include <functional>

#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QtGlobal>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>
#include <helloutau/Widgets/SettingPageRegistry.h>

class QObject;

namespace hello::daw {

    class SettingCatalog;
    class SettingPage;

    /// Adds a page to the setting catalog of a host, such as an editor, for the lifetime of the
    /// registration. The host creates the page when the registration is created and deletes the
    /// page when the registration is destroyed. See docs/Plugins.md.
    ///
    /// A plugin obtains the registry from the editor of AppLoader::editor(), creates its
    /// registrations in initialize() and destroys them in aboutToShutdown(), before its library
    /// is unloaded, as with ActionRegistration. A registration whose registry has been destroyed
    /// removes nothing. Registrations are used only on the application thread.
    class HELLOUTAU_WIDGETS_EXPORT SettingPageRegistration {
    public:
        /// Creates the page for \a host, the object that owns the catalog, such as an Editor. A
        /// page that requires the interface of the host casts \a host with \c qobject_cast. The
        /// catalog owns the page.
        using Factory = std::function<SettingPage *(QObject *host)>;

        /// \param parent the id of the page under which the page is placed, or empty for the
        ///        top level. The page is placed at the top level if no page has this id.
        /// \param before the id of the sibling before which the page is placed, or empty to
        ///        place the page last, as in SettingCatalog::addPage()
        SettingPageRegistration(SettingPageRegistry *registry, Factory factory,
                                const QString &parent = {}, const QString &before = {});
        ~SettingPageRegistration();

        /// Creates the page for \a host and adds it to \a catalog.
        SettingPage *addTo(SettingCatalog *catalog, QObject *host) const;

    private:
        QPointer<SettingPageRegistry> m_registry;
        Factory m_factory;
        QString m_parent;
        QString m_before;

        Q_DISABLE_COPY_MOVE(SettingPageRegistration)
    };

}

#endif // HELLOUTAU_WIDGETS_SETTINGPAGEREGISTRATION_H
