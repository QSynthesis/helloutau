#include "MenusSettingPage.h"

#include <optional>

#include <QtCore/QIdentityProxyModel>
#include <QtCore/QSortFilterProxyModel>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>

#include <QAKCore/actionlayoutsmodel.h>
#include <QAKCore/actioncatalogmodel.h>
#include <QAKCore/actionregistry.h>

namespace hello::daw {

    namespace {

        using Entry = QAK::ActionLayoutEntry;

        // A top-level container of a kind of window, by its id and the name the tree shows
        struct TopLevelNode {
            const char *id;
            const char *name;
        };

        // A kind of window, by the name of its tab and its menu bar and tool bars, which the tree
        // shows at the top
        struct WindowLayouts {
            Editor::WindowKind kind;
            const char *name;
            TopLevelNode nodes[2];
        };

        const WindowLayouts windowLayouts[] = {
            {Editor::ProjectWindowKind,
             QT_TRANSLATE_NOOP("hello::daw::MenusSettingPage", "Project Window"),
             {{"helloutau.mainMenu",
               QT_TRANSLATE_NOOP("hello::daw::MenusSettingPage", "Main Menu")},
              {"helloutau.mainToolBar",
               QT_TRANSLATE_NOOP("hello::daw::MenusSettingPage", "Main Toolbar")}}  },
            {Editor::VoiceBankWindowKind,
             QT_TRANSLATE_NOOP("hello::daw::MenusSettingPage", "Voice Bank Window"),
             {{"helloutau.voiceBankMenu",
               QT_TRANSLATE_NOOP("hello::daw::MenusSettingPage", "Main Menu")},
              {"helloutau.voiceBank.sampleToolBar",
               QT_TRANSLATE_NOOP("hello::daw::MenusSettingPage", "Sample Toolbar")}}},
        };

        const WindowLayouts &layoutsOf(Editor::WindowKind kind) {
            for (const auto &layouts : windowLayouts) {
                if (layouts.kind == kind) {
                    return layouts;
                }
            }
            return windowLayouts[0];
        }

        Entry entryOf(const QModelIndex &index) {
            return index.data(Qt::UserRole).value<Entry>();
        }

        // Returns the rows from the top of the model to index.
        QList<int> pathOf(QModelIndex index) {
            QList<int> path;
            while (index.isValid()) {
                path.prepend(index.row());
                index = index.parent();
            }
            return path;
        }

        QModelIndex indexAt(const QAbstractItemModel *model, const QList<int> &path) {
            QModelIndex index;
            for (const int row : path) {
                index = model->index(row, 0, index);
                if (!index.isValid()) {
                    break;
                }
            }
            return index;
        }

    }

    /// The layouts of a kind of window with the text of each entry as the menus show it, in
    /// place of its id, and its icon.
    class LayoutNamesModel : public QIdentityProxyModel {
    public:
        LayoutNamesModel(QAK::ActionRegistry *registry, Editor::WindowKind kind, QObject *parent)
            : QIdentityProxyModel(parent), m_registry(registry), m_kind(kind) {
        }

        QVariant data(const QModelIndex &index, int role) const override {
            if (role != Qt::DisplayRole && role != Qt::DecorationRole &&
                role != Qt::ForegroundRole) {
                return QIdentityProxyModel::data(index, role);
            }
            const auto entry = entryOf(index);
            const bool marker = entry.type() == Entry::Separator || entry.type() == Entry::Stretch;
            if (role == Qt::ForegroundRole) {
                return marker ? QVariant(QPalette().brush(QPalette::Disabled, QPalette::Text))
                              : QVariant();
            }
            if (role == Qt::DecorationRole) {
                const auto info = m_registry->actionInfo(entry.id());
                const auto icon = info ? m_registry->actionIcon(QString(), entry.id(), info->icon())
                                       : std::nullopt;
                return icon ? QVariant(icon->icon()) : QVariant();
            }
            if (entry.type() == Entry::Separator) {
                return MenusSettingPage::tr("Separator");
            }
            if (entry.type() == Entry::Stretch) {
                return MenusSettingPage::tr("Stretch");
            }
            if (!index.parent().isValid()) {
                for (const auto &node : layoutsOf(m_kind).nodes) {
                    if (entry.id() == QLatin1String(node.id)) {
                        return MenusSettingPage::tr(node.name);
                    }
                }
            }
            const auto info = m_registry->actionInfo(entry.id());
            return info ? info->shortText().withoutMnemonic() : entry.id();
        }

    private:
        QAK::ActionRegistry *m_registry;
        Editor::WindowKind m_kind;
    };

