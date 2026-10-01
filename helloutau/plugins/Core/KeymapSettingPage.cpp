#include "KeymapSettingPage.h"

#include <functional>

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
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTreeWidgetItemIterator>
#include <QtWidgets/QVBoxLayout>

#include <QAKCore/actionregistry.h>

#include <helloutau/Editor/Editor.h>

namespace hello::daw {

    namespace {

        constexpr int IdRole = Qt::UserRole;

        // A window, by the top-level menus of its layouts: its menu bar and its tool bar
        struct WindowLayout {
            const char *name;
            const char *menuBar;
            const char *toolBar;
        };

        const WindowLayout windowLayouts[] = {
            {QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Project Window"),
             "helloutau.mainMenu",      "helloutau.mainToolBar"            },
            {QT_TRANSLATE_NOOP("hello::daw::KeymapSettingPage", "Voice Bank Window"),
             "helloutau.voiceBankMenu", "helloutau.voiceBank.sampleToolBar"},
        };

        QString textOf(const QList<QKeySequence> &keys) {
            QStringList texts;
            for (const auto &key : keys) {
                texts.push_back(key.toString(QKeySequence::NativeText));
            }
            return texts.join(QStringLiteral(", "));
        }

    }

    KeymapSettingPage::KeymapSettingPage(Editor *editor, QObject *parent)
        : SettingPage(QLatin1String(pageId), parent), m_editor(editor),
          m_registry(editor->actionRegistry()) {
        setTitle(tr("Keymap"));
        setDescription(tr("The shortcuts of the commands."));
        setKeywords(
            {QStringLiteral("Keymap"), QStringLiteral("shortcut"), QStringLiteral("keyboard")});
    }

    QList<QKeySequence> KeymapSettingPage::shortcuts(const QString &id) const {
        return m_shortcuts.value(id);
    }

    bool KeymapSettingPage::sharesWindow(const QString &a, const QString &b) const {
        const auto first = m_windows.value(a);
        const auto second = m_windows.value(b);
        return first.isEmpty() || second.isEmpty() || first.intersects(second);
    }

    QStringList KeymapSettingPage::conflicts(const QString &id, const QKeySequence &key) const {
        QStringList result;
        for (const auto &other : m_commands) {
            if (other != id && m_shortcuts.value(other).contains(key) && sharesWindow(id, other)) {
                result.push_back(other);
            }
        }
        return result;
    }

    void KeymapSettingPage::addShortcut(const QString &id, const QKeySequence &key,
                                        const QStringList &removed) {
        for (const auto &other : removed) {
            m_shortcuts[other].removeAll(key);
        }
        if (!m_shortcuts[id].contains(key)) {
            m_shortcuts[id].push_back(key);
        }
        updateItems();
        Q_EMIT modifiedChanged();
    }

    void KeymapSettingPage::removeShortcut(const QString &id, const QKeySequence &key) {
        m_shortcuts[id].removeAll(key);
        updateItems();
        Q_EMIT modifiedChanged();
    }

    void KeymapSettingPage::resetShortcuts(const QString &id) {
        m_shortcuts[id] = defaultsOf(id);
        updateItems();
        Q_EMIT modifiedChanged();
    }

    void KeymapSettingPage::resetAll() {
        for (const auto &id : m_commands) {
            m_shortcuts[id] = defaultsOf(id);
        }
        updateItems();
        Q_EMIT modifiedChanged();
    }

    QTreeWidget *KeymapSettingPage::tree() const {
        return m_tree;
    }

    QString KeymapSettingPage::currentId() const {
        const auto item = m_tree ? m_tree->currentItem() : nullptr;
        return item ? item->data(0, IdRole).toString() : QString();
    }

    bool KeymapSettingPage::isModified() const {
        if (!m_tree) {
            return false;
        }
        for (const auto &id : m_commands) {
            if (m_shortcuts.value(id) != m_registry->actionShortcuts(id)) {
                return true;
            }
        }
        return false;
    }

    bool KeymapSettingPage::apply(QString *error) {
        // The overrides of commands that are not registered now, such as those of a plugin
        // turned off, stay.
        auto family = m_registry->shortcutsFamily();
        for (const auto &id : std::as_const(m_commands)) {
            const auto keys = m_shortcuts.value(id);
            if (keys == defaultsOf(id)) {
                family.remove(id);
            } else {
                family.insert(id, keys);
            }
        }
        m_registry->setShortcutsFamily(family);
        m_registry->updateContext(QAK::AE_Keymap);
        if (!m_editor->saveKeymap(error)) {
            return false;
        }
        updateItems();
        Q_EMIT modifiedChanged();
        return true;
    }

