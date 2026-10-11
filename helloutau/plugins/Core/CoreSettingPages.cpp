#include "CoreSettingPages.h"

#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/SettingPages/AppearanceAndBehaviorCategory.h>
#include <helloutau/Editor/SettingPages/EditorSettingPage.h>
#include <helloutau/Editor/SettingPages/RenderingSettingPage.h>
#include <helloutau/Editor/SettingPages/SystemSettingsPage.h>
#include <helloutau/Widgets/SettingPage.h>

#include "KeymapSettingPage.h"
#include "MenusSettingPage.h"
#include "PluginSettingPage.h"

namespace hello::daw {

    void addCoreSettingPages(Editor *editor, AppLoader *loader) {
        const auto catalog = editor->settingCatalog();
        if (const auto appearance =
                catalog->page(QLatin1String(AppearanceAndBehaviorCategory::pageId))) {
            appearance->addPage(new MenusSettingPage(editor),
                                QLatin1String(SystemSettingsPage::pageId));
        }
        catalog->addPage(new KeymapSettingPage(editor), QLatin1String(EditorSettingPage::pageId));
        // Plugins precedes Rendering as Plugins precedes the build settings in JetBrains IDEs.
        if (loader) {
            catalog->addPage(new PluginSettingPage(*loader),
                             QLatin1String(RenderingSettingPage::pageId));
        }
    }

}
