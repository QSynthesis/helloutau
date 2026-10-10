#ifndef HELLOUTAU_EDITOR_EDITORICONS_P_H
#define HELLOUTAU_EDITOR_EDITORICONS_P_H

namespace QAK {
    class ActionRegistry;
}

namespace hello::daw {

    /// Adds the icons of the commands of the editor to \a registry, in the default theme. Each
    /// icon is drawn by ThemeIcon in the text color of the control that shows it, as a tool bar
    /// button or a menu item, so that one file serves light and dark themes. The files are in
    /// lib/Editor/Resources/icons, with their sources in the README of each subdirectory.
    void addEditorIcons(QAK::ActionRegistry *registry);

}

#endif // HELLOUTAU_EDITOR_EDITORICONS_P_H