    class CatalogNamesModel : public QAK::ActionCatalogModel {
    public:
        CatalogNamesModel(QAK::ActionRegistry *registry, QObject *parent)
            : QAK::ActionCatalogModel(parent), m_registry(registry) {
        }

        QVariant data(const QModelIndex &index, int role) const override {
            if (role == Qt::UserRole) {
                return QAK::ActionCatalogModel::data(index, Qt::DisplayRole);
            }
            if (role != Qt::DisplayRole && role != Qt::DecorationRole) {
                return QAK::ActionCatalogModel::data(index, role);
            }
            const auto id = QAK::ActionCatalogModel::data(index, Qt::DisplayRole).toString();
            const auto info = m_registry->actionInfo(id);
            if (role == Qt::DecorationRole) {
                if (!info) {
                    return {};
                }
                const auto icon = m_registry->actionIcon(QString(), id, info->icon());
                return icon ? QVariant(icon->icon()) : QVariant();
            }
            return info ? info->shortText().withoutMnemonic() : id;
        }

    private:
        QAK::ActionRegistry *m_registry;
    };

    // Accepts the items that can be added and match the search. A phony item is only a directory
    // of the catalog, which the recursive filtering shows if it holds an accepted item.
    class CatalogFilterModel : public QSortFilterProxyModel {
    public:
        CatalogFilterModel(QAK::ActionRegistry *registry, QObject *parent)
            : QSortFilterProxyModel(parent), m_registry(registry) {
            setRecursiveFilteringEnabled(true);
        }

    protected:
        bool filterAcceptsRow(int row, const QModelIndex &parent) const override {
            const auto source = sourceModel()->index(row, 0, parent);
            const auto info = m_registry->actionInfo(source.data(Qt::UserRole).toString());
            if (info && info->type() == QAK::ActionItemInfo::Phony) {
                return false;
            }
            return filterRegularExpression().match(source.data().toString()).hasMatch();
        }

    private:
        QAK::ActionRegistry *m_registry;
    };

    MenusSettingPage::MenusSettingPage(Editor *editor, QObject *parent)
        : SettingPage(QLatin1String(pageId), parent), m_editor(editor) {
        setTitle(tr("Menus and Toolbars"));
        setDescription(tr("The commands of the menus and the tool bars of the windows."));
        setKeywords({QStringLiteral("Menus and Toolbars"), QStringLiteral("menu"),
                     QStringLiteral("toolbar")});
    }

    MenusSettingPage::~MenusSettingPage() = default;

    QAK::ActionRegistry *MenusSettingPage::registry(Editor::WindowKind kind) const {
        return m_editor->actionRegistry(kind);
    }

    QTreeView *MenusSettingPage::tree(Editor::WindowKind kind) const {
        return m_panels[kind].tree;
    }

    Editor::WindowKind MenusSettingPage::currentKind() const {
        return m_tabs ? windowLayouts[m_tabs->currentIndex()].kind : windowLayouts[0].kind;
    }

    void MenusSettingPage::setCurrentKind(Editor::WindowKind kind) {
        for (int i = 0; m_tabs && i < int(std::size(windowLayouts)); ++i) {
            if (windowLayouts[i].kind == kind) {
                m_tabs->setCurrentIndex(i);
            }
        }
    }

    const MenusSettingPage::Panel &MenusSettingPage::current() const {
        return m_panels[currentKind()];
    }

    bool MenusSettingPage::isModified() const {
        for (const auto kind : Editor::windowKinds) {
            const auto &panel = m_panels[kind];
            if (panel.model && panel.model->actionLayouts().adjacencyMap() !=
                                   registry(kind)->layouts().adjacencyMap()) {
                return true;
            }
        }
        return false;
    }

    bool MenusSettingPage::apply(QString *error) {
        for (const auto kind : Editor::windowKinds) {
            const auto &panel = m_panels[kind];
            if (!panel.model) {
                continue;
            }
            const auto actions = registry(kind);
            actions->setLayoutChanges(actions->computeLayoutChanges(panel.model->actionLayouts()));
            actions->updateContext(QAK::AE_Layouts);
        }
        if (!m_editor->saveActionLayouts(error)) {
            return false;
        }
        Q_EMIT modifiedChanged();
        return true;
    }

    void MenusSettingPage::select(const QList<int> &path) {
        const auto &panel = current();
        const auto index = panel.names->mapFromSource(indexAt(panel.model, path));
        if (index.isValid()) {
            panel.tree->setCurrentIndex(index);
            panel.tree->scrollTo(index);
        }
        updateButtons();
    }

