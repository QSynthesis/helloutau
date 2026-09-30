#include "ActionContribution.h"
#include "ActionContributions_p.h"

#include <QtCore/QPointer>
#include <QtGui/QAction>

#include <QAKCore/actionextension.h>
#include <QAKWidgets/widgetactioncontext.h>

namespace hello::daw {

    ActionContribution::~ActionContribution() = default;

    void ActionContribution::addActions(ProjectWindow *window, QAK::WidgetActionContext *context) {
        Q_UNUSED(window);
        Q_UNUSED(context);
    }

    void ActionContribution::addActions(VoiceBankWindow *window,
                                        QAK::WidgetActionContext *context) {
        Q_UNUSED(window);
        Q_UNUSED(context);
    }

    ActionRegistration::ActionRegistration(std::unique_ptr<ActionContribution> contribution)
        : m_contribution(std::move(contribution)) {
        ActionContributions::instance().add(m_contribution.get());
    }

    ActionRegistration::~ActionRegistration() {
        ActionContributions::instance().remove(m_contribution.get());
    }

    ActionContribution *ActionRegistration::contribution() const {
        return m_contribution.get();
    }

    ActionContributions &ActionContributions::instance() {
        static ActionContributions contributions;
        return contributions;
    }

    QList<ActionContribution *> ActionContributions::contributions() const {
        return m_contributions;
    }

    void ActionContributions::add(ActionContribution *contribution) {
        m_contributions.push_back(contribution);
        // A copy, since a listener may remove itself
        for (const auto listener : QList<Listener *>(m_listeners)) {
            listener->contributionAdded(contribution);
        }
    }

    void ActionContributions::remove(ActionContribution *contribution) {
        if (!m_contributions.removeOne(contribution)) {
            return;
        }
        for (const auto listener : QList<Listener *>(m_listeners)) {
            listener->contributionRemoved(contribution);
        }
    }

    void ActionContributions::addListener(Listener *listener) {
        m_listeners.push_back(listener);
    }

    void ActionContributions::removeListener(Listener *listener) {
        m_listeners.removeOne(listener);
    }

    void ActionContributions::removeActions(const QAK::ActionExtension *extension,
                                            QAK::WidgetActionContext *context) {
        for (int i = 0; i < extension->itemCount(); ++i) {
            const auto id = extension->item(i).id();
            // A context deletes an action that it owns as it removes it.
            const QPointer<QAction> action = context->action(id);
            if (!action) {
                continue;
            }
            context->remove(id);
            delete action.data();
        }
    }

}
