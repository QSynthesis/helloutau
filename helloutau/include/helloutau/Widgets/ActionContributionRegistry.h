#ifndef HELLOUTAU_WIDGETS_ACTIONCONTRIBUTIONREGISTRY_H
#define HELLOUTAU_WIDGETS_ACTIONCONTRIBUTIONREGISTRY_H

#include <QtCore/QList>
#include <QtCore/QObject>

#include <helloutau/Widgets/ActionContribution.h>
#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

namespace QAK {
    class ActionExtension;
    class WidgetActionContext;
}

namespace hello::daw {

    /// The action contributions of a host, such as an editor, in the order of registration.
    ///
    /// The host creates and holds the registry, applies each contribution to its windows, and
    /// follows the changes through contributionAdded() and contributionRemoved(). A plugin
    /// registers a contribution with an ActionRegistration rather than with add(). The registry
    /// is used only on the application thread.
    class HELLOUTAU_WIDGETS_EXPORT ActionContributionRegistry : public QObject {
        Q_OBJECT
    public:
        explicit ActionContributionRegistry(QObject *parent = nullptr);
        ~ActionContributionRegistry() override;

        QList<ActionContribution *> contributions() const;

        /// Appends \a contribution, which the caller continues to own, and emits
        /// contributionAdded().
        void add(ActionContribution *contribution);

        /// Removes \a contribution and emits contributionRemoved(). Does nothing if the registry
        /// does not contain \a contribution.
        void remove(ActionContribution *contribution);

        /// Adds the actions of every contribution to \a context of \a window. A window calls this
        /// function when it is created.
        void addActions(QWidget *window, QAK::WidgetActionContext *context) const;

        /// Removes the actions of the items of \a extension from \a context and deletes them.
        static void removeActions(const QAK::ActionExtension *extension,
                                  QAK::WidgetActionContext *context);

    Q_SIGNALS:
        void contributionAdded(hello::daw::ActionContribution *contribution);
        void contributionRemoved(hello::daw::ActionContribution *contribution);

    private:
        QList<ActionContribution *> m_contributions;
    };

}

#endif // HELLOUTAU_WIDGETS_ACTIONCONTRIBUTIONREGISTRY_H