    bool MenusSettingPage::addEntry(const QAK::ActionLayoutEntry &entry) {
        const auto &panel = current();
        const auto model = panel.model;
        const auto at = panel.names->mapToSource(panel.tree->currentIndex());
        if (!at.isValid()) {
            return false;
        }
        const auto currentPath = pathOf(at);
        const auto type = entryOf(at).type();
        const bool container =
            !at.parent().isValid() || type == Entry::Menu || type == Entry::Group;
        const auto parent = container ? at : at.parent();
        const int row = container ? model->rowCount(at) : at.row() + 1;
        // The model resets after an edit of a menu that several containers hold, so the parent
        // is found again by its path.
        const auto parentPath = pathOf(parent);
        if (!model->insertRows(row, 1, parent)) {
            return false;
        }
        const auto inserted = model->index(row, 0, indexAt(model, parentPath));
        if (!model->setData(inserted, QVariant::fromValue(entry), Qt::UserRole)) {
            model->removeRows(row, 1, indexAt(model, parentPath));
            select(currentPath);
            return false;
        }
        select(parentPath + QList<int>{row});
        return true;
    }

    void MenusSettingPage::removeCurrent() {
        const auto &panel = current();
        const auto model = panel.model;
        const auto at = panel.names->mapToSource(panel.tree->currentIndex());
        if (!at.isValid() || !at.parent().isValid()) {
            return;
        }
        const auto parentPath = pathOf(at.parent());
        const int row = at.row();
        model->removeRows(row, 1, at.parent());
        const int count = model->rowCount(indexAt(model, parentPath));
        select(count > 0 ? parentPath + QList<int>{std::min(row, count - 1)} : parentPath);
    }

    void MenusSettingPage::moveCurrent(bool up) {
        const auto &panel = current();
        const auto model = panel.model;
        const auto at = panel.names->mapToSource(panel.tree->currentIndex());
        if (!at.isValid() || !at.parent().isValid()) {
            return;
        }
        const auto parent = at.parent();
        const auto parentPath = pathOf(parent);
        const int row = at.row();
        const int to = up ? row - 1 : row + 1;
        if (to < 0 || to >= model->rowCount(parent)) {
            return;
        }
        // The destination is the row before which the entry goes.
        if (model->moveRows(parent, row, 1, parent, up ? to : to + 1)) {
            select(parentPath + QList<int>{to});
        }
    }

    void MenusSettingPage::restoreDefaults() {
        for (const auto kind : Editor::windowKinds) {
            const auto &panel = m_panels[kind];
            panel.model->setActionLayouts(registry(kind)->defaultLayouts());
            panel.tree->expandToDepth(0);
        }
        updateButtons();
        Q_EMIT modifiedChanged();
    }

    void MenusSettingPage::updateButtons() {
        const auto &panel = current();
        const auto at = panel.names->mapToSource(panel.tree->currentIndex());
        const bool entry = at.isValid() && at.parent().isValid();
        m_add->setEnabled(at.isValid());
        m_addSeparator->setEnabled(at.isValid());
        m_remove->setEnabled(entry);
        m_up->setEnabled(entry && at.row() > 0);
        m_down->setEnabled(entry && at.row() + 1 < panel.model->rowCount(at.parent()));
    }

    // Asks for an action, a menu or a group of the registry of the current tab in a list that the
    // user filters by its text, and adds it in its declared form.
    void MenusSettingPage::askAction() {
        const auto actions = registry(currentKind());
        QDialog dialog(m_tabs->window());
        dialog.setWindowTitle(tr("Add Action"));
        auto search = new QLineEdit();
        search->setPlaceholderText(tr("Search"));
        search->setClearButtonEnabled(true);
        auto catalog = new CatalogNamesModel(actions, &dialog);
        catalog->setCatalog(actions->catalog());
        auto filter = new CatalogFilterModel(actions, &dialog);
        filter->setSourceModel(catalog);
        filter->setFilterCaseSensitivity(Qt::CaseInsensitive);
        filter->setFilterKeyColumn(0);
        auto tree = new QTreeView();
        tree->setObjectName(QStringLiteral("actions"));
        tree->setModel(filter);
        tree->setHeaderHidden(true);
        tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tree->expandAll();
        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        auto layout = new QVBoxLayout(&dialog);
        layout->addWidget(search);
        layout->addWidget(tree);
        layout->addWidget(buttons);
        connect(search, &QLineEdit::textChanged, &dialog, [filter, tree](const QString &text) {
            filter->setFilterFixedString(text);
            tree->expandAll();
        });
        // Returns the layout entry of the current item in its declared form, or std::nullopt for
        // a directory of the catalog.
        const auto currentEntry = [&]() -> std::optional<Entry> {
            const auto id = filter->mapToSource(tree->currentIndex()).data(Qt::UserRole).toString();
            const auto info = actions->actionInfo(id);
            if (!info) {
                return std::nullopt;
            }
            switch (info->type()) {
                case QAK::ActionItemInfo::Action:
                    return Entry(id, Entry::Action);
                case QAK::ActionItemInfo::Menu:
                    return Entry(id, Entry::Menu);
                case QAK::ActionItemInfo::Group:
                    return Entry(id, Entry::Group);
                case QAK::ActionItemInfo::Phony:
                    break;
            }
            return std::nullopt;
        };
        const auto acceptAction = [&] {
            if (currentEntry()) {
                dialog.accept();
            }
        };
        connect(tree, &QTreeView::doubleClicked, &dialog, [acceptAction] { acceptAction(); });
        connect(buttons, &QDialogButtonBox::accepted, &dialog, [acceptAction] { acceptAction(); });
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        if (!addEntry(*currentEntry())) {
            QMessageBox::warning(m_tabs->window(), tr("Add Action"), tr("%1 cannot be added here.")
                                                                  .arg(tree->currentIndex().data().toString()));
        }
    }

