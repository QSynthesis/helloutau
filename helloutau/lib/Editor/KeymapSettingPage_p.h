#ifndef HELLOUTAU_EDITOR_KEYMAPSETTINGPAGE_P_H
#define HELLOUTAU_EDITOR_KEYMAPSETTINGPAGE_P_H

#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtGui/QKeySequence>

#include <helloutau/Widgets/SettingPage.h>

class QKeySequenceEdit;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace QAK {
    class ActionRegistry;
}

namespace hello::daw {

    /// The shortcuts of the commands, as the keymap of the settings of JetBrains IDEs. See the
    /// keymap in the settings dialog in docs/Widgets.md.
    ///
    /// The tree lists the commands of each window under the menus that hold them, and the
    /// commands in no menu under Other. The page edits a copy of the shortcuts. apply() gives
    /// them to the registry, updates every window and writes \c keymap.json, see KeymapFile.
    ///
    /// A shortcut conflicts with the shortcut of another command of the same window. A command
    /// in no menu counts as a command of every window, since the window that runs it is not
    /// known from the layouts.
    class KeymapSettingPage : public SettingPage {
        Q_OBJECT
    public:
        KeymapSettingPage(QAK::ActionRegistry *registry, const QString &fileName,
                          QObject *parent = nullptr);

        bool isModified() const override;
        bool apply(QString *error) override;

        /// Returns the shortcuts of \a id as the page has them.
        QList<QKeySequence> shortcuts(const QString &id) const;

        /// Returns the commands other than \a id with the shortcut \a key that share a window
        /// with \a id.
        QStringList conflicts(const QString &id, const QKeySequence &key) const;

        /// Adds \a key to the shortcuts of \a id, after removing it from \a removed.
        void addShortcut(const QString &id, const QKeySequence &key,
                         const QStringList &removed = {});

        void removeShortcut(const QString &id, const QKeySequence &key);

        /// Restores the shortcuts of \a id from its manifest.
        void resetShortcuts(const QString &id);

        /// Restores the shortcuts of every command from their manifests.
        void resetAll();

        /// The tree of commands, while the widget exists.
        QTreeWidget *tree() const;

        /// The id of the current command of the tree, or an empty string.
        QString currentId() const;

    protected:
        QWidget *createWidget() override;

    private:
        QAK::ActionRegistry *m_registry;
        QString m_fileName;

        // The shortcuts as the page has them, by command
        QHash<QString, QList<QKeySequence>> m_shortcuts;
        // The windows whose menus or tool bars hold each command, by command. A command in no
        // menu has none.
        QHash<QString, QSet<QString>> m_windows;
        QStringList m_commands;

        QPointer<QTreeWidget> m_tree;
        QPointer<QLineEdit> m_search;
        QPointer<QKeySequenceEdit> m_keySearch;
        QPointer<QPushButton> m_add;
        QPointer<QPushButton> m_remove;
        QPointer<QPushButton> m_reset;

        void load();
        void fillTree();
        void updateItems();
        void filter();
        void updateButtons();
        void askShortcut();
        QString nameOf(const QString &id) const;
        QList<QKeySequence> defaultsOf(const QString &id) const;
        bool sharesWindow(const QString &a, const QString &b) const;
    };

}

#endif // HELLOUTAU_EDITOR_KEYMAPSETTINGPAGE_P_H
