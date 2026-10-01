#ifndef HELLOUTAU_CORE_CORESETTINGPAGES_H
#define HELLOUTAU_CORE_CORESETTINGPAGES_H

#include <Core/CorePluginGlobal.h>

namespace hello::daw {

    class AppLoader;
    class Editor;

    /// Adds the pages of the core plugin to the setting catalog of \a editor in the order of
    /// the settings of JetBrains IDEs: Menus and Toolbars first under Appearance & Behavior,
    /// Keymap before Editor, and with \a loader the Plugins page of its plugins before
    /// Rendering. Without a loader, as in tests that construct the editor, there is no Plugins
    /// page. See the settings dialog in docs/Widgets.md.
    COREPLUGIN_EXPORT void addCoreSettingPages(Editor *editor, AppLoader *loader = nullptr);

}

#endif // HELLOUTAU_CORE_CORESETTINGPAGES_H
