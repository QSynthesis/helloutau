#ifndef HELLOUTAU_EDITOR_EDITORSETTINGPAGEIDS_H
#define HELLOUTAU_EDITOR_EDITORSETTINGPAGEIDS_H

namespace hello::daw {

    /// The ids of the pages that the editor adds to its setting catalog without a class of their
    /// own, by which a plugin places its pages under them. The other pages have their id in
    /// their class, such as UtauSettingPage::pageId. See the settings dialog in docs/Widgets.md.
    struct EditorSettingPageIds {
        static constexpr char appearanceAndBehavior[] = "editor.AppearanceAndBehavior";
    };

}

#endif // HELLOUTAU_EDITOR_EDITORSETTINGPAGEIDS_H
