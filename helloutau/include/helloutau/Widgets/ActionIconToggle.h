#ifndef HELLOUTAU_WIDGETS_ACTIONICONTOGGLE_H
#define HELLOUTAU_WIDGETS_ACTIONICONTOGGLE_H

#include <QtCore/QObject>
#include <QtGui/QIcon>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

class QAction;

namespace hello::daw {

    /// Shows the checked files of the icon of an action while toggled, without making the
    /// action checkable, since a menu draws a check mark beside a checked action. See
    /// ThemeIcon::checkedLook().
    ///
    /// An icon assigned to the action later, as QActionKit assigns one after an update of the
    /// icons or layouts, replaces the untoggled icon, and its checked files are shown while
    /// toggled.
    class HELLOUTAU_WIDGETS_EXPORT ActionIconToggle : public QObject {
        Q_OBJECT
    public:
        /// Creates the toggle as a child of \a action, untoggled.
        explicit ActionIconToggle(QAction *action);
        ~ActionIconToggle();

        bool isToggled() const;
        void setToggled(bool toggled);

    private:
        QAction *m_action;
        bool m_toggled = false;
        // The icon assigned by others, and the cache key of the icon last assigned here
        QIcon m_icon;
        qint64 m_shownKey;
        bool m_assigning = false;

        void show();
    };

}

#endif // HELLOUTAU_WIDGETS_ACTIONICONTOGGLE_H
