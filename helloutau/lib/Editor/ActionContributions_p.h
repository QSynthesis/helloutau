#ifndef HELLOUTAU_EDITOR_ACTIONCONTRIBUTIONS_P_H
#define HELLOUTAU_EDITOR_ACTIONCONTRIBUTIONS_P_H

#include <QtCore/QList>

#include <helloutau/Editor/ActionContribution.h>

namespace QAK {
    class ActionExtension;
    class WidgetActionContext;
}

namespace hello::daw {

    /// The registered action contributions of the process, in the order of registration, and
    /// the editors that apply them.
    class ActionContributions {
    public:
        /// Told of each contribution registered or unregistered after it was added.
        class Listener {
        public:
            virtual ~Listener() = default;

            virtual void contributionAdded(ActionContribution *contribution) = 0;
            virtual void contributionRemoved(ActionContribution *contribution) = 0;
        };

        static ActionContributions &instance();

        QList<ActionContribution *> contributions() const;

        void add(ActionContribution *contribution);
        void remove(ActionContribution *contribution);

        void addListener(Listener *listener);
        void removeListener(Listener *listener);

        /// Adds the actions of every contribution to \a context of \a window, as a window does
        /// when it is created.
        template <class Window>
        void addActions(Window *window, QAK::WidgetActionContext *context) const {
            for (const auto contribution : m_contributions) {
                contribution->addActions(window, context);
            }
        }

        /// Removes from \a context the actions of the items of \a extension, and deletes them.
        static void removeActions(const QAK::ActionExtension *extension,
                                  QAK::WidgetActionContext *context);

    private:
        QList<ActionContribution *> m_contributions;
        QList<Listener *> m_listeners;
    };

}

#endif // HELLOUTAU_EDITOR_ACTIONCONTRIBUTIONS_P_H
