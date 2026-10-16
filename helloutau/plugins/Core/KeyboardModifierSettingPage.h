#ifndef HELLOUTAU_CORE_KEYBOARDMODIFIERSETTINGPAGE_H
#define HELLOUTAU_CORE_KEYBOARDMODIFIERSETTINGPAGE_H

#include <iterator>

#include <QtCore/QPointer>

#include <helloutau/Widgets/KeyboardModifierBindings.h>
#include <helloutau/Widgets/SettingPage.h>

#include <helloutau/Editor/Editor.h>

#include <Core/CorePluginGlobal.h>

class QComboBox;
class QPushButton;

namespace hello::daw {

    /// The modifier keys of the mouse and wheel operations, a page under the keymap. See the
    /// modifier keys in the settings dialog in docs/Widgets.md.
    ///
    /// Each kind of window with modifier schemes (Editor::keyboardModifierSchemes()) has a tab
    /// that scrolls its groups, as the keymap scrolls its commands, and Restore Defaults is
    /// beside the tabs. Each scheme has a group of boxes, one for each role. The boxes ignore the
    /// wheel, which scrolls the tab instead, so that a scroll does not change a binding. The page
    /// edits a copy of the bindings. apply() rejects bindings that conflict, gives the others to
    /// the editor and writes them with Editor::saveKeymap().
    class COREPLUGIN_EXPORT KeyboardModifierSettingPage : public SettingPage {
        Q_OBJECT
    public:
        static constexpr char pageId[] = "core.KeyboardModifiers";

        explicit KeyboardModifierSettingPage(Editor *editor, QObject *parent = nullptr);

        bool isModified() const override;
        bool apply(QString *error) override;

        /// Restores every modifier binding of every kind of window to its default.
        void restoreDefaults();

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
        };
        QList<Box> m_boxes;
        QPointer<QPushButton> m_restore;

        void updateWidgets();
        QWidget *createSchemeGroup(Editor::WindowKind kind, int scheme);
    };

}

#endif // HELLOUTAU_CORE_KEYBOARDMODIFIERSETTINGPAGE_H
