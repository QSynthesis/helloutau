#include "RegionDialog.h"

#include <algorithm>

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

namespace hello::daw {

    RegionDialog::RegionDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle(tr("Regions"));
        resize(360, 320);

        m_list = new QTreeWidget();
        m_list->setRootIsDecorated(false);
        m_list->setUniformRowHeights(true);
        m_list->setHeaderLabels({tr("Name"), tr("Notes")});
        m_list->header()->setStretchLastSection(false);
        m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        connect(m_list, &QTreeWidget::currentItemChanged, this, &RegionDialog::updateButtons);
        connect(m_list, &QTreeWidget::itemActivated, this, [this] {
            if (const auto region = currentRegion()) {
                Q_EMIT goToRequested(*region);
            }
        });

        m_goTo = new QPushButton(tr("&Go To"));
        m_remove = new QPushButton(tr("&Remove"));
        connect(m_goTo, &QPushButton::clicked, this, [this] {
            if (const auto region = currentRegion()) {
                Q_EMIT goToRequested(*region);
            }
        });
        connect(m_remove, &QPushButton::clicked, this, [this] {
            if (const auto region = currentRegion()) {
                Q_EMIT removeRequested(*region);
            }
        });
        auto actions = new QVBoxLayout();
        actions->addWidget(m_goTo);
        actions->addWidget(m_remove);
        actions->addStretch();

        auto content = new QHBoxLayout();
        content->addWidget(m_list, 1);
        content->addLayout(actions);

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Close);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto layout = new QVBoxLayout(this);
        layout->addLayout(content, 1);
        layout->addWidget(buttons);
        updateButtons();
    }

    RegionDialog::~RegionDialog() = default;

    QList<kit::Region> RegionDialog::regions() const {
        return m_regions;
    }

    void RegionDialog::setRegions(const QList<kit::Region> &regions) {
        const auto current = currentRegion();
        // The row of the current region before the change, so that the next region becomes
        // current after a removal
        const int row = m_list->currentIndex().row();
        m_regions = regions;
        m_list->clear();
        for (const auto &region : regions) {
            // Notes are numbered from 1 for the user.
            const auto notes = region.first == region.last
                                   ? QString::number(region.first + 1)
                                   : tr("%1–%2").arg(region.first + 1).arg(region.last + 1);
            m_list->addTopLevelItem(new QTreeWidgetItem({region.name, notes}));
        }
        if (current && regions.contains(*current)) {
            setCurrentRegion(*current);
        } else if (!regions.isEmpty()) {
            m_list->setCurrentItem(
                m_list->topLevelItem(std::clamp(row, 0, int(regions.size()) - 1)));
        }
        updateButtons();
    }

    std::optional<kit::Region> RegionDialog::currentRegion() const {
        const int row = m_list->currentIndex().row();
        if (row < 0 || row >= m_regions.size()) {
            return std::nullopt;
        }
        return m_regions.at(row);
    }

    void RegionDialog::setCurrentRegion(const kit::Region &region) {
        if (const auto row = m_regions.indexOf(region); row >= 0) {
            m_list->setCurrentItem(m_list->topLevelItem(int(row)));
        }
    }

    QTreeWidget *RegionDialog::list() const {
        return m_list;
    }

    QPushButton *RegionDialog::goToButton() const {
        return m_goTo;
    }

    QPushButton *RegionDialog::removeButton() const {
        return m_remove;
    }

    void RegionDialog::updateButtons() {
        const bool current = currentRegion().has_value();
        m_goTo->setEnabled(current);
        m_remove->setEnabled(current);
    }

}