    QString KeymapSettingPage::nameOf(const QString &id) const {
        const auto info = m_registry->actionInfo(id);
        return info ? info->text().withoutMnemonic() : id;
    }

    QList<QKeySequence> KeymapSettingPage::defaultsOf(const QString &id) const {
        const auto info = m_registry->actionInfo(id);
        return info ? info->shortcuts() : QList<QKeySequence>();
    }

    void KeymapSettingPage::load() {
        m_commands.clear();
        m_shortcuts.clear();
        m_windows.clear();
        for (const auto &id : m_registry->actionIds()) {
            const auto info = m_registry->actionInfo(id);
            if (info && info->isCommand()) {
                m_commands.push_back(id);
                m_shortcuts.insert(id, m_registry->actionShortcuts(id));
            }
        }
        // Records the windows whose menu bar or tool bar holds each command.
        const auto layouts = m_registry->layouts().adjacencyMap();
        for (const auto &window : windowLayouts) {
            QSet<QString> visited;
            const std::function<void(const QString &)> visit = [&](const QString &container) {
                if (visited.contains(container)) {
                    return;
                }
                visited.insert(container);
                for (const auto &entry : layouts.value(container)) {
                    if (entry.type() == QAK::ActionLayoutEntry::Action) {
                        m_windows[entry.id()].insert(QLatin1String(window.name));
                    } else if (entry.type() == QAK::ActionLayoutEntry::Menu ||
                               entry.type() == QAK::ActionLayoutEntry::Group) {
                        visit(entry.id());
                    }
                }
            };
            visit(QLatin1String(window.menuBar));
            visit(QLatin1String(window.toolBar));
        }
    }

