#ifndef HELLOUTAU_EDITOR_MENUSSETTINGPAGE_P_H
#define HELLOUTAU_EDITOR_MENUSSETTINGPAGE_P_H

#include <QtCore/QPointer>

#include <helloutau/Widgets/SettingPage.h>

class QModelIndex;
class QPushButton;
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
    /// The tree shows the menu bar and the tool bar of each window with their entries, a menu
    /// that several containers hold under each of them. The page edits a copy of the layouts in
    /// QActionKit's ActionLayoutsModel, which checks each edit against the manifests. apply()
    /// gives the registry the changes from the default layouts, updates every window and writes
    /// \c layouts.json, see LayoutsFile.
    class MenusSettingPage : public SettingPage {
        Q_OBJECT
    public:
        MenusSettingPage(QAK::ActionRegistry *registry, const QString &fileName,
                         QObject *parent = nullptr);
        ~MenusSettingPage() override;

        bool isModified() const override;
        bool apply(QString *error) override;

        /// The tree of the layouts, while the widget exists.
        QTreeView *tree() const;

        /// Adds \a entry after the current entry, or as the last entry of the current menu or
        /// tool bar. Returns whether the entry is added, which the manifests may refuse.
        bool addEntry(const QAK::ActionLayoutEntry &entry);

        /// Removes the current entry, which is not a menu bar or a tool bar.
        void removeCurrent();

        /// Moves the current entry up or down within its menu.
        void moveCurrent(bool up);

        /// Restores the default layouts of every menu and tool bar.
        void restoreDefaults();

    protected:
        QWidget *createWidget() override;

    private:
        QAK::ActionRegistry *m_registry;
        QString m_fileName;
        QPointer<QAK::ActionLayoutsModel> m_model;
        QPointer<LayoutNamesModel> m_names;
        QPointer<QTreeView> m_tree;
        QPointer<QPushButton> m_add;
        QPointer<QPushButton> m_addSeparator;
        QPointer<QPushButton> m_remove;
        QPointer<QPushButton> m_up;
        QPointer<QPushButton> m_down;

        void askAction();
        void select(const QList<int> &path);
        void updateButtons();
    };

}

#endif // HELLOUTAU_EDITOR_MENUSSETTINGPAGE_P_H
