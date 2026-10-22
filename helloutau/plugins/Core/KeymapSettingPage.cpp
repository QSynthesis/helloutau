#include "KeymapSettingPage.h"

#include <algorithm>
#include <functional>
#include <iterator>

#include <QtCore/QSet>
#include <QtGui/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QKeySequenceEdit>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyle>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTreeWidgetItemIterator>
#include <QtWidgets/QVBoxLayout>

#include <QAKCore/actionregistry.h>

namespace hello::daw {

    namespace {

        // The id and the kind of window of the command of an item. A window or a menu has no id.
        constexpr int IdRole = Qt::UserRole;
        constexpr int KindRole = Qt::UserRole + 1;

        // A kind of window, by its name and the top-level menu of its menu bar
        struct WindowLayout {
            Editor::WindowKind kind;
            const char *name;
            const char *menuBar;
        };

        const WindowLayout windowLayouts[] = {
            {Editor::ProjectWindowKind,
             QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Project Window"),
             "helloutau.mainMenu"              },
            {Editor::VoiceBankWindowKind,
             QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Voice Bank Window"),
             "helloutau.voiceBankMenu"         },
            {Editor::FrequencyMapEditorKind,
             QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Frequency Map Editor"),
             "helloutau.frequencyMapEditorMenu"},
        };

        QString textOf(const QList<QKeySequence> &keys) {
            QStringList texts;
            for (const auto &key : keys) {
                texts.push_back(key.toString(QKeySequence::NativeText));
            }
            return texts.join(QStringLiteral(", "));
        }

        // Returns the command of item, or std::nullopt for a window or a menu.
        std::optional<KeymapSettingPage::Command> commandOf(const QTreeWidgetItem *item) {
            const auto id = item ? item->data(0, IdRole).toString() : QString();
            if (id.isEmpty()) {
                return std::nullopt;
            }
            return KeymapSettingPage::Command{Editor::WindowKind(item->data(0, KindRole).toInt()),
                                              id};
        }

    }

    KeymapSettingPage::KeymapSettingPage(Editor *editor, QObject *parent)
        : SettingPage(QLatin1String(pageId), parent), m_editor(editor) {
        setTitle(tr("Keymap"));
        setDescription(tr("The shortcuts of the commands."));
        setKeywords(
            {QStringLiteral("Keymap"), QStringLiteral("shortcut"), QStringLiteral("keyboard")});
    }

    QAK::ActionRegistry *KeymapSettingPage::registry(Editor::WindowKind kind) const {
        return m_editor->actionRegistry(kind);
    }

    QList<QKeySequence> KeymapSettingPage::shortcuts(const Command &command) const {
        return m_shortcuts[command.kind].value(command.id);
    }

    QList<KeymapSettingPage::Command> KeymapSettingPage::conflicts(const Command &command,
                                                                   const QKeySequence &key) const {
        QList<Command> result;
        for (const auto &other : m_commands[command.kind]) {
            if (other != command.id && m_shortcuts[command.kind].value(other).contains(key)) {
                result.push_back({command.kind, other});
            }
        }
        return result;
    }

    QList<QKeySequence> KeymapSettingPage::conflictingShortcuts(const Command &command) const {
        QList<QKeySequence> result;
        for (const auto &key : shortcuts(command)) {
            if (!conflicts(command, key).isEmpty()) {
                result.push_back(key);
            }
        }
        return result;
    }

    QStringList KeymapSettingPage::conflictDescriptions() const {
        QStringList lines;
        for (const auto &window : windowLayouts) {
            const auto kind = window.kind;
            // The shortcuts in the order of the commands, with the commands that have each
            QList<QKeySequence> keys;
            QHash<QKeySequence, QStringList> names;
            for (const auto &id : m_commands[kind]) {
                for (const auto &key : m_shortcuts[kind].value(id)) {
                    if (!names.contains(key)) {
                        keys.push_back(key);
                    }
                    names[key].push_back(nameOf({kind, id}));
                }
            }
            for (const auto &key : std::as_const(keys)) {
                const auto &commands = names[key];
                if (commands.size() > 1) {
                    lines.push_back(tr("%1 in %2: %3")
                                        .arg(key.toString(QKeySequence::NativeText),
                                             tr(window.name), commands.join(u", ")));
                }
            }
        }
        return lines;
    }

