#ifndef HELLOUTAU_CORE_KEYMAPSETTINGPAGE_H
#define HELLOUTAU_CORE_KEYMAPSETTINGPAGE_H

#include <iterator>
#include <optional>

#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtGui/QKeySequence>

#include <helloutau/Widgets/SettingPage.h>

#include <helloutau/Editor/Editor.h>

#include <Core/CorePluginGlobal.h>

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
    /// Each kind of window has an action registry of its own, see Editor::actionRegistry(). The
    /// tree lists the kinds of window, and under each the commands of its registry in the menus
    /// of its menu bar, and its commands in no menu under Other. The page edits a copy of the
    /// shortcuts. apply() gives them to the registries, updates every window and writes them with
    /// Editor::saveKeymap().
    ///
    /// A command is identified by the kind of window and its id, so that a command that two kinds
    /// of window have, such as Undo, has shortcuts in each apart from the other. A shortcut
    /// conflicts with the shortcut of another command of the same kind of window alone.
    class COREPLUGIN_EXPORT KeymapSettingPage : public SettingPage {
        Q_OBJECT
    public:
        static constexpr char pageId[] = "core.Keymap";

        /// A command of one kind of window
        struct Command {
            Editor::WindowKind kind = Editor::ProjectWindowKind;
            QString id;

            inline bool operator==(const Command &other) const {
                return kind == other.kind && id == other.id;
            }
        };

        explicit KeymapSettingPage(Editor *editor, QObject *parent = nullptr);

        bool isModified() const override;
        bool apply(QString *error) override;

        /// Returns the shortcuts of \a command as the page has them.
        QList<QKeySequence> shortcuts(const Command &command) const;

        /// Returns the other commands of the kind of window of \a command with the shortcut
        /// \a key.
        QList<Command> conflicts(const Command &command, const QKeySequence &key) const;

        /// Adds \a key to the shortcuts of \a command, after removing it from \a removed.
        void addShortcut(const Command &command, const QKeySequence &key,
                         const QList<Command> &removed = {});

        void removeShortcut(const Command &command, const QKeySequence &key);

        /// Restores the shortcuts of \a command from its manifest.
        void resetShortcuts(const Command &command);

        /// Restores the shortcuts of every command from their manifests.
        void resetAll();

        /// The tree of commands, while the widget exists.
        QTreeWidget *tree() const;

        /// Returns the current command of the tree, or \c std::nullopt if the current item is no
        /// command.
        std::optional<Command> currentCommand() const;

    protected:
        QWidget *createWidget() override;

    private:
        Editor *m_editor;

        // For each kind of window: the commands of its registry, and their shortcuts as the page
        // has them
        QStringList m_commands[std::size(Editor::windowKinds)];
        QHash<QString, QList<QKeySequence>> m_shortcuts[std::size(Editor::windowKinds)];

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
        QAK::ActionRegistry *registry(Editor::WindowKind kind) const;
        QString nameOf(const Command &command) const;
        QList<QKeySequence> defaultsOf(const Command &command) const;
    };

}

#endif // HELLOUTAU_CORE_KEYMAPSETTINGPAGE_H
