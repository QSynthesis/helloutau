#include "MenusSettingPage.h"

#include <QtCore/QIdentityProxyModel>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>

#include <QAKCore/actionlayoutsmodel.h>
#include <QAKCore/actionregistry.h>

#include <helloutau/Editor/Editor.h>

namespace hello::daw {

    namespace {

        using Entry = QAK::ActionLayoutEntry;

        // The menu bars and the tool bars of the windows, which the tree shows at the top
        const std::pair<const char *, const char *> topLevelNodes[] = {
            {"helloutau.mainMenu",
             QT_TRANSLATE_NOOP("hello::daw::MenusSettingPage", "Project Window: Main Menu")   },
            {"helloutau.mainToolBar",
             QT_TRANSLATE_NOOP("hello::daw::MenusSettingPage", "Project Window: Main Toolbar")},
            {"helloutau.voiceBankMenu",
             QT_TRANSLATE_NOOP("hello::daw::MenusSettingPage", "Voice Bank Window: Main Menu")},
            {"helloutau.voiceBank.sampleToolBar",
             QT_TRANSLATE_NOOP("hello::daw::MenusSettingPage",
             "Voice Bank Window: Sample Toolbar")                                             },
        };

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

    /// The layouts with the text of each entry as the menus show it, in place of its id, and
    /// its icon.
    class LayoutNamesModel : public QIdentityProxyModel {
    public:
        LayoutNamesModel(QAK::ActionRegistry *registry, QObject *parent)
            : QIdentityProxyModel(parent), m_registry(registry) {
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
                for (const auto &[id, name] : topLevelNodes) {
                    if (entry.id() == QLatin1String(id)) {
                        return MenusSettingPage::tr(name);
                    }
                }
            }
            const auto info = m_registry->actionInfo(entry.id());
            return info ? info->shortText().withoutMnemonic() : entry.id();
        }

    private:
        QAK::ActionRegistry *m_registry;
    };

    MenusSettingPage::MenusSettingPage(Editor *editor, QObject *parent)
        : SettingPage(QLatin1String(pageId), parent), m_editor(editor),
          m_registry(editor->actionRegistry()) {
        setTitle(tr("Menus and Toolbars"));
        setDescription(tr("The commands of the menus and the tool bars of the windows."));
        setKeywords({QStringLiteral("Menus and Toolbars"), QStringLiteral("menu"),
                     QStringLiteral("toolbar")});
    }

    MenusSettingPage::~MenusSettingPage() = default;

    QTreeView *MenusSettingPage::tree() const {
        return m_tree;
    }

    bool MenusSettingPage::isModified() const {
        return m_model &&
               m_model->actionLayouts().adjacencyMap() != m_registry->layouts().adjacencyMap();
    }

    bool MenusSettingPage::apply(QString *error) {
        m_registry->setLayoutChanges(m_registry->computeLayoutChanges(m_model->actionLayouts()));
        m_registry->updateContext(QAK::AE_Layouts);
        if (!m_editor->saveActionLayouts(error)) {
            return false;
        }
        Q_EMIT modifiedChanged();
        return true;
    }

    void MenusSettingPage::select(const QList<int> &path) {
        const auto index = m_names->mapFromSource(indexAt(m_model, path));
        if (index.isValid()) {
            m_tree->setCurrentIndex(index);
            m_tree->scrollTo(index);
        }
        updateButtons();
    }

    bool MenusSettingPage::addEntry(const QAK::ActionLayoutEntry &entry) {
        const auto current = m_names->mapToSource(m_tree->currentIndex());
        if (!current.isValid()) {
            return false;
        }
        const auto currentPath = pathOf(current);
        const auto type = entryOf(current).type();
        const bool container =
            !current.parent().isValid() || type == Entry::Menu || type == Entry::Group;
        const auto parent = container ? current : current.parent();
        const int row = container ? m_model->rowCount(current) : current.row() + 1;
        // The model resets after an edit of a menu that several containers hold, so the parent
        // is found again by its path.
        const auto parentPath = pathOf(parent);
        if (!m_model->insertRows(row, 1, parent)) {
            return false;
        }
        const auto inserted = m_model->index(row, 0, indexAt(m_model, parentPath));
        if (!m_model->setData(inserted, QVariant::fromValue(entry), Qt::UserRole)) {
            m_model->removeRows(row, 1, indexAt(m_model, parentPath));
            select(currentPath);
            return false;
        }
        select(parentPath + QList<int>{row});
        return true;
    }

    void MenusSettingPage::removeCurrent() {
        const auto current = m_names->mapToSource(m_tree->currentIndex());
        if (!current.isValid() || !current.parent().isValid()) {
            return;
        }
        const auto parentPath = pathOf(current.parent());
        const int row = current.row();
        m_model->removeRows(row, 1, current.parent());
        const int count = m_model->rowCount(indexAt(m_model, parentPath));
        select(count > 0 ? parentPath + QList<int>{std::min(row, count - 1)} : parentPath);
    }