    QWidget *KeymapSettingPage::conflictBanner() const {
        return m_conflictBanner;
    }

    void KeymapSettingPage::addShortcut(const Command &command, const QKeySequence &key,
                                        const QList<Command> &removed) {
        for (const auto &other : removed) {
            m_shortcuts[other.kind][other.id].removeAll(key);
        }
        auto &keys = m_shortcuts[command.kind][command.id];
        if (!keys.contains(key)) {
            keys.push_back(key);
        }
        updateItems();
        Q_EMIT modifiedChanged();
    }

    void KeymapSettingPage::removeShortcut(const Command &command, const QKeySequence &key) {
        m_shortcuts[command.kind][command.id].removeAll(key);
        updateItems();
        Q_EMIT modifiedChanged();
    }

    void KeymapSettingPage::resetShortcuts(const Command &command) {
        m_shortcuts[command.kind][command.id] = defaultsOf(command);
        updateItems();
        Q_EMIT modifiedChanged();
    }

    void KeymapSettingPage::resetAll() {
        for (const auto kind : Editor::windowKinds) {
            for (const auto &id : std::as_const(m_commands[kind])) {
                m_shortcuts[kind][id] = defaultsOf({kind, id});
            }
        }
        updateItems();
        Q_EMIT modifiedChanged();
    }

    QTreeWidget *KeymapSettingPage::tree() const {
        return m_tree;
    }

    std::optional<KeymapSettingPage::Command> KeymapSettingPage::currentCommand() const {
        return commandOf(m_tree ? m_tree->currentItem() : nullptr);
    }

    bool KeymapSettingPage::isModified() const {
        if (!m_tree) {
            return false;
        }
        for (const auto kind : Editor::windowKinds) {
            for (const auto &id : m_commands[kind]) {
                if (m_shortcuts[kind].value(id) != registry(kind)->actionShortcuts(id)) {
                    return true;
                }
            }
        }
        return false;
    }

    bool KeymapSettingPage::apply(QString *error) {
        // Checked again here, because a shortcut of two commands of a window is ambiguous, and Qt
        // triggers no action for an ambiguous shortcut.
        const auto conflicting = conflictDescriptions();
        if (!conflicting.isEmpty()) {
            if (error) {
                *error = tr("Some shortcuts are assigned to more than one command:") +
                         QLatin1Char('\n') + conflicting.join(QLatin1Char('\n'));
            }
            return false;
        }
        for (const auto kind : Editor::windowKinds) {
            // The overrides of commands that are not registered now, such as those of a plugin
            // turned off, stay.
            const auto actions = registry(kind);
            auto family = actions->shortcutsFamily();
            for (const auto &id : std::as_const(m_commands[kind])) {
                const auto keys = m_shortcuts[kind].value(id);
                if (keys == defaultsOf({kind, id})) {
                    family.remove(id);
                } else {
                    family.insert(id, keys);
                }
            }
            actions->setShortcutsFamily(family);
            actions->updateContext(QAK::AE_Keymap);
        }
        if (!m_editor->saveKeymap(error)) {
            return false;
        }
        updateItems();
        Q_EMIT modifiedChanged();
        return true;
    }

    QString KeymapSettingPage::nameOf(const Command &command) const {
        const auto info = registry(command.kind)->actionInfo(command.id);
        return info ? info->text().withoutMnemonic() : command.id;
    }

    QList<QKeySequence> KeymapSettingPage::defaultsOf(const Command &command) const {
        const auto info = registry(command.kind)->actionInfo(command.id);
        return info ? info->shortcuts() : QList<QKeySequence>();
    }

