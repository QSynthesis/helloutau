#ifndef HELLOUTAU_EDITOR_SETTINGPAGEREGISTRATION_H
#define HELLOUTAU_EDITOR_SETTINGPAGEREGISTRATION_H

#include <functional>

#include <QtCore/QString>
#include <QtCore/QtGlobal>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    class Editor;
    class SettingPage;

    /// Adds a page to the setting catalog of every editor for the lifetime of the registration.
    /// Each editor creates the page when the editor or the registration is created, whichever
    /// is later, and deletes the page when the registration is destroyed. See docs/Plugins.md.
    ///
    /// A plugin creates its registrations in initialize() and destroys them in
    /// aboutToShutdown(), before its library is unloaded, as with ActionRegistration.
    /// Registrations and editors are used only on the application thread.
    class HELLOUTAU_EDITOR_EXPORT SettingPageRegistration {
    public:
        /// Creates the page for \a editor. The catalog of \a editor owns the page.
        using Factory = std::function<SettingPage *(Editor *editor)>;

        /// \param parent the id of the page under which the page is placed, or empty for the
        ///        top level. The page is placed at the top level if no page has this id.
        /// \param before the id of the sibling before which the page is placed, or empty to
        ///        place the page last, as in SettingCatalog::addPage()
        explicit SettingPageRegistration(Factory factory, const QString &parent = {},
                                         const QString &before = {});
        ~SettingPageRegistration();

        /// Creates the page for \a editor and adds it to the catalog of \a editor.
        SettingPage *addTo(Editor *editor) const;

    private:
        Factory m_factory;
        QString m_parent;
        QString m_before;

        Q_DISABLE_COPY_MOVE(SettingPageRegistration)
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGPAGEREGISTRATION_H