    QWidget *MenusSettingPage::createWidget() {
        auto widget = new QWidget();
        m_tabs = new QTabWidget();
        m_tabs->setObjectName(QStringLiteral("windows"));
        for (const auto &window : windowLayouts) {
            const auto actions = registry(window.kind);
            auto &panel = m_panels[window.kind];
            panel.model = new QAK::ActionLayoutsModel(widget);
            panel.model->setRegistry(actions);
            QVector<Entry> nodes;
            for (const auto &node : window.nodes) {
                nodes.push_back(Entry(QLatin1String(node.id), Entry::Menu));
            }
            panel.model->setTopLevelNodes(nodes);
            panel.model->setActionLayouts(actions->layouts());
            panel.names = new LayoutNamesModel(actions, window.kind, widget);
            panel.names->setSourceModel(panel.model);

            panel.tree = new QTreeView();
            panel.tree->setObjectName(QStringLiteral("layouts"));
            panel.tree->setHeaderHidden(true);
            panel.tree->setModel(panel.names);
            panel.tree->expandToDepth(0);
            m_tabs->addTab(panel.tree, tr(window.name));
        }

        m_add = new QPushButton(tr("&Add Action..."));
        m_add->setObjectName(QStringLiteral("add"));
        m_addSeparator = new QPushButton(tr("Add &Separator"));
        m_addSeparator->setObjectName(QStringLiteral("addSeparator"));
        m_remove = new QPushButton(tr("&Remove"));
        m_remove->setObjectName(QStringLiteral("remove"));
        m_up = new QPushButton(tr("Move &Up"));
        m_up->setObjectName(QStringLiteral("moveUp"));
        m_down = new QPushButton(tr("Move &Down"));
        m_down->setObjectName(QStringLiteral("moveDown"));
        auto restore = new QPushButton(tr("Restore &Defaults"));
        restore->setObjectName(QStringLiteral("restoreDefaults"));
        auto buttons = new QVBoxLayout();
        for (const auto button :
             {m_add.data(), m_addSeparator.data(), m_remove.data(), m_up.data(), m_down.data()}) {
            buttons->addWidget(button);
        }
        buttons->addStretch();
        buttons->addWidget(restore);

        auto layout = new QHBoxLayout(widget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_tabs, 1);
        layout->addLayout(buttons);

        connect(m_add, &QPushButton::clicked, this, [this] { askAction(); });
        connect(m_addSeparator, &QPushButton::clicked, this,
                [this] { addEntry(Entry({}, Entry::Separator)); });
        connect(m_remove, &QPushButton::clicked, this, [this] { removeCurrent(); });
        connect(m_up, &QPushButton::clicked, this, [this] { moveCurrent(true); });
        connect(m_down, &QPushButton::clicked, this, [this] { moveCurrent(false); });
        connect(restore, &QPushButton::clicked, this, [this] { restoreDefaults(); });
        connect(m_tabs, &QTabWidget::currentChanged, this, [this] { updateButtons(); });
        for (const auto kind : Editor::windowKinds) {
            const auto &panel = m_panels[kind];
            connect(panel.tree->selectionModel(), &QItemSelectionModel::currentChanged, this,
                    [this] { updateButtons(); });
            for (const auto signal :
                 {&QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved}) {
                connect(panel.model, signal, this, &SettingPage::modifiedChanged);
            }
            connect(panel.model, &QAbstractItemModel::rowsMoved, this,
                    &SettingPage::modifiedChanged);
            connect(panel.model, &QAbstractItemModel::dataChanged, this,
                    &SettingPage::modifiedChanged);
            connect(panel.model, &QAbstractItemModel::modelReset, this,
                    &SettingPage::modifiedChanged);
        }
        updateButtons();
        return widget;
    }

}
