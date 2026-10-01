#ifndef HELLOUTAU_CORE_CORESETTINGPAGES_H
#define HELLOUTAU_CORE_CORESETTINGPAGES_H

#include <Core/CorePluginGlobal.h>

namespace hello::daw {

    class Editor;

    /// Adds the pages of the core plugin to the setting catalog of \a editor in the order of
    /// the settings of JetBrains IDEs: Menus and Toolbars first under Appearance & Behavior,
    /// and Keymap before Editor. See the settings dialog in docs/Widgets.md.
    COREPLUGIN_EXPORT void addCoreSettingPages(Editor *editor);

}

#endif // HELLOUTAU_CORE_CORESETTINGPAGES_H
