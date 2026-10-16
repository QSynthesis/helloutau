#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QTreeWidget>

#include <helloutau/Widgets/CheckAllBox.h>

using namespace hello::daw;

namespace {

    QListWidgetItem *checkableItem(QListWidget &list, Qt::CheckState state) {
        auto item = new QListWidgetItem(QStringLiteral("item"), &list);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        item->setCheckState(state);
        return item;
    }

}

class test_CheckAllBox : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The box is partially checked while some checkable items are checked, and checked or
    // unchecked while all or none are. An item that is not checkable does not count.
    void the_box_follows_the_checkable_items() {
        QListWidget list;
        const auto first = checkableItem(list, Qt::Checked);
        const auto second = checkableItem(list, Qt::Unchecked);
        auto fixed = new QListWidgetItem(QStringLiteral("fixed"), &list);
        fixed->setFlags(Qt::NoItemFlags);
        const CheckAllBox box(&list);
        QVERIFY(box.isTristate());
        QCOMPARE(box.checkState(), Qt::PartiallyChecked);

        second->setCheckState(Qt::Checked);
        QCOMPARE(box.checkState(), Qt::Checked);
        first->setCheckState(Qt::Unchecked);
        second->setCheckState(Qt::Unchecked);
        QCOMPARE(box.checkState(), Qt::Unchecked);

        checkableItem(list, Qt::Checked);
        QCOMPARE(box.checkState(), Qt::PartiallyChecked);
        list.clear();
        QCOMPARE(box.checkState(), Qt::Unchecked);
        QVERIFY(!box.isEnabled());
    }

    // A click checks every checkable item, from a partially checked box as well, and a click on
    // a checked box unchecks every checkable item. The view reports each changed item, and an
    // item that is not checkable is not changed.
    void a_click_checks_or_unchecks_every_item() {
        QListWidget list;
        checkableItem(list, Qt::Checked);
        checkableItem(list, Qt::Unchecked);
        checkableItem(list, Qt::Unchecked);
        auto fixed = new QListWidgetItem(QStringLiteral("fixed"), &list);
        fixed->setFlags(Qt::ItemIsUserCheckable);
        fixed->setCheckState(Qt::Unchecked);
        CheckAllBox box(&list);
        QSignalSpy changed(&list, &QListWidget::itemChanged);

        box.click();
        QCOMPARE(box.checkState(), Qt::Checked);
        QCOMPARE(changed.size(), 2);
        for (int row = 0; row < 3; ++row) {
            QCOMPARE(list.item(row)->checkState(), Qt::Checked);
        }
        QCOMPARE(fixed->checkState(), Qt::Unchecked);

        box.click();
        QCOMPARE(box.checkState(), Qt::Unchecked);
        QCOMPARE(changed.size(), 5);
        for (int row = 0; row < 3; ++row) {
            QCOMPARE(list.item(row)->checkState(), Qt::Unchecked);
        }
    }

    // The items of a column of a tree are followed, and the box is disabled with the view.
    void the_column_of_a_tree_is_followed_and_the_view_disables_the_box() {
        QTreeWidget tree;
        tree.setColumnCount(2);
        for (const auto state : {Qt::Unchecked, Qt::Unchecked}) {
            auto item = new QTreeWidgetItem(&tree);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(0, state);
        }
        CheckAllBox box(&tree);
        QCOMPARE(box.checkState(), Qt::Unchecked);
        QVERIFY(box.isEnabled());
        box.click();
        QCOMPARE(tree.topLevelItem(0)->checkState(0), Qt::Checked);
        QCOMPARE(tree.topLevelItem(1)->checkState(0), Qt::Checked);

        tree.setEnabled(false);
        QVERIFY(!box.isEnabled());
        tree.setEnabled(true);
        QVERIFY(box.isEnabled());
    }

    // A partially checked item makes the box partially checked, although no top-level item is
    // checked.
    void a_partially_checked_item_makes_the_box_partial() {
        QTreeWidget tree;
        auto folder = new QTreeWidgetItem(&tree);
        folder->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
        for (const auto state : {Qt::Checked, Qt::Unchecked}) {
            auto item = new QTreeWidgetItem(folder);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(0, state);
        }
        auto file = new QTreeWidgetItem(&tree);
        file->setFlags(file->flags() | Qt::ItemIsUserCheckable);
        file->setCheckState(0, Qt::Unchecked);
        CheckAllBox box(&tree);
        QCOMPARE(folder->checkState(0), Qt::PartiallyChecked);
        QCOMPARE(box.checkState(), Qt::PartiallyChecked);

        box.click();
        QCOMPARE(folder->child(1)->checkState(0), Qt::Checked);
        QCOMPARE(box.checkState(), Qt::Checked);
    }
};

QTEST_MAIN(test_CheckAllBox)
#include "test_CheckAllBox.moc"
