#ifndef HELLOUTAU_EDITOR_ACTIONCONTRIBUTION_H
#define HELLOUTAU_EDITOR_ACTIONCONTRIBUTION_H

#include <memory>

#include <QtCore/QtGlobal>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace QAK {
    class ActionExtension;
    class WidgetActionContext;
}

namespace hello::daw {

    class ProjectWindow;
    class VoiceBankWindow;

    /// Actions that a plugin adds to the windows of the editor: an action extension compiled by
    /// AEC, which declares the items and inserts them into the menus of the editor, and the
    /// actions that implement the items in each window. See the editor extensions in
    /// docs/Plugins.md.
    class HELLOUTAU_EDITOR_EXPORT ActionContribution {
    public:
        virtual ~ActionContribution();

        virtual const QAK::ActionExtension *extension() const = 0;

        /// Adds to \a context, the actions of \a window, an action for each item of extension()
        /// that the window offers, parented to \a window. The editor removes and deletes them
        /// when the contribution goes. The default adds none.
        ///
        /// For an item in a menu of the window without an action, the context shows a stand-in
        /// that does nothing.
        virtual void addActions(ProjectWindow *window, QAK::WidgetActionContext *context);
        virtual void addActions(VoiceBankWindow *window, QAK::WidgetActionContext *context);
    };

    /// Registers a contribution with every editor while the registration exists: each editor
    /// adds its extension and its actions to the open windows and to those opened later, and
    /// removes them when the registration is destroyed. Used on the main thread only.
    ///
    /// A plugin creates its registrations in initialize() and destroys them in
    /// aboutToShutdown(), before its library is unloaded.
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

#endif // HELLOUTAU_EDITOR_ACTIONCONTRIBUTION_H
