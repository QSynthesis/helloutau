#include "ActionContribution.h"

#include <QtCore/QPointer>
#include <QtCore/QtGlobal>
#include <QtGui/QAction>
#include <QtWidgets/QMenu>

#include <QAKCore/actionextension.h>
#include <QAKWidgets/widgetactioncontext.h>

namespace hello::daw {

    ActionContribution::~ActionContribution() = default;

    void ActionContribution::addActions(QWidget *window, QAK::WidgetActionContext *context) {
        Q_UNUSED(window);
        Q_UNUSED(context);
    }

    void ActionContribution::removeActions(const QAK::ActionExtension *extension,
                                           QAK::WidgetActionContext *context) {
        for (int i = 0; i < extension->itemCount(); ++i) {
            const auto id = extension->item(i).id();
            // A context deletes an owned action on removal.
            const QPointer<QAction> action = context->action(id);
            if (!action) {
                continue;
            }
            context->remove(id);
            // The action of an item that stands for a menu is the menuAction() of the menu and is
            // owned by the menu. Deleting the menu deletes the action.
            const auto menu = qobject_cast<QMenu *>(action->parent());
            if (menu && menu->menuAction() == action) {
                delete menu;
            } else {
                delete action.data();
            }
        }
    }

}
