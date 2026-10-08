#include "ActionContributionRegistry.h"

#include <QtCore/QPointer>
#include <QtGui/QAction>
#include <QtWidgets/QMenu>

#include <QAKCore/actionextension.h>
#include <QAKWidgets/widgetactioncontext.h>

namespace hello::daw {

    ActionContributionRegistry::ActionContributionRegistry(QObject *parent) : QObject(parent) {
    }

    ActionContributionRegistry::~ActionContributionRegistry() = default;

    QList<ActionContribution *> ActionContributionRegistry::contributions() const {
        return m_contributions;
    }

    void ActionContributionRegistry::add(ActionContribution *contribution) {
        m_contributions.push_back(contribution);
        Q_EMIT contributionAdded(contribution);
    }

    void ActionContributionRegistry::remove(ActionContribution *contribution) {
        if (m_contributions.removeOne(contribution)) {
            Q_EMIT contributionRemoved(contribution);
        }
    }

    void ActionContributionRegistry::addActions(QWidget *window,
                                                QAK::WidgetActionContext *context) const {
        for (const auto contribution : m_contributions) {
            contribution->addActions(window, context);
        }
    }

    void ActionContributionRegistry::removeActions(const QAK::ActionExtension *extension,
                                                   QAK::WidgetActionContext *context) {
        for (int i = 0; i < extension->itemCount(); ++i) {
            const auto id = extension->item(i).id();
            // A context deletes an owned action on removal.
            const QPointer<QAction> action = context->action(id);
            if (!action) {
                continue;
            }
            context->remove(id);
            // The action of an external item is the menuAction() of its menu and is owned by the
            // menu. Deleting the menu deletes the action.
            const auto menu = qobject_cast<QMenu *>(action->parent());
            if (menu && menu->menuAction() == action) {
                delete menu;
            } else {
                delete action.data();
            }
        }
    }

}