    void KeymapSettingPage::load() {
        for (const auto kind : Editor::windowKinds) {
            m_commands[kind].clear();
            m_shortcuts[kind].clear();
            const auto actions = registry(kind);
            for (const auto &id : actions->actionIds()) {
                const auto info = actions->actionInfo(id);
                if (info && info->isCommand()) {
                    m_commands[kind].push_back(id);
                    m_shortcuts[kind].insert(id, actions->actionShortcuts(id));
                }
            }
        }
    }

    void KeymapSettingPage::fillTree() {
        for (int windowIndex = 0; windowIndex < int(std::size(windowLayouts)); ++windowIndex) {
            const auto &window = windowLayouts[windowIndex];
            QTreeWidget *tree = m_trees[windowIndex];
            tree->clear();
            const auto kind = window.kind;
            const auto actions = registry(kind);
            const auto layouts = actions->layouts().adjacencyMap();
            QSet<QString> listed;
            const auto leaf = [&](QTreeWidgetItem *parent, const QString &id) {
                auto item = parent ? new QTreeWidgetItem(parent, {nameOf({kind, id})})
                                   : new QTreeWidgetItem(tree, {nameOf({kind, id})});
                item->setData(0, IdRole, id);
                item->setData(0, KindRole, int(kind));
                const auto info = actions->actionInfo(id);
                if (const auto icon = actions->actionIcon(QString(), id, info->icon())) {
                    item->setIcon(0, icon->icon());
                }
                listed.insert(id);
            };
            // Lists the commands of the menus under parent. A menu becomes an item of its own,
            // and a group is listed inline. An empty menu is removed.
            QSet<QString> visited;
            const std::function<void(QTreeWidgetItem *, const QString &)> fill =
                [&](QTreeWidgetItem *parent, const QString &container) {
                    if (visited.contains(container)) {
                        return;
                    }
                    visited.insert(container);
                    for (const auto &entry : layouts.value(container)) {
                        const auto id = entry.id();
                        if (entry.type() == QAK::ActionLayoutEntry::Action) {
                            if (m_shortcuts[kind].contains(id)) {
                                leaf(parent, id);
                            }
                        } else if (entry.type() == QAK::ActionLayoutEntry::Group) {
                            fill(parent, id);
                        } else if (entry.type() == QAK::ActionLayoutEntry::Menu) {
                            auto menu = parent ? new QTreeWidgetItem(parent, {nameOf({kind, id})})
                                               : new QTreeWidgetItem(tree, {nameOf({kind, id})});
                            fill(menu, id);
                            if (menu->childCount() == 0) {
                                delete menu;
                            }
                        }
                    }
                    visited.remove(container);
                };
            fill(nullptr, QLatin1String(window.menuBar));
            // The commands of this kind of window in no menu of its menu bar
            auto other = new QTreeWidgetItem(tree, {tr("Other")});
            for (const auto &id : std::as_const(m_commands[kind])) {
                if (!listed.contains(id)) {
                    leaf(other, id);
                }
            }
            other->sortChildren(0, Qt::AscendingOrder);
            if (other->childCount() == 0) {
                delete other;
            }
        }
        updateItems();
        for (QTreeWidget *tree : m_trees) {
            tree->expandToDepth(0);
        }
    }

