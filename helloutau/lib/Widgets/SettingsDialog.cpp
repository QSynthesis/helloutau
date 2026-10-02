#include "SettingsDialog.h"

#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QDialogButtonBox>
#include <QtGui/QPalette>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include "SettingPage.h"

namespace hello::daw {

    namespace {

        constexpr int PageRole = Qt::UserRole;

        // The text a control shows, without the mnemonic marks
        QString textOf(const QWidget *widget) {
            QString text;
            if (const auto label = qobject_cast<const QLabel *>(widget)) {
                text = label->text();
            } else if (const auto button = qobject_cast<const QAbstractButton *>(widget)) {
                text = button->text();
            } else if (const auto group = qobject_cast<const QGroupBox *>(widget)) {
                text = group->title();
            }
            return text.remove(QLatin1Char('&'));
        }

        // The controls of widget whose text contains word
        QList<QWidget *> controlsMatching(QWidget *widget, const QString &word) {
            QList<QWidget *> found;
            if (!widget || word.isEmpty()) {
                return found;
            }
            for (const auto child : widget->findChildren<QWidget *>()) {
                if (textOf(child).contains(word, Qt::CaseInsensitive)) {
                    found.push_back(child);
                }
            }
            return found;
        }

        bool matches(SettingPage *page, const QString &word) {
            const auto contains = [&word](const QString &text) {
                return text.contains(word, Qt::CaseInsensitive);
            };
            if (contains(page->title()) || contains(page->description())) {
                return true;
            }
            for (const auto &keyword : page->keywords()) {
                if (contains(keyword)) {
                    return true;
                }
            }
            return !controlsMatching(page->widget(), word).isEmpty();
        }

    }

