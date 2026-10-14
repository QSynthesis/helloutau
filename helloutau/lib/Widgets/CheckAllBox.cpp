#include "CheckAllBox.h"

#include <QtCore/QEvent>
#include <QtWidgets/QAbstractItemView>

namespace hello::daw {

    namespace {

        bool checkable(const QAbstractItemModel *model, const QModelIndex &index) {
            const auto flags = model->flags(index);
            return flags.testFlag(Qt::ItemIsUserCheckable) && flags.testFlag(Qt::ItemIsEnabled);
        }

    }

    CheckAllBox::CheckAllBox(QAbstractItemView *view, int column, QWidget *parent)
        : QCheckBox(tr("All"), parent), m_view(view), m_column(column) {
        setTristate(true);
        const auto model = view->model();
        connect(model, &QAbstractItemModel::dataChanged, this, &CheckAllBox::refresh);
        connect(model, &QAbstractItemModel::rowsInserted, this, &CheckAllBox::refresh);
        connect(model, &QAbstractItemModel::rowsRemoved, this, &CheckAllBox::refresh);
        connect(model, &QAbstractItemModel::modelReset, this, &CheckAllBox::refresh);
        connect(model, &QAbstractItemModel::layoutChanged, this, &CheckAllBox::refresh);
        view->installEventFilter(this);
        refresh();
    }

    CheckAllBox::~CheckAllBox() = default;

    // A partially checked box becomes checked, as the click checks every item.
    void CheckAllBox::nextCheckState() {
        if (!m_view) {
            return;
        }
        const auto state = checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked;
        const auto model = m_view->model();
        m_applying = true;
        for (int row = 0; row < model->rowCount(); ++row) {
            const auto index = model->index(row, m_column);
            if (checkable(model, index) && index.data(Qt::CheckStateRole).toInt() != state) {
                model->setData(index, state, Qt::CheckStateRole);
            }
        }
        m_applying = false;
        refresh();
    }

    bool CheckAllBox::eventFilter(QObject *watched, QEvent *event) {
        if (watched == m_view && event->type() == QEvent::EnabledChange) {
            refresh();
        }
        return QCheckBox::eventFilter(watched, event);
    }

    void CheckAllBox::refresh() {
        if (m_applying || !m_view) {
            return;
        }
        const auto model = m_view->model();
        int items = 0;
        int checked = 0;
        for (int row = 0; row < model->rowCount(); ++row) {
            const auto index = model->index(row, m_column);
            if (checkable(model, index)) {
                ++items;
                checked += index.data(Qt::CheckStateRole).toInt() == Qt::Checked ? 1 : 0;
            }
        }
        setEnabled(items > 0 && m_view->isEnabled());
        setCheckState(checked == 0       ? Qt::Unchecked
                      : checked == items ? Qt::Checked
                                         : Qt::PartiallyChecked);
    }

}
