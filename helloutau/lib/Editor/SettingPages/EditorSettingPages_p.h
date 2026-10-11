#ifndef HELLOUTAU_EDITOR_SETTINGPAGES_EDITORSETTINGPAGES_P_H
#define HELLOUTAU_EDITOR_SETTINGPAGES_EDITORSETTINGPAGES_P_H

namespace hello::daw {

    class AppSettings;
    class SettingCatalog;
    class UpdateChecker;

    /// Adds the pages of the editor to \a catalog: Appearance & Behavior with System Settings and
    /// its Updates in it, Editor, and Rendering. A plugin places its pages among them by
    /// the page ids, as Core places Keymap before Editor and Plugins before Rendering.
    void addEditorSettingPages(SettingCatalog *catalog, AppSettings &settings,
                               UpdateChecker &checker);

}

#endif // HELLOUTAU_EDITOR_SETTINGPAGES_EDITORSETTINGPAGES_P_H
