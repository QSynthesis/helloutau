#include "CoreSettingPages.h"

#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/EditorSettingPageIds.h>
#include <helloutau/Widgets/SettingPage.h>

#include "KeymapSettingPage.h"
#include "MenusSettingPage.h"

namespace hello::daw {

    void addCoreSettingPages(Editor *editor) {
        const auto catalog = editor->settingCatalog();
        if (const auto appearance =
                catalog->page(QLatin1String(EditorSettingPageIds::appearanceAndBehavior))) {
            appearance->addPage(new MenusSettingPage(editor),
                                QLatin1String(EditorSettingPageIds::systemSettings));
        }
        catalog->addPage(new KeymapSettingPage(editor),
                         QLatin1String(EditorSettingPageIds::editor));
    }

}
