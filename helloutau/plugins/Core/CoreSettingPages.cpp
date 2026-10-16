#include "CoreSettingPages.h"

#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/SettingPages/AppearanceAndBehaviorCategory.h>
#include <helloutau/Editor/SettingPages/EditorSettingPage.h>
#include <helloutau/Editor/SettingPages/RenderingSettingPage.h>
#include <helloutau/Editor/SettingPages/SystemSettingsPage.h>
#include <helloutau/Widgets/SettingPage.h>

#include "KeyboardModifierSettingPage.h"
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
        const auto keymap = new KeymapSettingPage(editor);
        catalog->addPage(keymap, QLatin1String(EditorSettingPage::pageId));
        keymap->addPage(new KeyboardModifierSettingPage(editor));
        // Plugins precedes Rendering as Plugins precedes the build settings in JetBrains IDEs.
        if (loader) {
            catalog->addPage(new PluginSettingPage(*loader),
                             QLatin1String(RenderingSettingPage::pageId));
        }
    }

}