    void KeymapSettingPage::fillTree() {
        m_tree->clear();
        const auto layouts = m_registry->layouts().adjacencyMap();
        QSet<QString> listed;
        const auto leaf = [&](QTreeWidgetItem *parent, const QString &id) {
            auto item = new QTreeWidgetItem(parent, {nameOf(id)});
            item->setData(0, IdRole, id);
            const auto info = m_registry->actionInfo(id);
            if (const auto icon = m_registry->actionIcon(QString(), id, info->icon())) {
                item->setIcon(0, icon->icon());
            }
            listed.insert(id);
        };
        // Lists the commands of the menus under parent. A menu becomes an item of its own, and a
        // group is listed inline. An empty menu is removed.
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
                        if (m_shortcuts.contains(id)) {
                            leaf(parent, id);
                        }
                    } else if (entry.type() == QAK::ActionLayoutEntry::Group) {
                        fill(parent, id);
                    } else if (entry.type() == QAK::ActionLayoutEntry::Menu) {
                        auto menu = new QTreeWidgetItem(parent, {nameOf(id)});
                        fill(menu, id);
                        if (menu->childCount() == 0) {
                            delete menu;
                        }
                    }
                }
                visited.remove(container);
            };
        for (const auto &window : windowLayouts) {
            auto item = new QTreeWidgetItem(m_tree, {tr(window.name)});
            fill(item, QLatin1String(window.menuBar));
        }
        auto other = new QTreeWidgetItem(m_tree, {tr("Other")});
        for (const auto &id : std::as_const(m_commands)) {
            if (!listed.contains(id)) {
                leaf(other, id);
            }
        }
        other->sortChildren(0, Qt::AscendingOrder);
        if (other->childCount() == 0) {
            delete other;
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
            const auto id = (*it)->data(0, IdRole).toString();
            if (id.isEmpty()) {
                continue;
            }
            const auto keys = m_shortcuts.value(id);
            (*it)->setText(1, textOf(keys));
            const auto brush = keys == defaultsOf(id) ? QBrush() : QBrush(modified);
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
        // A command is shown if it matches, and a menu if one of its items is shown.
        const std::function<bool(QTreeWidgetItem *)> show = [&](QTreeWidgetItem *item) {
            const auto id = item->data(0, IdRole).toString();
            bool shown = false;
            if (!id.isEmpty()) {
                const auto info = m_registry->actionInfo(id);
                const bool byText =
                    text.isEmpty() || item->text(0).contains(text, Qt::CaseInsensitive) ||
                    (info && info->text().source.contains(text, Qt::CaseInsensitive));
                const bool byKey = key.isEmpty() || m_shortcuts.value(id).contains(key);
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
        const auto id = currentId();
        m_add->setEnabled(!id.isEmpty());
        m_remove->setEnabled(!id.isEmpty() && !m_shortcuts.value(id).isEmpty());
        m_reset->setEnabled(!id.isEmpty() && m_shortcuts.value(id) != defaultsOf(id));
    }

    // Asks for a shortcut of the current command in a dialog that lists the commands that
    // already have it, and adds it, after asking whether to remove it from them.
    void KeymapSettingPage::askShortcut() {
        const auto id = currentId();
        if (id.isEmpty()) {
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
        layout->addWidget(new QLabel(tr("Press the shortcut of %1:").arg(nameOf(id))));
        layout->addWidget(edit);
        layout->addWidget(conflictLabel);
        layout->addWidget(buttons);
        const auto update = [&] {
            const auto key = edit->keySequence();
            QStringList names;
            for (const auto &other : conflicts(id, key)) {
                names.push_back(nameOf(other));
            }
            conflictLabel->setText(
                names.isEmpty() ? QString() : tr("Already assigned to: %1").arg(names.join(u", ")));
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
        const auto others = conflicts(id, key);
        QStringList removed;
        if (!others.isEmpty()) {
            QStringList names;
            for (const auto &other : others) {
                names.push_back(nameOf(other));
            }
            QMessageBox box(QMessageBox::Warning, tr("Add Keyboard Shortcut"),
                            tr("%1 is already assigned to %2. Remove it from them?")
                                .arg(key.toString(QKeySequence::NativeText), names.join(u", ")),
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
        addShortcut(id, key, removed);
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
        layout->addLayout(buttonRow);

        fillTree();

        connect(m_search, &QLineEdit::textChanged, this, [this] { filter(); });
        connect(m_keySearch, &QKeySequenceEdit::keySequenceChanged, this, [this] { filter(); });
        connect(m_tree, &QTreeWidget::currentItemChanged, this, [this] { updateButtons(); });
        connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this] { askShortcut(); });
        connect(m_add, &QPushButton::clicked, this, [this] { askShortcut(); });
        connect(resetAllButton, &QPushButton::clicked, this, [this] { resetAll(); });
        connect(m_reset, &QPushButton::clicked, this, [this] {
            if (const auto id = currentId(); !id.isEmpty()) {
                resetShortcuts(id);
            }
        });
        // Removes the shortcut of the current command, or the shortcut chosen from a menu if the
        // command has several.
        const auto removeFrom = [this](const QPoint &at) {
            const auto id = currentId();
            const auto keys = m_shortcuts.value(id);
            if (keys.size() == 1) {
                removeShortcut(id, keys.first());
                return;
            }
            QMenu menu(m_tree);
            for (const auto &key : keys) {
                connect(menu.addAction(tr("Remove %1").arg(key.toString(QKeySequence::NativeText))),
                        &QAction::triggered, this, [this, id, key] { removeShortcut(id, key); });
            }
            menu.exec(at);
        };
        connect(m_remove, &QPushButton::clicked, this, [this, removeFrom] {
            removeFrom(m_remove->mapToGlobal(QPoint(0, m_remove->height())));
        });
        connect(m_tree, &QWidget::customContextMenuRequested, this, [this](const QPoint &at) {
            const auto id = currentId();
            if (id.isEmpty()) {
                return;
            }
            QMenu menu(m_tree);
            connect(menu.addAction(tr("&Add Keyboard Shortcut...")), &QAction::triggered, this,
                    [this] { askShortcut(); });
            for (const auto &key : m_shortcuts.value(id)) {
                connect(menu.addAction(tr("Remove %1").arg(key.toString(QKeySequence::NativeText))),
                        &QAction::triggered, this, [this, id, key] { removeShortcut(id, key); });
            }
            const auto reset = menu.addAction(tr("Re&set Shortcuts"));
            reset->setEnabled(m_shortcuts.value(id) != defaultsOf(id));
            connect(reset, &QAction::triggered, this, [this, id] { resetShortcuts(id); });
            menu.exec(m_tree->viewport()->mapToGlobal(at));
        });
        return widget;
    }

}
