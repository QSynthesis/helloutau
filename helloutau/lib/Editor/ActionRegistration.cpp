#include "ActionRegistration.h"
#include "ActionRegistrations_p.h"

#include <QtCore/QPointer>
#include <QtGui/QAction>

#include <QAKCore/actionextension.h>
#include <QAKWidgets/widgetactioncontext.h>

namespace hello::daw {

    ActionRegistration::ActionRegistration(std::unique_ptr<ActionContribution> contribution)
        : m_contribution(std::move(contribution)) {
        ActionRegistrations::instance().add(m_contribution.get());
    }

    ActionRegistration::~ActionRegistration() {
        ActionRegistrations::instance().remove(m_contribution.get());
    }

    ActionContribution *ActionRegistration::contribution() const {
        return m_contribution.get();
    }

    ActionRegistrations &ActionRegistrations::instance() {
        static ActionRegistrations registrations;
        return registrations;
    }

    QList<ActionContribution *> ActionRegistrations::contributions() const {
        return m_contributions;
    }

    void ActionRegistrations::add(ActionContribution *contribution) {
        m_contributions.push_back(contribution);
        // A copy, since a listener may remove itself
        for (const auto listener : QList<Listener *>(m_listeners)) {
            listener->contributionAdded(contribution);
        }
    }

    void ActionRegistrations::remove(ActionContribution *contribution) {
        if (!m_contributions.removeOne(contribution)) {
            return;
        }
        for (const auto listener : QList<Listener *>(m_listeners)) {
            listener->contributionRemoved(contribution);
        }
    }

    void ActionRegistrations::addListener(Listener *listener) {
        m_listeners.push_back(listener);
    }

    void ActionRegistrations::removeListener(Listener *listener) {
        m_listeners.removeOne(listener);
    }

    void ActionRegistrations::removeActions(const QAK::ActionExtension *extension,
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
