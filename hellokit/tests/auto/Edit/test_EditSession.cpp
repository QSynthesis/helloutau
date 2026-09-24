#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <hellokit/Edit/EditSession.h>

#include "TestSession.h"

using namespace hello::kit;

// The tests use a tree unrelated to UTAU, see TestSession.h.
class test_EditSession : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void nodes_are_addressed_by_identifier_and_slot() {
        TestSession session;
        QCOMPARE(session.value(session.root(), TestRootSlots::Title), QStringLiteral("title"));
        QCOMPARE(session.size(session.items()), 2);
        QCOMPARE(session.names(), QStringList({QStringLiteral("first"), QStringLiteral("second")}));

        const auto first = session.at(session.items(), 0);
        const auto values = session.child(first, TestItemSlots::Values);
        QCOMPARE(session.size(values), 3);
        QCOMPARE(session.values(values), QList<double>({1, 2, 3}));

        QCOMPARE(session.size(session.tags()), 1);
        QCOMPARE(session.keys(session.tags()), QStringList({QStringLiteral("a")}));
        QCOMPARE(session.entry(session.tags(), QStringLiteral("a")).toInt(), 1);
    }

    // A node of another kind, and an identifier of no node, read as default values.
    void a_node_of_another_kind_reads_as_defaults() {
        TestSession session;
        QVERIFY(!session.value(session.items(), 0).isValid());
        QCOMPARE(session.child(session.items(), TestItemSlots::Values), NodeId(0));
        QVERIFY(session.keys(session.items()).isEmpty());
        QVERIFY(session.values(session.tags()).isEmpty());
        QCOMPARE(session.size(session.root()), 0);
        QVERIFY(!session.contains(0));
        QVERIFY(!session.contains(1000000));
    }

    void every_kind_of_modification_is_applied() {
        TestSession session;
        const auto second = session.at(session.items(), 1);
        const auto values = session.child(session.at(session.items(), 0), TestItemSlots::Values);

        auto transaction = session.transaction(QStringLiteral("Edit"));
        session.setValue(session.root(), TestRootSlots::Title, QStringLiteral("renamed"));
        session.insertItems(1, {QStringLiteral("inserted")});
        session.move(session.items(), 0, 1, 2);
        session.setEntry(session.tags(), QStringLiteral("b"), 2);
        session.setEntry(session.tags(), QStringLiteral("a"), QVariant());
        session.replaceValues(values, 2, {30, 40});
        session.insertValues(values, 0, {0});
        session.removeValues(values, 1, 1);
        session.removeChild(second, TestItemSlots::Values);
        transaction.commit();

        QCOMPARE(session.value(session.root(), TestRootSlots::Title), QStringLiteral("renamed"));
        QCOMPARE(session.names(), QStringList({QStringLiteral("inserted"), QStringLiteral("second"),
                                               QStringLiteral("first")}));
        QCOMPARE(session.keys(session.tags()), QStringList({QStringLiteral("b")}));
        QCOMPARE(session.values(values), QList<double>({0, 2, 30, 40}));
        QCOMPARE(session.child(second, TestItemSlots::Values), NodeId(0));

        session.undo();
        QCOMPARE(session.value(session.root(), TestRootSlots::Title), QStringLiteral("title"));
        QCOMPARE(session.names(), QStringList({QStringLiteral("first"), QStringLiteral("second")}));
        QCOMPARE(session.keys(session.tags()), QStringList({QStringLiteral("a")}));
        QCOMPARE(session.values(values), QList<double>({1, 2, 3}));
        QVERIFY(session.child(second, TestItemSlots::Values) != 0);
    }

    void a_transaction_without_commit_is_rolled_back() {
        TestSession session;
        {
            auto transaction = session.transaction(QStringLiteral("Discarded"));
            session.setValue(session.root(), TestRootSlots::Title, QStringLiteral("x"));
            session.remove(session.items(), 0, 1);
            session.setEntry(session.tags(), QStringLiteral("a"), QVariant());
            QVERIFY(session.inTransaction());
        }
        QVERIFY(!session.inTransaction());
        QVERIFY(!session.canUndo());
        QCOMPARE(session.value(session.root(), TestRootSlots::Title), QStringLiteral("title"));
        QCOMPARE(session.names(), QStringList({QStringLiteral("first"), QStringLiteral("second")}));
        QCOMPARE(session.size(session.tags()), 1);
    }

    void a_committed_transaction_is_one_undo_step_with_its_message() {
        TestSession session;
        auto transaction = session.transaction(QString::fromUtf8("移动 1 个项目"));
        session.setValue(session.root(), TestRootSlots::Title, QStringLiteral("x"));
        session.move(session.items(), 0, 1, 1);
        transaction.commit();

        QVERIFY(session.canUndo());
        QCOMPARE(session.undoMessage(), QString::fromUtf8("移动 1 个项目"));
        QVERIFY(session.redoMessage().isEmpty());

        session.undo();
        QCOMPARE(session.names(), QStringList({QStringLiteral("first"), QStringLiteral("second")}));
        QCOMPARE(session.value(session.root(), TestRootSlots::Title), QStringLiteral("title"));
        QVERIFY(!session.canUndo());
        QCOMPARE(session.redoMessage(), QString::fromUtf8("移动 1 个项目"));

        session.redo();
        QCOMPARE(session.names(), QStringList({QStringLiteral("second"), QStringLiteral("first")}));
        QCOMPARE(session.value(session.root(), TestRootSlots::Title), QStringLiteral("x"));
    }

    void an_unchanged_value_creates_no_undo_step() {
        TestSession session;
        auto transaction = session.transaction(QStringLiteral("Nothing"));
        session.setValue(session.root(), TestRootSlots::Title, QStringLiteral("title"));
        session.setEntry(session.tags(), QStringLiteral("a"), 1);
        transaction.commit();
        QVERIFY(!session.canUndo());
    }

    void the_signals_report_the_changes_in_the_applied_direction() {
        TestSession session;
        const auto items = session.items();
        const auto values = session.child(session.at(items, 0), TestItemSlots::Values);

        QSignalSpy valueChanged(&session, &EditSession::valueChanged);
        QSignalSpy entryChanged(&session, &EditSession::entryChanged);
        QSignalSpy arrayChanged(&session, &EditSession::arrayChanged);
        QSignalSpy inserted(&session, &EditSession::itemsInserted);
        QSignalSpy aboutToBeRemoved(&session, &EditSession::itemsAboutToBeRemoved);
        QSignalSpy removed(&session, &EditSession::itemsRemoved);
        QSignalSpy moved(&session, &EditSession::itemsMoved);
        QSignalSpy stepChanged(&session, &EditSession::stepChanged);

        auto transaction = session.transaction(QStringLiteral("Signals"));
        session.setValue(session.root(), TestRootSlots::Title, QStringLiteral("x"));
        session.setEntry(session.tags(), QStringLiteral("new"), 2);
        session.removeValues(values, 0, 1);
        session.insertItems(2, {QStringLiteral("third"), QStringLiteral("fourth")});
        session.move(items, 0, 1, 3);
        QCOMPARE(stepChanged.count(), 0);
        transaction.commit();

        QCOMPARE(valueChanged.count(), 1);
        QCOMPARE(valueChanged.at(0).at(0).value<NodeId>(), session.root());
        QCOMPARE(valueChanged.at(0).at(1).toInt(), TestRootSlots::Title.index);
        QCOMPARE(entryChanged.count(), 1);
        QCOMPARE(entryChanged.at(0).at(1).toString(), QStringLiteral("new"));
        QCOMPARE(arrayChanged.count(), 1);
        QCOMPARE(arrayChanged.at(0).at(0).value<NodeId>(), values);
        QCOMPARE(inserted.count(), 1);
        QCOMPARE(inserted.at(0), QVariantList({QVariant::fromValue(items), 2, 2}));
        QCOMPARE(moved.count(), 1);
        QCOMPARE(moved.at(0), QVariantList({QVariant::fromValue(items), 0, 1, 3}));
        QCOMPARE(stepChanged.count(), 1);

        // Undo applies the inverse changes in reverse order: the move back, then the removal of
        // the inserted items, which is announced while they are still in the list.
        session.undo();
        QCOMPARE(moved.count(), 2);
        QCOMPARE(moved.at(1), QVariantList({QVariant::fromValue(items), 3, 1, 0}));
        QCOMPARE(aboutToBeRemoved.count(), 1);
        QCOMPARE(aboutToBeRemoved.at(0), QVariantList({QVariant::fromValue(items), 2, 2}));
        QCOMPARE(removed.count(), 1);
        QCOMPARE(removed.at(0), QVariantList({QVariant::fromValue(items), 2, 2}));
        QCOMPARE(valueChanged.count(), 2);
        QCOMPARE(entryChanged.count(), 2);
        QCOMPARE(arrayChanged.count(), 2);
        QCOMPARE(stepChanged.count(), 2);
    }

    void a_rollback_reports_the_inverse_changes() {
        TestSession session;
        QSignalSpy inserted(&session, &EditSession::itemsInserted);
        QSignalSpy removed(&session, &EditSession::itemsRemoved);
        QSignalSpy stepChanged(&session, &EditSession::stepChanged);
        {
            auto transaction = session.transaction(QStringLiteral("Discarded"));
            session.insertItems(0, {QStringLiteral("discarded")});
        }
        QCOMPARE(inserted.count(), 1);
        QCOMPARE(removed.count(), 1);
        QCOMPARE(stepChanged.count(), 0);
    }
};

QTEST_APPLESS_MAIN(test_EditSession)

#include "test_EditSession.moc"
