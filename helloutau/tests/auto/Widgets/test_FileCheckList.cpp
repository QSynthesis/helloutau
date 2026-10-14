#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>

#include <helloutau/Widgets/CheckAllBox.h>
#include <helloutau/Widgets/FileCheckList.h>

using namespace hello::daw;

namespace {

    // A list of three files, the second of which has no frequency map
    void fill(FileCheckList &list) {
        list.addRow({QStringLiteral("a.wav"), QStringLiteral("present")});
        list.addRow({QStringLiteral("i.wav"), QStringLiteral("missing")});
        list.addRow({QStringLiteral("u.wav"), QStringLiteral("present")});
    }

}

class test_FileCheckList : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // A list with some rows checked is destroyed without an assertion. The tree, destroyed
    // first, once unchecked the box for all during the destruction, when Qt asserted a parent.
    void a_partly_checked_list_is_destroyed() {
        auto list = new FileCheckList({QStringLiteral("File"), QStringLiteral("State")});
        fill(*list);
        list->setChecked(1, true);
        QCOMPARE(list->checkAllBox()->checkState(), Qt::PartiallyChecked);
        delete list;
    }

    // A row is added unchecked with its texts, and the box for all follows the rows.
    void rows_are_added_unchecked() {
        FileCheckList list({QStringLiteral("File"), QStringLiteral("State")});
        fill(list);
        QCOMPARE(list.rowCount(), 3);
        QCOMPARE(list.tree()->topLevelItem(1)->text(1), QStringLiteral("missing"));
        QVERIFY(list.checkedRows().isEmpty());
        QCOMPARE(list.checkAllBox()->checkState(), Qt::Unchecked);

        list.setChecked(2, true);
        QVERIFY(list.isChecked(2));
        QCOMPARE(list.checkedRows(), QList<int>{2});
        QCOMPARE(list.checkAllBox()->checkState(), Qt::PartiallyChecked);
    }

    // checkWhere() checks exactly the rows of the condition, with one signal.
    void the_rows_of_a_condition_are_checked() {
        FileCheckList list({QStringLiteral("File"), QStringLiteral("State")});
        fill(list);
        list.setChecked(0, true);
        QSignalSpy changed(&list, &FileCheckList::checkedChanged);
        list.checkWhere([](int row) { return row > 0; });
        QCOMPARE(list.checkedRows(), (QList<int>{1, 2}));
        QCOMPARE(changed.size(), 1);
    }

    // A button checks exactly the rows of its condition, such as the missing maps.
    void a_button_checks_the_rows_of_its_condition() {
        FileCheckList list({QStringLiteral("File"), QStringLiteral("State")});
        fill(list);
        list.checkWhere([](int) { return true; });
        const auto missing = list.addCheckButton(QStringLiteral("Check Missing"), [&list](int row) {
            return list.tree()->topLevelItem(row)->text(1) == QStringLiteral("missing");
        });
        missing->click();
        QCOMPARE(list.checkedRows(), QList<int>{1});
    }

    // A click on the box for all checks every row, and checkedChanged() follows the user.
    void the_box_checks_every_row() {
        FileCheckList list({QStringLiteral("File"), QStringLiteral("State")});
        fill(list);
        QSignalSpy changed(&list, &FileCheckList::checkedChanged);
        list.checkAllBox()->click();
        QCOMPARE(list.checkedRows(), (QList<int>{0, 1, 2}));
        QVERIFY(!changed.isEmpty());
    }

    // A column alignment applies to the header and to the cells, those of later rows included.
    void a_column_is_aligned() {
        FileCheckList list({QStringLiteral("File"), QStringLiteral("State")});
        list.addRow({QStringLiteral("a.wav"), QStringLiteral("present")});
        list.setColumnAlignment(1, Qt::AlignCenter);
        list.addRow({QStringLiteral("i.wav"), QStringLiteral("missing")});
        const auto tree = list.tree();
        QCOMPARE(tree->headerItem()->textAlignment(1), int(Qt::AlignCenter));
        QCOMPARE(tree->topLevelItem(0)->textAlignment(1), int(Qt::AlignCenter));
        QCOMPARE(tree->topLevelItem(1)->textAlignment(1), int(Qt::AlignCenter));
        QVERIFY(tree->topLevelItem(1)->textAlignment(0) != int(Qt::AlignCenter));
    }

    // The columns are resized with the mouse. The first show fits every column to its contents
    // without stretching the last to the width of the list, and a later show keeps the widths
    // that the user set.
    void the_columns_are_resized_by_the_user() {
        FileCheckList list(
            {QStringLiteral("File"), QStringLiteral("State"), QStringLiteral("Note")});
        const auto name = QStringLiteral("a-file-with-a-long-name.wav");
        list.addRow({name, QStringLiteral("present"), QString()});
        const auto header = list.tree()->header();
        for (int column = 0; column < 3; ++column) {
            QCOMPARE(header->sectionResizeMode(column), QHeaderView::Interactive);
        }
        header->resizeSection(0, 10);
        list.resize(1200, 300);
        list.show();
        QVERIFY(QTest::qWaitForWindowExposed(&list));
        QVERIFY(header->sectionSize(0) > list.tree()->fontMetrics().horizontalAdvance(name));
        QVERIFY(!header->stretchLastSection());
        QVERIFY(header->length() < list.tree()->viewport()->width());

        header->resizeSection(0, 300);
        list.hide();
        list.show();
        QCOMPARE(header->sectionSize(0), 300);
    }
};

QTEST_MAIN(test_FileCheckList)

#include "test_FileCheckList.moc"
