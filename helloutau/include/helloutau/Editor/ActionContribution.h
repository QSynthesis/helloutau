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

}

#endif // HELLOUTAU_EDITOR_ACTIONCONTRIBUTION_H
