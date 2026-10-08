#ifndef HELLOUTAU_EDITOR_EDITORSETTINGPAGEIDS_H
#define HELLOUTAU_EDITOR_EDITORSETTINGPAGEIDS_H

namespace hello::daw {

    /// The ids of the pages that the editor adds to its setting catalog, in the order of the
    /// settings of JetBrains IDEs, by which a plugin places its pages among them. See the
    /// settings dialog in docs/Widgets.md.
    struct EditorSettingPageIds {
        static constexpr char appearanceAndBehavior[] = "editor.AppearanceAndBehavior";
        static constexpr char systemSettings[] = "editor.SystemSettings";
        static constexpr char editor[] = "editor.Editor";
        static constexpr char utau[] = "editor.Utau";
        static constexpr char audio[] = "editor.Audio";
        static constexpr char rendering[] = "editor.Rendering";
    };

}

#endif // HELLOUTAU_EDITOR_EDITORSETTINGPAGEIDS_H
