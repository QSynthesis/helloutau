#ifndef HELLOUTAU_EDITOR_SETTINGPAGES_APPEARANCEANDBEHAVIORCATEGORY_H
#define HELLOUTAU_EDITOR_SETTINGPAGES_APPEARANCEANDBEHAVIORCATEGORY_H

#include <helloutau/Widgets/SettingPage.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// Appearance & Behavior, a category without settings of its own, which the settings dialog
    /// shows as the links to its pages, as JetBrains IDEs show it. System Settings and the Menus
    /// and Toolbars page of the core plugin are among its pages.
    class HELLOUTAU_EDITOR_EXPORT AppearanceAndBehaviorCategory : public SettingPage {
        Q_OBJECT
    public:
        static constexpr char pageId[] = "editor.AppearanceAndBehavior";

        explicit AppearanceAndBehaviorCategory(QObject *parent = nullptr);
        ~AppearanceAndBehaviorCategory();
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGPAGES_APPEARANCEANDBEHAVIORCATEGORY_H
