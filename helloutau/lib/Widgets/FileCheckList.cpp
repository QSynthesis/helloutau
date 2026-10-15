#include "FileCheckList.h"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include "CheckAllBox.h"

namespace hello::daw {

    FileCheckList::FileCheckList(const QStringList &headers, QWidget *parent)
        : QWidget(parent), m_tree(new QTreeWidget(this)),
          m_folders(m_tree, Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate) {
        m_tree->setHeaderLabels(headers);
        m_tree->setSelectionMode(QAbstractItemView::NoSelection);
        m_tree->header()->setSectionResizeMode(QHeaderView::Interactive);
        m_tree->header()->setStretchLastSection(false);
        m_checkAll = new CheckAllBox(m_tree, 0, this);
        m_buttons = new QWidget(this);
        auto buttons = new QHBoxLayout(m_buttons);
        buttons->setContentsMargins(0, 0, 0, 0);

        auto top = new QHBoxLayout();
        top->addWidget(m_checkAll);
        top->addWidget(m_buttons);
        top->addStretch();
        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addLayout(top);
        layout->addWidget(m_tree, 1);

        connect(m_tree, &QTreeWidget::itemChanged, this, [this] {
            // QTreeWidgetItem emits no change of a folder whose check state is set to its stored
            // value, although its rows change. The whole view is therefore repainted.
            m_tree->viewport()->update();
            if (!m_applying) {
                Q_EMIT checkedChanged();
            }
        });
    }

    FileCheckList::~FileCheckList() {
        // The box is deleted first. Otherwise the tree, created before it, is destroyed first,
        // and the removal of its rows unchecks the box during the destruction of this widget,
        // when QAbstractButton queries the buttons of a null parent.
        delete m_checkAll;
    }

    int FileCheckList::addRow(const std::filesystem::path &path, const QStringList &texts) {
        auto item = m_folders.addFile(path);
        for (qsizetype i = 0; i < texts.size(); ++i) {
            item->setText(int(i + 1), texts.at(i));
        }
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, Qt::Unchecked);
        for (auto it = m_alignments.cbegin(); it != m_alignments.cend(); ++it) {
            item->setTextAlignment(it.key(), it.value());
        }
        m_rows.push_back(item);
        return int(m_rows.size() - 1);
    }

    void FileCheckList::setColumnAlignment(int column, Qt::Alignment alignment) {
        m_alignments.insert(column, alignment);
        m_tree->headerItem()->setTextAlignment(column, alignment);
        for (const auto item : std::as_const(m_rows)) {
            item->setTextAlignment(column, alignment);
        }
    }

    int FileCheckList::rowCount() const {
        return int(m_rows.size());
    }

    QTreeWidgetItem *FileCheckList::itemOf(int row) const {
        return m_rows.at(row);
    }

    void FileCheckList::setText(int row, int column, const QString &text) {
        m_rows.at(row)->setText(column, text);
    }

    bool FileCheckList::isChecked(int row) const {
        return m_rows.at(row)->checkState(0) == Qt::Checked;
    }

    void FileCheckList::setChecked(int row, bool checked) {
        m_rows.at(row)->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
    }

    void FileCheckList::checkWhere(const std::function<bool(int row)> &condition) {
        m_applying = true;
        for (int row = 0; row < rowCount(); ++row) {
            setChecked(row, condition(row));
        }
        m_applying = false;
        Q_EMIT checkedChanged();
    }

    QList<int> FileCheckList::checkedRows() const {
        QList<int> rows;
        for (int row = 0; row < rowCount(); ++row) {
            if (isChecked(row)) {
                rows.push_back(row);
            }
        }
        return rows;
    }

    QPushButton *FileCheckList::addCheckButton(const QString &text,
                                               std::function<bool(int row)> condition) {
        auto button = new QPushButton(text, m_buttons);
        m_buttons->layout()->addWidget(button);
        connect(button, &QPushButton::clicked, this,
                [this, condition = std::move(condition)] { checkWhere(condition); });
        return button;
    }

    QTreeWidget *FileCheckList::tree() const {
        return m_tree;
    }

    CheckAllBox *FileCheckList::checkAllBox() const {
        return m_checkAll;
    }

    void FileCheckList::showEvent(QShowEvent *event) {
        QWidget::showEvent(event);
        if (m_fitted) {
            return;
        }
        // ResizeToContents would fit the columns as well, but it prevents resizing with the
        // mouse.
        m_fitted = true;
        for (int column = 0; column < m_tree->columnCount(); ++column) {
            m_tree->resizeColumnToContents(column);
        }
    }

}
