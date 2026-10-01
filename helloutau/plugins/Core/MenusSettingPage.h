#ifndef HELLOUTAU_CORE_MENUSSETTINGPAGE_H
#define HELLOUTAU_CORE_MENUSSETTINGPAGE_H

#include <iterator>

#include <QtCore/QPointer>

#include <helloutau/Widgets/SettingPage.h>

#include <helloutau/Editor/Editor.h>

#include <Core/CorePluginGlobal.h>

class QModelIndex;
class QPushButton;
class QTabWidget;
class QTreeView;

namespace QAK {
    class ActionRegistry;
    class ActionLayoutEntry;
    class ActionLayoutsModel;
}

namespace hello::daw {

    class LayoutNamesModel;

    /// The menus and the tool bars of the windows, as the menus and tool bars of the settings of
    /// JetBrains IDEs. See the menus and tool bars in the settings dialog in docs/Widgets.md.
    ///
    /// Each kind of window has an action registry of its own, see Editor::actionRegistry(), and
    /// a tab of the page, whose tree shows the menu bar and the tool bars of the kind with their
    /// entries, a menu that several containers hold under each of them. The page edits a copy of
    /// the layouts of each registry in QActionKit's ActionLayoutsModel, which checks each edit
    /// against the manifests. apply() gives each registry the changes from its default layouts,
    /// updates every window and writes them with Editor::saveActionLayouts().
    class COREPLUGIN_EXPORT MenusSettingPage : public SettingPage {
        Q_OBJECT
    public:
        static constexpr char pageId[] = "core.MenusAndToolbars";

        explicit MenusSettingPage(Editor *editor, QObject *parent = nullptr);
        ~MenusSettingPage() override;

        bool isModified() const override;
        bool apply(QString *error) override;

        /// The tree of the layouts of \a kind, while the widget exists.
        QTreeView *tree(Editor::WindowKind kind) const;

        /// The kind of window of the current tab, whose tree the editing functions act on.
        Editor::WindowKind currentKind() const;
        void setCurrentKind(Editor::WindowKind kind);

        /// Adds \a entry after the current entry, or as the last entry of the current menu or
        /// tool bar. Returns whether the entry is added, which the manifests may refuse.
        bool addEntry(const QAK::ActionLayoutEntry &entry);

        /// Removes the current entry, which is not a menu bar or a tool bar.
        void removeCurrent();

        /// Moves the current entry up or down within its menu.
        void moveCurrent(bool up);

        /// Restores the default layouts of every menu and tool bar of every kind of window.
        void restoreDefaults();

    protected:
        QWidget *createWidget() override;

    private:
        // The layouts of one kind of window as the page edits them
        struct Panel {
            QPointer<QAK::ActionLayoutsModel> model;
            QPointer<LayoutNamesModel> names;
            QPointer<QTreeView> tree;
        };

        Editor *m_editor;
        Panel m_panels[std::size(Editor::windowKinds)];
        QPointer<QTabWidget> m_tabs;
        QPointer<QPushButton> m_add;
        QPointer<QPushButton> m_addSeparator;
        QPointer<QPushButton> m_remove;
        QPointer<QPushButton> m_up;
        QPointer<QPushButton> m_down;

        const Panel &current() const;
        QAK::ActionRegistry *registry(Editor::WindowKind kind) const;
        void askAction();
        void select(const QList<int> &path);
        void updateButtons();
    };

}

#endif // HELLOUTAU_CORE_MENUSSETTINGPAGE_H
