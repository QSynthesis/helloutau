#include "KeymapSettingPage.h"

#include <functional>

#include <QtCore/QSet>
#include <QtCore/QSignalBlocker>
#include <QtGui/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QKeySequenceEdit>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QGroupBox>
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
             "helloutau.mainMenu"     },
            {Editor::VoiceBankWindowKind,
             QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Voice Bank Window"),
             "helloutau.voiceBankMenu"},
        };

        QString textOf(const QList<QKeySequence> &keys) {
            QStringList texts;
            for (const auto &key : keys) {
                texts.push_back(key.toString(QKeySequence::NativeText));
            }
            return texts.join(QStringLiteral(", "));
        }

        const Qt::KeyboardModifiers modifierOptions[] = {
            Qt::NoModifier,
            Qt::ControlModifier,
            Qt::AltModifier,
            Qt::ShiftModifier,
            Qt::MetaModifier,
            Qt::ControlModifier | Qt::AltModifier,
            Qt::ControlModifier | Qt::ShiftModifier,
            Qt::AltModifier | Qt::ShiftModifier,
            Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier,
        };

        const char *modifierOptionNames[] = {
            QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "None"),
            QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Ctrl"),
            QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Alt"),
            QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Shift"),
            QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Meta"),
            QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Ctrl+Alt"),
            QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Ctrl+Shift"),
            QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Alt+Shift"),
            QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Ctrl+Alt+Shift"),
        };

        const char *modifierNames[] = {
            QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Horizontal Scroll"),
            QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Time Zoom"),
            QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Key Zoom"),
            QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Drag Zoom"),
            QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Drag Zoom Axis Lock"),
            QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Disable Note Snap"),
            QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Lock Parameter Time"),
            QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Snap Parameter Value"),
        };

        Qt::KeyboardModifiers modifierAt(const EditorModifierBindings &bindings, int index) {
            switch (index) {
            case 0:
                return bindings.horizontalScroll;
            case 1:
                return bindings.timeZoom;
            case 2:
                return bindings.keyZoom;
            case 3:
                return bindings.dragZoom;
            case 4:
                return bindings.dragZoomAxisLock;
            case 5:
                return bindings.disableNoteSnap;
            case 6:
                return bindings.lockParameterTime;
            default:
                return bindings.snapParameterValue;
            }
        }

        void setModifierAt(EditorModifierBindings &bindings, int index,
                           Qt::KeyboardModifiers modifiers) {
            switch (index) {
            case 0:
                bindings.horizontalScroll = modifiers;
                break;
            case 1:
                bindings.timeZoom = modifiers;
                break;
            case 2:
                bindings.keyZoom = modifiers;
                break;
            case 3:
                bindings.dragZoom = modifiers;
                break;
            case 4:
                bindings.dragZoomAxisLock = modifiers;
                break;
            case 5:
                bindings.disableNoteSnap = modifiers;
                break;
            case 6:
                bindings.lockParameterTime = modifiers;
                break;
            default:
                bindings.snapParameterValue = modifiers;
                break;
            }
        }

        bool validModifiers(const EditorModifierBindings &bindings) {
            const auto wheel = {bindings.horizontalScroll, bindings.timeZoom, bindings.keyZoom};
            for (auto it = wheel.begin(); it != wheel.end(); ++it) {
                if (*it == Qt::NoModifier) {
                    continue;
                }
                for (auto other = std::next(it); other != wheel.end(); ++other) {
                    if (*it == *other) {
                        return false;
                    }
                }
            }
            return (bindings.dragZoom & bindings.dragZoomAxisLock) == Qt::NoModifier;
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
        m_modifiers = {};
        updateItems();
        updateModifierWidgets();
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
        return m_modifiers != m_editor->modifierBindings();
    }

    bool KeymapSettingPage::apply(QString *error) {
        if (!validModifiers(m_modifiers)) {
            if (error) {
                *error = tr("Modifier bindings conflict with each other.");
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
        m_editor->setModifierBindings(m_modifiers);
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
        m_modifiers = m_editor->modifierBindings();
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

    void KeymapSettingPage::updateModifierWidgets() {
        for (int i = 0; i < int(std::size(m_modifierBoxes)); ++i) {
            if (!m_modifierBoxes[i]) {
                continue;
            }
            const auto modifiers = modifierAt(m_modifiers, i);
            int option = 0;
            while (option < int(std::size(modifierOptions)) && modifierOptions[option] != modifiers) {
                ++option;
            }
            const QSignalBlocker blocker(m_modifierBoxes[i]);
            m_modifierBoxes[i]->setCurrentIndex(option < int(std::size(modifierOptions)) ? option : 0);
        }
    }

    void KeymapSettingPage::modifierChanged(int index, int option) {
        if (index < 0 || index >= int(std::size(m_modifierBoxes)) ||
            option < 0 || option >= int(std::size(modifierOptions))) {
            return;
        }
        setModifierAt(m_modifiers, index, modifierOptions[option]);
        Q_EMIT modifiedChanged();
    }

    void KeymapSettingPage::fillTree() {
        m_tree->clear();
        for (const auto &window : windowLayouts) {
            const auto kind = window.kind;
            const auto actions = registry(kind);
            const auto layouts = actions->layouts().adjacencyMap();
            QSet<QString> listed;
            const auto leaf = [&](QTreeWidgetItem *parent, const QString &id) {
                auto item = new QTreeWidgetItem(parent, {nameOf({kind, id})});
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
                            auto menu = new QTreeWidgetItem(parent, {nameOf({kind, id})});
                            fill(menu, id);
                            if (menu->childCount() == 0) {
                                delete menu;
                            }
                        }
                    }
                    visited.remove(container);
                };
            auto item = new QTreeWidgetItem(m_tree, {tr(window.name)});
            fill(item, QLatin1String(window.menuBar));
            // The commands of this kind of window in no menu of its menu bar
            auto other = new QTreeWidgetItem(item, {tr("Other")});
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
        m_tree->expandToDepth(0);
    }

    void KeymapSettingPage::updateItems() {
        if (!m_tree) {
            return;
        }
        // The commands whose shortcuts differ from their manifests are drawn in the color of links,
        // as JetBrains IDEs mark them.
        const auto modified = m_tree->palette().color(QPalette::Link);
        for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
            const auto command = commandOf(*it);
            if (!command) {
                continue;
            }
            const auto keys = shortcuts(*command);
            (*it)->setText(1, textOf(keys));
            const auto brush = keys == defaultsOf(*command) ? QBrush() : QBrush(modified);
            (*it)->setForeground(0, brush);
            (*it)->setForeground(1, brush);
        }
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
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
            show(m_tree->topLevelItem(i));
        }
    }

    void KeymapSettingPage::updateButtons() {
        const auto command = currentCommand();
        m_add->setEnabled(command.has_value());
        m_remove->setEnabled(command && !shortcuts(*command).isEmpty());
        m_reset->setEnabled(command && shortcuts(*command) != defaultsOf(*command));
    }

    // Asks for a shortcut of the current command in a dialog that lists the commands of the same
    // kind of window that already have it, and adds it, after asking whether to remove it from
    // them.
    void KeymapSettingPage::askShortcut() {
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

        m_tree = new QTreeWidget();
        m_tree->setObjectName(QStringLiteral("commands"));
        m_tree->setHeaderLabels({tr("Command"), tr("Shortcuts")});
        m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_tree->header()->setStretchLastSection(false);
        m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        m_tree->setContextMenuPolicy(Qt::CustomContextMenu);

        m_add = new QPushButton(tr("&Add Shortcut..."));
        m_add->setObjectName(QStringLiteral("add"));
        m_remove = new QPushButton(tr("Re&move Shortcut"));
        m_remove->setObjectName(QStringLiteral("remove"));
        m_reset = new QPushButton(tr("Re&set"));
        m_reset->setObjectName(QStringLiteral("reset"));
        auto resetAllButton = new QPushButton(tr("Restore &Defaults"));
        resetAllButton->setObjectName(QStringLiteral("resetAll"));
        auto buttonRow = new QHBoxLayout();
        buttonRow->addWidget(m_add);
        buttonRow->addWidget(m_remove);
        buttonRow->addWidget(m_reset);
        buttonRow->addStretch();
        buttonRow->addWidget(resetAllButton);

        auto layout = new QVBoxLayout(widget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addLayout(searchRow);
        layout->addWidget(m_tree, 1);
        m_modifierGroup = new QGroupBox(tr("View Modifiers"));
        auto modifierLayout = new QFormLayout(m_modifierGroup);
        for (int i = 0; i < int(std::size(m_modifierBoxes)); ++i) {
            m_modifierBoxes[i] = new QComboBox(m_modifierGroup);
            m_modifierBoxes[i]->setObjectName(QStringLiteral("modifier_%1").arg(i));
            for (const auto *name : modifierOptionNames) {
                m_modifierBoxes[i]->addItem(tr(name));
            }
            modifierLayout->addRow(tr(modifierNames[i]), m_modifierBoxes[i]);
            connect(m_modifierBoxes[i], qOverload<int>(&QComboBox::currentIndexChanged), this,
                    [this, i](int option) { modifierChanged(i, option); });
        }
        layout->addWidget(m_modifierGroup);
        layout->addLayout(buttonRow);

        fillTree();
        updateModifierWidgets();

        connect(m_search, &QLineEdit::textChanged, this, [this] { filter(); });
        connect(m_keySearch, &QKeySequenceEdit::keySequenceChanged, this, [this] { filter(); });
        connect(m_tree, &QTreeWidget::currentItemChanged, this, [this] { updateButtons(); });
        connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this] { askShortcut(); });
        connect(m_add, &QPushButton::clicked, this, [this] { askShortcut(); });
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
        connect(m_tree, &QWidget::customContextMenuRequested, this, [this](const QPoint &at) {
            const auto command = currentCommand();
            if (!command) {
                return;
            }
            QMenu menu(m_tree);
            connect(menu.addAction(tr("&Add Keyboard Shortcut...")), &QAction::triggered, this,
                    [this] { askShortcut(); });
            for (const auto &key : shortcuts(*command)) {
                connect(menu.addAction(tr("Remove %1").arg(key.toString(QKeySequence::NativeText))),
                        &QAction::triggered, this,
                        [this, command, key] { removeShortcut(*command, key); });
            }
            const auto reset = menu.addAction(tr("Re&set Shortcuts"));
            reset->setEnabled(shortcuts(*command) != defaultsOf(*command));
            connect(reset, &QAction::triggered, this,
                    [this, command] { resetShortcuts(*command); });
            menu.exec(m_tree->viewport()->mapToGlobal(at));
        });
        return widget;
    }

}
