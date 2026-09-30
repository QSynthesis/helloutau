#include "ActionContribution.h"

#include <QtCore/QtGlobal>

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

}
