#ifndef HELLOUTAU_WIDGETS_ACTIONREGISTRATIONS_P_H
#define HELLOUTAU_WIDGETS_ACTIONREGISTRATIONS_P_H

#include <QtCore/QList>

#include <helloutau/Widgets/ActionContribution.h>
#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

namespace QAK {
    class ActionExtension;
    class WidgetActionContext;
}

namespace hello::daw {

    /// Registry of the action contributions of the process, in the order of registration, and of
    /// the hosts that apply them. The interface for a host, such as the editor, and not for a
    /// plugin, which uses ActionRegistration.
    class HELLOUTAU_WIDGETS_EXPORT ActionRegistrations {
    public:
        /// Receives a notification of each contribution registered or unregistered after the
        /// listener was added.
        class Listener {
        public:
            virtual ~Listener() = default;

            virtual void contributionAdded(ActionContribution *contribution) = 0;
            virtual void contributionRemoved(ActionContribution *contribution) = 0;
        };

        static ActionRegistrations &instance();

        QList<ActionContribution *> contributions() const;

        void add(ActionContribution *contribution);
        void remove(ActionContribution *contribution);

        void addListener(Listener *listener);
        void removeListener(Listener *listener);

        /// Adds the actions of every contribution to \a context of \a window. A window calls this
        /// function when it is created.
        void addActions(QWidget *window, QAK::WidgetActionContext *context) const;

        /// Removes the actions of the items of \a extension from \a context and deletes them.
        static void removeActions(const QAK::ActionExtension *extension,
                                  QAK::WidgetActionContext *context);

    private:
        QList<ActionContribution *> m_contributions;
        QList<Listener *> m_listeners;
    };

}

#endif // HELLOUTAU_WIDGETS_ACTIONREGISTRATIONS_P_H