    SettingsDialog::SettingsDialog(SettingCatalog *catalog, QWidget *parent)
        : QDialog(parent), m_catalog(catalog) {
        setWindowTitle(tr("Settings"));

        m_search = new QLineEdit();
        m_search->setPlaceholderText(tr("Search settings"));
        m_search->setClearButtonEnabled(true);
        m_tree = new QTreeWidget();
        m_tree->setHeaderHidden(true);

        auto left = new QWidget();
        auto leftLayout = new QVBoxLayout(left);
        leftLayout->setContentsMargins({});
        leftLayout->addWidget(m_search);
        leftLayout->addWidget(m_tree, 1);

        m_title = new QLabel();
        auto font = m_title->font();
        font.setPointSizeF(font.pointSizeF() * 1.2);
        font.setBold(true);
        m_title->setFont(font);
        m_description = new QLabel();
        m_description->setWordWrap(true);
        m_message = new QLabel();
        m_message->setWordWrap(true);
        auto messagePalette = m_message->palette();
        messagePalette.setColor(QPalette::WindowText, QColor(Qt::red));
        m_message->setPalette(messagePalette);
        m_message->hide();
        m_stack = new QStackedWidget();

        auto right = new QWidget();
        auto rightLayout = new QVBoxLayout(right);
        rightLayout->setContentsMargins({});
        rightLayout->addWidget(m_title);
        rightLayout->addWidget(m_description);
        rightLayout->addWidget(m_message);
        rightLayout->addWidget(m_stack, 1);

        auto splitter = new QSplitter();
        splitter->addWidget(left);
        splitter->addWidget(right);
        splitter->setStretchFactor(1, 1);
        splitter->setSizes({220, 580});

        m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
                                         QDialogButtonBox::Apply);
        connect(m_buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
        connect(m_buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
        connect(applyButton(), &QPushButton::clicked, this, [this] { apply(); });

        auto layout = new QVBoxLayout(this);
        layout->addWidget(splitter, 1);
        layout->addWidget(m_buttons);

        addItems(m_catalog->pages(), nullptr);
        m_tree->expandAll();
        connect(m_tree, &QTreeWidget::currentItemChanged, this,
                [this](QTreeWidgetItem *item) { showPage(pageOf(item)); });
        connect(m_search, &QLineEdit::textChanged, this, [this] { search(); });
        if (m_tree->topLevelItemCount() > 0) {
            m_tree->setCurrentItem(m_tree->topLevelItem(0));
        }
        updateModified();
        resize(820, 560);
    }

    SettingsDialog::~SettingsDialog() {
        releaseAll();
    }

    bool SettingsDialog::selectPage(const QString &id) {
        const auto page = m_catalog->page(id);
        const auto item = m_items.value(page);
        if (!item) {
            return false;
        }
        m_tree->setCurrentItem(item);
        return true;
    }

    SettingPage *SettingsDialog::currentPage() const {
        return pageOf(m_tree->currentItem());
    }

    bool SettingsDialog::apply() {
        for (const auto page : m_catalog->allPages()) {
            if (!page->hasWidget() || !page->isModified()) {
                continue;
            }
            QString error;
            if (!page->apply(&error)) {
                m_tree->setCurrentItem(m_items.value(page));
                m_message->setText(error.isEmpty() ? tr("This page could not be applied.") : error);
                m_message->show();
                return false;
            }
        }
        m_message->hide();
        updateModified();
        Q_EMIT applied();
        return true;
    }

    void SettingsDialog::accept() {
        if (!apply()) {
            return;
        }
        releaseAll();
        QDialog::accept();
    }

    void SettingsDialog::reject() {
        releaseAll();
        QDialog::reject();
    }

    QLineEdit *SettingsDialog::searchBox() const {
        return m_search;
    }

    QTreeWidget *SettingsDialog::tree() const {
        return m_tree;
    }

    QPushButton *SettingsDialog::applyButton() const {
        return m_buttons->button(QDialogButtonBox::Apply);
    }

    QList<SettingPage *> SettingsDialog::visiblePages() const {
        QList<SettingPage *> pages;
        for (const auto page : m_catalog->allPages()) {
            const auto item = m_items.value(page);
            if (item && !item->isHidden()) {
                pages.push_back(page);
            }
        }
        return pages;
    }

    QList<QWidget *> SettingsDialog::highlighted() const {
        QList<QWidget *> widgets;
        for (const auto &[widget, palette] : m_highlighted) {
            if (widget) {
                widgets.push_back(widget);
            }
        }
        return widgets;
    }

    QLabel *SettingsDialog::messageLabel() const {
        return m_message;
    }

    void SettingsDialog::addItems(const QList<SettingPage *> &pages, QTreeWidgetItem *parent) {
        for (const auto page : pages) {
            auto item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
            item->setText(0, page->title());
            item->setToolTip(0, page->description());
            item->setData(0, PageRole, QVariant::fromValue<void *>(page));
            m_items.insert(page, item);
            connect(page, &SettingPage::modifiedChanged, this, [this] { updateModified(); });
            addItems(page->pages(), item);
        }
    }

    SettingPage *SettingsDialog::pageOf(QTreeWidgetItem *item) const {
        return item ? static_cast<SettingPage *>(item->data(0, PageRole).value<void *>()) : nullptr;
    }

    void SettingsDialog::showPage(SettingPage *page) {
        // The page is taken out, not deleted: it keeps its edits until the dialog closes.
        while (m_stack->count() > 0) {
            m_stack->removeWidget(m_stack->widget(0));
        }
        delete m_category;
        m_message->hide();
        if (!page) {
            m_title->clear();
            m_description->clear();
            return;
        }

        QStringList path;
        for (auto p = page; p; p = p->parentPage()) {
            path.prepend(p->title());
        }
        m_title->setText(path.join(QStringLiteral(" > ")));
        m_description->setText(page->description());
        m_description->setVisible(!page->description().isEmpty());

        if (const auto widget = page->widget()) {
            m_stack->addWidget(widget);
            m_stack->setCurrentWidget(widget);
        } else {
            // A category shows the links to its pages.
            m_category = new QWidget();
            auto layout = new QVBoxLayout(m_category);
            for (const auto child : page->pages()) {
                auto link = new QPushButton(child->title());
                link->setFlat(true);
                link->setToolTip(child->description());
                connect(link, &QPushButton::clicked, this,
                        [this, child] { m_tree->setCurrentItem(m_items.value(child)); });
                layout->addWidget(link, 0, Qt::AlignLeft);
            }
            layout->addStretch(1);
            m_stack->addWidget(m_category);
            m_stack->setCurrentWidget(m_category);
        }
        highlight();
    }

    void SettingsDialog::updateModified() {
        bool any = false;
        for (auto it = m_items.cbegin(); it != m_items.cend(); ++it) {
            const bool modified = it.key()->hasWidget() && it.key()->isModified();
            auto font = it.value()->font(0);
            font.setBold(modified);
            it.value()->setFont(0, font);
            any = any || modified;
        }
        applyButton()->setEnabled(any);
    }

    void SettingsDialog::search() {
        const auto word = m_search->text().trimmed();
        if (word.isEmpty()) {
            for (const auto item : std::as_const(m_items)) {
                item->setHidden(false);
            }
        } else {
            for (const auto item : std::as_const(m_items)) {
                item->setHidden(true);
            }
            for (const auto page : m_catalog->allPages()) {
                if (!matches(page, word)) {
                    continue;
                }
                for (auto item = m_items.value(page); item && item->isHidden();
                     item = item->parent()) {
                    item->setHidden(false);
                }
            }
            m_tree->expandAll();
        }

        const auto pages = visiblePages();
        if (pages.isEmpty()) {
            m_message->setText(tr("No matching settings"));
            m_message->show();
            return;
        }
        m_message->hide();
        const auto current = m_tree->currentItem();
        if (!current || current->isHidden()) {
            // The first page that matches rather than a parent shown for it
            SettingPage *first = pages.first();
            for (const auto page : pages) {
                if (matches(page, word)) {
                    first = page;
                    break;
                }
            }
            m_tree->setCurrentItem(m_items.value(first));
        } else {
            highlight();
        }
    }

    void SettingsDialog::highlight() {
        for (const auto &[widget, palette] : std::as_const(m_highlighted)) {
            if (widget) {
                widget->setPalette(palette);
                widget->setAutoFillBackground(false);
            }
        }
        m_highlighted.clear();
        const auto page = currentPage();
        if (!page || !page->hasWidget()) {
            return;
        }
        const auto color = palette().color(QPalette::Highlight);
        for (const auto widget : controlsMatching(page->widget(), m_search->text().trimmed())) {
            m_highlighted.push_back({widget, widget->palette()});
            auto marked = widget->palette();
            auto background = color;
            background.setAlphaF(0.35f);
            marked.setColor(widget->backgroundRole(), background);
            widget->setPalette(marked);
            widget->setAutoFillBackground(true);
        }
    }

    void SettingsDialog::releaseAll() {
        while (m_stack->count() > 0) {
            m_stack->removeWidget(m_stack->widget(0));
        }
        m_highlighted.clear();
        for (const auto page : m_catalog->allPages()) {
            page->release();
        }
    }

}
