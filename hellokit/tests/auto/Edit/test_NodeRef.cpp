#include <QtTest/QTest>

#include <hellokit/Edit/NodeRef.h>

#include "TestSession.h"

using namespace hello::kit;

// The tests use a tree unrelated to UTAU, see TestSession.h.
class test_NodeRef : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void a_default_handle_is_invalid() {
        const NodeRef ref;
        QVERIFY(!ref.isValid());
        QCOMPARE(ref.id(), NodeId(0));
        QVERIFY(!ref.session());
    }

    // Acceptance criterion 5 of docs/Editing.md. A handle holds the identifier, which removal,
    // undo and redo leave unchanged.
    void a_handle_of_a_removed_node_is_valid_again_after_undo() {
        TestSession session;
        const NodeRef second(&session, session.itemAt(1));
        QVERIFY(second.isValid());

        auto transaction = session.transaction(QStringLiteral("Remove"));
        session.removeItems(1, 1);
        transaction.commit();
        QVERIFY(!second.isValid());
        QVERIFY(session.nameOf(second.id()).isEmpty());

        session.undo();
        QVERIFY(second.isValid());
        QCOMPARE(session.itemAt(1), second.id());
        QCOMPARE(session.nameOf(second.id()), QStringLiteral("second"));

        session.redo();
        QVERIFY(!second.isValid());
    }

    void a_moved_node_keeps_its_handle() {
        TestSession session;
        const NodeRef first(&session, session.itemAt(0));

        auto transaction = session.transaction(QStringLiteral("Move"));
        session.moveItems(0, 1, 1);
        transaction.commit();
        QVERIFY(first.isValid());
        QCOMPARE(session.itemAt(1), first.id());
    }

    // The identifier of a node is not reused, so a handle of a destroyed node does not refer to
    // a node inserted later at the same position.
    void a_handle_of_a_destroyed_node_stays_invalid() {
        TestSession session;

        auto insert = session.transaction(QStringLiteral("Insert"));
        session.insertItems(1, {QStringLiteral("inserted")});
        insert.commit();
        const NodeRef inserted(&session, session.itemAt(1));
        QVERIFY(inserted.isValid());

        // After the undo, the insertion owns the node. The next commit discards the insertion
        // from the redo branch, which destroys the node.
        session.undo();
        auto other = session.transaction(QStringLiteral("Insert again"));
        session.insertItems(1, {QStringLiteral("again")});
        other.commit();

        QVERIFY(!inserted.isValid());
        QVERIFY(session.itemAt(1) != inserted.id());
        QVERIFY(!session.canRedo());
    }

    void handles_are_equal_if_they_refer_to_the_same_node() {
        TestSession session;
        const NodeRef first(&session, session.itemAt(0));
        QVERIFY(first == NodeRef(&session, session.itemAt(0)));
        QVERIFY(first != NodeRef(&session, session.itemAt(1)));
        QVERIFY(first != NodeRef());
    }
};

QTEST_APPLESS_MAIN(test_NodeRef)

#include "test_NodeRef.moc"
