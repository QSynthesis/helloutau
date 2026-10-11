#include "AppearanceAndBehaviorCategory.h"

namespace hello::daw {

    AppearanceAndBehaviorCategory::AppearanceAndBehaviorCategory(QObject *parent)
        : SettingPage(QLatin1String(pageId), parent) {
        setTitle(tr("Appearance & Behavior"));
        setKeywords({QStringLiteral("Appearance & Behavior")});
    }

    AppearanceAndBehaviorCategory::~AppearanceAndBehaviorCategory() = default;

}