    void KeymapSettingPage::updateItems() {
        if (!m_tree) {
            return;
        }
        // The commands whose shortcuts differ from their manifests are drawn in the color of links,
        // as JetBrains IDEs mark them.
        const auto modified = m_tree->palette().color(QPalette::Link);
        // The commands with a conflicting shortcut, the menus that contain them and their tabs
        // carry the warning icon, so that a collapsed menu and another tab show the conflict.
        const auto warning = m_tree->style()->standardIcon(QStyle::SP_MessageBoxWarning);
        bool anyConflict = false;
        for (int index = 0; index < int(std::size(m_trees)); ++index) {
            QTreeWidget *tree = m_trees[index];
            QSet<QTreeWidgetItem *> containing;
            for (QTreeWidgetItemIterator it(tree); *it; ++it) {
                const auto command = commandOf(*it);
                if (!command) {
                    continue;
                }
                const auto keys = shortcuts(*command);
                (*it)->setText(1, textOf(keys));
                const auto brush = keys == defaultsOf(*command) ? QBrush() : QBrush(modified);
                (*it)->setForeground(0, brush);
                (*it)->setForeground(1, brush);
                QStringList conflictLines;
                for (const auto &key : conflictingShortcuts(*command)) {
                    QStringList names;
                    for (const auto &other : conflicts(*command, key)) {
                        names.push_back(nameOf(other));
                    }
                    conflictLines.push_back(
                        tr("%1 is also assigned to %2.")
                            .arg(key.toString(QKeySequence::NativeText), names.join(u", ")));
                }
                (*it)->setIcon(1, conflictLines.isEmpty() ? QIcon() : warning);
                (*it)->setToolTip(1, conflictLines.join(QLatin1Char('\n')));
                if (!conflictLines.isEmpty()) {
                    for (auto parent = (*it)->parent(); parent; parent = parent->parent()) {
                        containing.insert(parent);
                    }
                }
            }
            for (QTreeWidgetItemIterator it(tree); *it; ++it) {
                if (!commandOf(*it)) {
                    (*it)->setIcon(1, containing.contains(*it) ? warning : QIcon());
                }
            }
            const bool conflict = std::any_of(
                m_commands[windowLayouts[index].kind].cbegin(),
                m_commands[windowLayouts[index].kind].cend(), [this, index](const QString &id) {
                    return !conflictingShortcuts({windowLayouts[index].kind, id}).isEmpty();
                });
            m_tabs->setTabIcon(index, conflict ? warning : QIcon());
            anyConflict = anyConflict || conflict;
        }
        m_conflictBanner->setVisible(anyConflict);
        filter();
        updateButtons();
    }

