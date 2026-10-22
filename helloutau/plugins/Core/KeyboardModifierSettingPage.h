#ifndef HELLOUTAU_CORE_KEYBOARDMODIFIERSETTINGPAGE_H
#define HELLOUTAU_CORE_KEYBOARDMODIFIERSETTINGPAGE_H

#include <iterator>

#include <QtCore/QPointer>

#include <helloutau/Widgets/KeyboardModifierBindings.h>
#include <helloutau/Widgets/SettingPage.h>

#include <helloutau/Editor/Editor.h>

#include <Core/CorePluginGlobal.h>

class QComboBox;
class QLabel;
class QPushButton;
class QTabWidget;

namespace hello::daw {

    /// The modifier keys of the mouse and wheel operations, a page under the keymap. See the
    /// modifier keys in the settings dialog in docs/Widgets.md.
    ///
    /// Each kind of window with modifier schemes (Editor::keyboardModifierSchemes()) has a tab
    /// that scrolls its groups, as the keymap scrolls its commands, and Restore Defaults is
    /// beside the tabs. Each scheme has a group of boxes, one for each role. The boxes ignore the
    /// wheel, which scrolls the tab instead, so that a scroll does not change a binding. The page
    /// edits a copy of the bindings, marks the conflicting boxes as they change, and apply()
    /// rejects bindings that conflict, gives the others to the editor and writes them with
    /// Editor::saveKeymap().
    class COREPLUGIN_EXPORT KeyboardModifierSettingPage : public SettingPage {
        Q_OBJECT
    public:
        static constexpr char pageId[] = "core.KeyboardModifiers";

        explicit KeyboardModifierSettingPage(Editor *editor, QObject *parent = nullptr);

        bool isModified() const override;
        bool apply(QString *error) override;

        /// Restores every modifier binding of every kind of window to its default.
        void restoreDefaults();

        /// Returns a line for each conflict of the bindings as the page has them, with the kind
        /// of window, the scheme and the roles, or an empty list if none conflict. apply()
        /// refuses the bindings while the list is not empty.
        QStringList conflictDescriptions() const;

        /// The banner above the tabs that reports conflicts while any exist, while the widget
        /// exists. Each conflicting box has a warning icon beside it, named after the box with
        /// \c /conflict appended, whose tool tip names the other role, and the tab of the box
        /// carries the warning icon.
        QWidget *conflictBanner() const;

    protected:
        QWidget *createWidget() override;

    private:
        Editor *m_editor;

        // For each kind of window: its modifier bindings as the page has them
        QList<KeyboardModifierBindings> m_modifiers[std::size(Editor::windowKinds)];

        // The box of one role, by the kind of window, the index of the scheme in
        // Editor::keyboardModifierSchemes() and the role
        struct Box {
            Editor::WindowKind kind;
            int scheme;
            int role;
            QPointer<QComboBox> box;
            // The warning icon beside the box, shown while the role conflicts
            QPointer<QLabel> mark;
        };
        QList<Box> m_boxes;
        QPointer<QPushButton> m_restore;
        QPointer<QTabWidget> m_tabs;
        QPointer<QWidget> m_conflictBanner;
        // The tab of each kind of window, or -1 for a kind without modifier schemes
        int m_tabIndex[std::size(Editor::windowKinds)] = {};

        void updateWidgets();
        QWidget *createSchemeGroup(Editor::WindowKind kind, int scheme);
    };

}

#endif // HELLOUTAU_CORE_KEYBOARDMODIFIERSETTINGPAGE_H
