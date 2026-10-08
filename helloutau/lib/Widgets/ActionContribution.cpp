#include "ActionContribution.h"

#include <QtCore/QtGlobal>

namespace hello::daw {

    ActionContribution::~ActionContribution() = default;

    void ActionContribution::addActions(QWidget *window, QAK::WidgetActionContext *context) {
        Q_UNUSED(window);
        Q_UNUSED(context);
    }

}