    void KeymapSettingPage::filter() {
        if (!m_tree) {
            return;
        }
        const auto text = m_search->text().trimmed();
        const auto key = m_keySearch->keySequence();
        // A command is shown if it matches, and a window or a menu if one of its items is shown.
        const std::function<bool(QTreeWidgetItem *)> show = [&](QTreeWidgetItem *item) {
            const auto command = commandOf(item);
            bool shown = false;
            if (command) {
                const auto info = registry(command->kind)->actionInfo(command->id);
                const bool byText =
                    text.isEmpty() || item->text(0).contains(text, Qt::CaseInsensitive) ||
                    (info && info->text().source.contains(text, Qt::CaseInsensitive));
                const bool byKey = key.isEmpty() || shortcuts(*command).contains(key);
                shown = byText && byKey;
            } else {
                for (int i = 0; i < item->childCount(); ++i) {
                    shown = show(item->child(i)) || shown;
                }
                if (shown && (!text.isEmpty() || !key.isEmpty())) {
                    item->setExpanded(true);
                }
            }
            item->setHidden(!shown);
            return shown;
        };
        for (QTreeWidget *tree : m_trees) {
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                show(tree->topLevelItem(i));
            }
        }
    }

    void KeymapSettingPage::updateButtons() {
        const auto command = currentCommand();
        m_add->setEnabled(command.has_value());
        m_remove->setEnabled(command && !shortcuts(*command).isEmpty());
        m_reset->setEnabled(command && shortcuts(*command) != defaultsOf(*command));
    }

    // Shows a dialog in which the user enters a shortcut for the current command, and adds it. The
    // dialog lists the commands of the same kind of window that already have the shortcut, and
    // the user confirms removing it from them first.
    void KeymapSettingPage::promptForShortcut() {
        const auto command = currentCommand();
        if (!command) {
            return;
        }
        QDialog dialog(m_tree->window());
        dialog.setWindowTitle(tr("Add Keyboard Shortcut"));
        auto edit = new QKeySequenceEdit();
        edit->setObjectName(QStringLiteral("shortcut"));
        edit->setMaximumSequenceLength(1);
        edit->setClearButtonEnabled(true);
        auto conflictLabel = new QLabel();
        conflictLabel->setObjectName(QStringLiteral("conflicts"));
        conflictLabel->setWordWrap(true);
        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        auto layout = new QVBoxLayout(&dialog);
        layout->addWidget(new QLabel(tr("Press the shortcut of %1:").arg(nameOf(*command))));
        layout->addWidget(edit);
        layout->addWidget(conflictLabel);
        layout->addWidget(buttons);
        const auto namesOf = [this](const QList<Command> &commands) {
            QStringList names;
            for (const auto &other : commands) {
                names.push_back(nameOf(other));
            }
            return names.join(u", ");
        };
        const auto update = [&] {
            const auto key = edit->keySequence();
            const auto others = conflicts(*command, key);
            conflictLabel->setText(
                others.isEmpty() ? QString() : tr("Already assigned to: %1").arg(namesOf(others)));
            buttons->button(QDialogButtonBox::Ok)->setEnabled(!key.isEmpty());
        };
        connect(edit, &QKeySequenceEdit::keySequenceChanged, &dialog, update);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        update();
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        const auto key = edit->keySequence();
        const auto others = conflicts(*command, key);
        QList<Command> removed;
        if (!others.isEmpty()) {
            QMessageBox box(QMessageBox::Warning, tr("Add Keyboard Shortcut"),
                            tr("%1 is already assigned to %2. Remove it from them?")
                                .arg(key.toString(QKeySequence::NativeText), namesOf(others)),
                            QMessageBox::Cancel, m_tree->window());
            const auto remove = box.addButton(tr("&Remove"), QMessageBox::AcceptRole);
            const auto leave = box.addButton(tr("&Leave"), QMessageBox::NoRole);
            box.setDefaultButton(remove);
            box.exec();
            if (box.clickedButton() == remove) {
                removed = others;
            } else if (box.clickedButton() != leave) {
                return;
            }
        }
        addShortcut(*command, key, removed);
    }

    QWidget *KeymapSettingPage::createWidget() {
        load();
        auto widget = new QWidget();

        m_search = new QLineEdit();
        m_search->setObjectName(QStringLiteral("search"));
        m_search->setPlaceholderText(tr("Search by name"));
        m_search->setClearButtonEnabled(true);
        m_keySearch = new QKeySequenceEdit();
        m_keySearch->setObjectName(QStringLiteral("keySearch"));
        m_keySearch->setMaximumSequenceLength(1);
        m_keySearch->setClearButtonEnabled(true);
        m_keySearch->setToolTip(tr("Find the commands by a shortcut"));
        auto searchRow = new QHBoxLayout();
        searchRow->addWidget(m_search, 2);
        searchRow->addWidget(new QLabel(tr("Shortcut:")));
        searchRow->addWidget(m_keySearch, 1);

        m_tabs = new QTabWidget();
        m_tabs->setObjectName(QStringLiteral("windows"));
        for (int i = 0; i < int(std::size(windowLayouts)); ++i) {
            auto tree = new QTreeWidget();
            tree->setObjectName(QStringLiteral("commands"));
            tree->setHeaderLabels({tr("Command"), tr("Shortcuts")});
            tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
            tree->header()->setStretchLastSection(false);
            tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
            tree->setContextMenuPolicy(Qt::CustomContextMenu);
            m_trees[i] = tree;
            m_tabs->addTab(tree, tr(windowLayouts[i].name));
        }
        m_tree = m_trees[0];

        m_add = new QPushButton(tr("&Add Shortcut..."));
        m_add->setObjectName(QStringLiteral("add"));
        m_remove = new QPushButton(tr("Re&move Shortcut"));
        m_remove->setObjectName(QStringLiteral("remove"));
        m_reset = new QPushButton(tr("Re&set"));
        m_reset->setObjectName(QStringLiteral("reset"));
        auto resetAllButton = new QPushButton(tr("Restore &Defaults"));
        resetAllButton->setObjectName(QStringLiteral("resetAll"));
        auto buttonColumn = new QVBoxLayout();
        buttonColumn->addWidget(m_add);
        buttonColumn->addWidget(m_remove);
        buttonColumn->addWidget(m_reset);
        buttonColumn->addStretch();
        buttonColumn->addWidget(resetAllButton);

        auto content = new QHBoxLayout();
        content->addWidget(m_tabs, 1);
        content->addLayout(buttonColumn);

        m_conflictBanner = new QWidget();
        m_conflictBanner->setObjectName(QStringLiteral("conflictBanner"));
        {
            const auto size = widget->style()->pixelMetric(QStyle::PM_SmallIconSize);
            auto icon = new QLabel();
            icon->setPixmap(
                widget->style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(size, size));
            auto text = new QLabel(tr("Some shortcuts are assigned to more than one command of "
                                      "a window. The commands, their menus and their tabs are "
                                      "marked with the warning icon. The shortcuts cannot be "
                                      "applied until each belongs to one command."));
            text->setWordWrap(true);
            auto bannerLayout = new QHBoxLayout(m_conflictBanner);
            bannerLayout->setContentsMargins(0, 0, 0, 0);
            bannerLayout->addWidget(icon, 0, Qt::AlignTop);
            bannerLayout->addWidget(text, 1);
        }

        auto layout = new QVBoxLayout(widget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addLayout(searchRow);
        layout->addWidget(m_conflictBanner);
        layout->addLayout(content, 1);

        fillTree();

        connect(m_search, &QLineEdit::textChanged, this, [this] { filter(); });
        connect(m_keySearch, &QKeySequenceEdit::keySequenceChanged, this, [this] { filter(); });
        connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
            m_tree = m_trees[index];
            updateItems();
        });
        for (QTreeWidget *tree : m_trees) {
            connect(tree, &QTreeWidget::currentItemChanged, this, [this] { updateButtons(); });
            connect(tree, &QTreeWidget::itemDoubleClicked, this, [this] { promptForShortcut(); });
        }
        connect(m_add, &QPushButton::clicked, this, [this] { promptForShortcut(); });
        connect(resetAllButton, &QPushButton::clicked, this, [this] { resetAll(); });
        connect(m_reset, &QPushButton::clicked, this, [this] {
            if (const auto command = currentCommand()) {
                resetShortcuts(*command);
            }
        });
        // Removes the shortcut of the current command, or the shortcut chosen from a menu if the
        // command has several.
        const auto removeFrom = [this](const QPoint &at) {
            const auto command = currentCommand();
            if (!command) {
                return;
            }
            const auto keys = shortcuts(*command);
            if (keys.size() == 1) {
                removeShortcut(*command, keys.first());
                return;
            }
            QMenu menu(m_tree);
            for (const auto &key : keys) {
                connect(menu.addAction(tr("Remove %1").arg(key.toString(QKeySequence::NativeText))),
                        &QAction::triggered, this,
                        [this, command, key] { removeShortcut(*command, key); });
            }
            menu.exec(at);
        };
        connect(m_remove, &QPushButton::clicked, this, [this, removeFrom] {
            removeFrom(m_remove->mapToGlobal(QPoint(0, m_remove->height())));
        });
        for (QTreeWidget *tree : m_trees) {
            connect(
                tree, &QWidget::customContextMenuRequested, this, [this, tree](const QPoint &at) {
                    m_tree = tree;
                    const auto command = currentCommand();
                    if (!command) {
                        return;
                    }
                    QMenu menu(m_tree);
                    connect(menu.addAction(tr("&Add Keyboard Shortcut...")), &QAction::triggered,
                            this, [this] { promptForShortcut(); });
                    for (const auto &key : shortcuts(*command)) {
                        connect(menu.addAction(
                                    tr("Remove %1").arg(key.toString(QKeySequence::NativeText))),
                                &QAction::triggered, this,
                                [this, command, key] { removeShortcut(*command, key); });
                    }
                    const auto reset = menu.addAction(tr("Re&set Shortcuts"));
                    reset->setEnabled(shortcuts(*command) != defaultsOf(*command));
                    connect(reset, &QAction::triggered, this,
                            [this, command] { resetShortcuts(*command); });
                    menu.exec(m_tree->viewport()->mapToGlobal(at));
                });
        }
        return widget;
    }

}
