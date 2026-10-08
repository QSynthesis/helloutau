#ifndef HELLOUTAU_WIDGETS_ACTIONCONTRIBUTION_H
#define HELLOUTAU_WIDGETS_ACTIONCONTRIBUTION_H

#include <QtCore/QString>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

class QWidget;

namespace QAK {
    class ActionExtension;
    class WidgetActionContext;
}

namespace hello::daw {

    /// Actions that a plugin adds to the windows of a host, such as the editor. A contribution
    /// consists of action extensions compiled by AEC, at most one for each kind of window, which
    /// declare the items and insert them into the menus of the host, and of the actions that
    /// implement the items in each window. See the editor extensions in docs/Plugins.md.
    ///
    /// A kind of window is identified by the name that the host defines for it, such as
    /// Editor::projectWindowName.
    class HELLOUTAU_WIDGETS_EXPORT ActionContribution {
    public:
        virtual ~ActionContribution();

        /// Returns the extension that the host registers with the action registry of the
        /// windows of the kind \a windowKind, or null if the contribution adds nothing to them.
        /// Each kind of window has a registry of its own.
        virtual const QAK::ActionExtension *extension(const QString &windowKind) const = 0;

        /// Adds to \a context, the action context of \a window, an action parented to \a window
        /// for each item of extension() that the window supports. The host removes and deletes
        /// these actions when the registration of the contribution is destroyed. The default
        /// implementation adds no action.
        ///
        /// \a window is a window of any kind of the host. A contribution that requires the
        /// interface of one kind casts \a window with \c qobject_cast and ignores the other
        /// kinds.
        ///
        /// For an item without an action in a menu of the window, the context shows a
        /// placeholder that has no effect.
        ///
        /// The action of an external item is the \c menuAction() of a menu parented to
        /// \a window, whose content the contribution maintains. In this case the host deletes
        /// the menu, which deletes the action.
        virtual void addActions(QWidget *window, QAK::WidgetActionContext *context);

        /// Removes the actions of the items of \a extension from \a context and deletes them. A
        /// host calls this function for each window when a contribution is removed.
        static void removeActions(const QAK::ActionExtension *extension,
                                  QAK::WidgetActionContext *context);
    };

}

#endif // HELLOUTAU_WIDGETS_ACTIONCONTRIBUTION_H
