#include "ActionIconToggle.h"

#include <QtGui/QAction>

#include <helloutau/Theme/ThemeIcon.h>

namespace hello::daw {

    ActionIconToggle::ActionIconToggle(QAction *action)
        : QObject(action), m_action(action), m_icon(action->icon()), m_shownKey(m_icon.cacheKey()) {
        // QAction::changed() also reports changes other than the icon. A key other than the
        // one assigned here identifies an icon assigned by others.
        connect(action, &QAction::changed, this, [this] {
            if (!m_assigning && m_action->icon().cacheKey() != m_shownKey) {
                m_icon = m_action->icon();
                show();
            }
        });
    }

    ActionIconToggle::~ActionIconToggle() = default;

    bool ActionIconToggle::isToggled() const {
        return m_toggled;
    }

    void ActionIconToggle::setToggled(bool toggled) {
        if (toggled == m_toggled) {
            return;
        }
        m_toggled = toggled;
        show();
    }

    void ActionIconToggle::show() {
        m_assigning = true;
        m_action->setIcon(m_toggled ? ThemeIcon::checkedLook(m_icon) : m_icon);
        m_assigning = false;
        m_shownKey = m_action->icon().cacheKey();
    }

}
