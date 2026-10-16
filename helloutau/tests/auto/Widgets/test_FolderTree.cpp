#include <QtCore/QDir>
#include <QtTest/QTest>
#include <QtWidgets/QTreeWidget>

#include <helloutau/Widgets/FolderTree.h>

using namespace hello::daw;

class test_FolderTree : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // A file lies under the items of its folders, each created once in the order of its first
    // file, with the file name and the folder names in the first column.
    void a_file_lies_under_its_folders() {
        QTreeWidget tree;
        FolderTree folders(&tree);
        const auto a = folders.addFile("a.wav");
        const auto x = folders.addFile("sub/x.wav");
        const auto y = folders.addFile("sub/deep/y.wav");
        const auto z = folders.addFile("sub/z.wav");
        QCOMPARE(tree.topLevelItemCount(), 2);
        QCOMPARE(tree.topLevelItem(0), a);
        QCOMPARE(a->text(0), QStringLiteral("a.wav"));
        const auto sub = tree.topLevelItem(1);
        QCOMPARE(sub->text(0), QStringLiteral("sub"));
        QCOMPARE(x->parent(), sub);
        QCOMPARE(z->parent(), sub);
        QCOMPARE(x->text(0), QStringLiteral("x.wav"));
        const auto deep = y->parent();
        QCOMPARE(deep->text(0), QStringLiteral("deep"));
        QCOMPARE(deep->parent(), sub);
        QCOMPARE(sub->childCount(), 3);
        QCOMPARE(sub->child(1), deep);
    }

    // The items of the folders are expanded and have the flags of the constructor, and the items
    // of the folders are unchecked if they are checkable.
    void the_folders_have_the_flags_given() {
        QTreeWidget tree;
        FolderTree plain(&tree);
        const auto sub = plain.addFile("sub/x.wav")->parent();
        QCOMPARE(sub->flags(), Qt::ItemFlags(Qt::ItemIsEnabled));
        QVERIFY(sub->isExpanded());
        QVERIFY(!sub->data(0, Qt::CheckStateRole).isValid());

        QTreeWidget checkable;
        FolderTree folders(&checkable, Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        const auto other = folders.addFile("other/y.wav")->parent();
        QVERIFY(other->flags().testFlag(Qt::ItemIsUserCheckable));
        QCOMPARE(other->checkState(0), Qt::Unchecked);
        QVERIFY(other->isExpanded());
    }

    // A path outside the folder of the tree lies at the top level with the whole path.
    void a_path_outside_lies_at_the_top_level() {
        QTreeWidget tree;
        FolderTree folders(&tree);
        const auto up = folders.addFile("../sub/e.wav");
        const auto absolute = folders.addFile("/bank/f.wav");
        QCOMPARE(tree.topLevelItemCount(), 2);
        QVERIFY(!up->parent());
        QVERIFY(!absolute->parent());
        QCOMPARE(up->text(0), QDir::toNativeSeparators(QStringLiteral("../sub/e.wav")));
        QCOMPARE(absolute->text(0), QDir::toNativeSeparators(QStringLiteral("/bank/f.wav")));
    }
};

QTEST_MAIN(test_FolderTree)

#include "test_FolderTree.moc"