    void MenusSettingPage::moveCurrent(bool up) {
        const auto current = m_names->mapToSource(m_tree->currentIndex());
        if (!current.isValid() || !current.parent().isValid()) {
            return;
        }
        const auto parent = current.parent();
        const auto parentPath = pathOf(parent);
        const int row = current.row();
        const int to = up ? row - 1 : row + 1;
        if (to < 0 || to >= m_model->rowCount(parent)) {
            return;
        }
        // The destination is the row before which the entry goes.
        if (m_model->moveRows(parent, row, 1, parent, up ? to : to + 1)) {
            select(parentPath + QList<int>{to});
        }
    }

    void MenusSettingPage::restoreDefaults() {
        m_model->setActionLayouts(m_registry->defaultLayouts());
        m_tree->expandToDepth(0);
        updateButtons();
        Q_EMIT modifiedChanged();
    }

    void MenusSettingPage::updateButtons() {
        const auto current = m_names->mapToSource(m_tree->currentIndex());
        const bool entry = current.isValid() && current.parent().isValid();
        m_add->setEnabled(current.isValid());
        m_addSeparator->setEnabled(current.isValid());
        m_remove->setEnabled(entry);
        m_up->setEnabled(entry && current.row() > 0);
        m_down->setEnabled(entry && current.row() + 1 < m_model->rowCount(current.parent()));
    }

    // Asks for an action in a list that the user filters by its text, and adds it.
    void MenusSettingPage::askAction() {
        QDialog dialog(m_tree->window());
        dialog.setWindowTitle(tr("Add Action"));
        auto search = new QLineEdit();
        search->setPlaceholderText(tr("Search"));
        search->setClearButtonEnabled(true);
        auto list = new QListWidget();
        list->setObjectName(QStringLiteral("actions"));
        for (const auto &id : m_registry->actionIds()) {
            const auto info = m_registry->actionInfo(id);
            if (!info || info->type() != QAK::ActionItemInfo::Action) {
                continue;
            }
            const auto category = info->category().withoutMnemonic();
            const auto text = info->text().withoutMnemonic();
            auto item =
                new QListWidgetItem(category.isEmpty() ? text : category + u": " + text, list);
            item->setData(Qt::UserRole, id);
            if (const auto icon = m_registry->actionIcon(QString(), id, info->icon())) {
                item->setIcon(icon->icon());
            }
        }
        list->sortItems();
        list->setCurrentRow(0);
        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        auto layout = new QVBoxLayout(&dialog);
        layout->addWidget(search);
        layout->addWidget(list);
        layout->addWidget(buttons);
        connect(search, &QLineEdit::textChanged, &dialog, [list](const QString &text) {
            for (int i = 0; i < list->count(); ++i) {
                list->item(i)->setHidden(
                    !list->item(i)->text().contains(text, Qt::CaseInsensitive));
            }
        });
        connect(list, &QListWidget::itemDoubleClicked, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted || !list->currentItem()) {
            return;
        }
        const auto id = list->currentItem()->data(Qt::UserRole).toString();
        if (!addEntry(Entry(id, Entry::Action))) {
            QMessageBox::warning(m_tree->window(), tr("Add Action"),
                                 tr("%1 cannot be added here.").arg(list->currentItem()->text()));
        }
    }

    QWidget *MenusSettingPage::createWidget() {
        auto widget = new QWidget();
        m_model = new QAK::ActionLayoutsModel(widget);
        m_model->setRegistry(m_registry);
        QVector<Entry> nodes;
        for (const auto &[id, name] : topLevelNodes) {
            nodes.push_back(Entry(QLatin1String(id), Entry::Menu));
        }
        m_model->setTopLevelNodes(nodes);
        m_model->setActionLayouts(m_registry->layouts());
        m_names = new LayoutNamesModel(m_registry, widget);
        m_names->setSourceModel(m_model);

        m_tree = new QTreeView();
        m_tree->setObjectName(QStringLiteral("layouts"));
        m_tree->setHeaderHidden(true);
        m_tree->setModel(m_names);
        m_tree->expandToDepth(0);

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
        layout->addWidget(m_tree, 1);
        layout->addLayout(buttons);

        connect(m_add, &QPushButton::clicked, this, [this] { askAction(); });
        connect(m_addSeparator, &QPushButton::clicked, this,
                [this] { addEntry(Entry({}, Entry::Separator)); });
        connect(m_remove, &QPushButton::clicked, this, [this] { removeCurrent(); });
        connect(m_up, &QPushButton::clicked, this, [this] { moveCurrent(true); });
        connect(m_down, &QPushButton::clicked, this, [this] { moveCurrent(false); });
        connect(restore, &QPushButton::clicked, this, [this] { restoreDefaults(); });
        connect(m_tree->selectionModel(), &QItemSelectionModel::currentChanged, this,
                [this] { updateButtons(); });
        for (const auto signal :
             {&QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved}) {
            connect(m_model, signal, this, &SettingPage::modifiedChanged);
        }
        connect(m_model, &QAbstractItemModel::rowsMoved, this, &SettingPage::modifiedChanged);
        connect(m_model, &QAbstractItemModel::dataChanged, this, &SettingPage::modifiedChanged);
        connect(m_model, &QAbstractItemModel::modelReset, this, &SettingPage::modifiedChanged);
        updateButtons();
        return widget;
    }

}
