#ifndef HELLOUTAU_EDITOR_ACTIONCONTRIBUTION_H
#define HELLOUTAU_EDITOR_ACTIONCONTRIBUTION_H

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace QAK {
    class ActionExtension;
    class WidgetActionContext;
}

namespace hello::daw {

    class ProjectWindow;
    class VoiceBankWindow;

    /// Actions that a plugin adds to the windows of the editor. A contribution consists of an
    /// action extension compiled by AEC, which declares the items and inserts them into the menus
    /// of the editor, and of the actions that implement the items in each window. See the editor
    /// extensions in docs/Plugins.md.
    class HELLOUTAU_EDITOR_EXPORT ActionContribution {
    public:
        virtual ~ActionContribution();

        virtual const QAK::ActionExtension *extension() const = 0;

        /// Adds to \a context, the action context of \a window, an action parented to \a window
        /// for each item of extension() that the window supports. The editor removes and deletes
        /// these actions when the registration of the contribution is destroyed. The default
        /// implementation adds no action.
        ///
        /// For an item without an action in a menu of the window, the context shows a
        /// placeholder that has no effect.
        ///
        /// The action of an external item is the \c menuAction() of a menu parented to
        /// \a window, whose content the contribution maintains. In this case the editor deletes
        /// the menu, which deletes the action.
        virtual void addActions(ProjectWindow *window, QAK::WidgetActionContext *context);
        virtual void addActions(VoiceBankWindow *window, QAK::WidgetActionContext *context);
    };

}

#endif // HELLOUTAU_EDITOR_ACTIONCONTRIBUTION_H
