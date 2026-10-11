#include "EditorSettingPages_p.h"

#include <helloutau/Widgets/SettingPage.h>

#include "AppearanceAndBehaviorCategory.h"
#include "AudioSettingPage.h"
#include "EditorSettingPage.h"
#include "RenderingSettingPage.h"
#include "SystemSettingsPage.h"
#include "UpdateSettingPage.h"
#include "UtauSettingPage.h"

namespace hello::daw {

    void addEditorSettingPages(SettingCatalog *catalog, AppSettings &settings,
                               UpdateChecker &checker) {
        const auto appearance = new AppearanceAndBehaviorCategory();
        const auto system = new SystemSettingsPage(settings);
        system->addPage(new UpdateSettingPage(settings, checker));
        appearance->addPage(system);
        catalog->addPage(appearance);
        catalog->addPage(new EditorSettingPage(settings));
        catalog->addPage(new UtauSettingPage(settings));
        catalog->addPage(new AudioSettingPage(settings));
        catalog->addPage(new RenderingSettingPage(settings));
    }

}
