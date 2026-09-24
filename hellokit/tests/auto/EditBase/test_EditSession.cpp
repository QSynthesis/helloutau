#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <hellokit/EditBase/Change.h>
#include <hellokit/EditBase/EditSession.h>

#include "TestSession.h"

using namespace hello::kit;
using namespace hello::kit::edit;

// The tests use a tree unrelated to UTAU, see TestSession.h.
class test_EditSession : public QObject {
    Q_OBJECT

private:
    static QStringList initialNames() {
        return {QStringLiteral("first"), QStringLiteral("second")};
    }

    static ChangePtr changeAt(const QSignalSpy &spy, qsizetype index) {
        return spy.at(index).at(0).value<ChangePtr>();
    }

    static void verifyList(const ChangePtr &change, ListChange::Type type, NodeId node, int index,
                           int count) {
        const auto list = change->as<ListChange>();
        QVERIFY(list);
        QCOMPARE(list->type(), type);
        QCOMPARE(list->node(), node);
        QCOMPARE(list->index(), index);
        QCOMPARE(list->count(), count);
    }

    static void verifyMove(const ChangePtr &change, NodeId node, int index, int count,
                           int destination) {
        const auto move = change->as<MoveChange>();
        QVERIFY(move);
        QCOMPARE(move->node(), node);
        QCOMPARE(move->index(), index);
        QCOMPARE(move->count(), count);
        QCOMPARE(move->destination(), destination);
    }

private Q_SLOTS:
    void a_node_is_in_the_tree_until_it_is_removed() {
        TestSession session;
        const auto second = session.itemAt(1);
        QVERIFY(session.contains(session.root()));
        QVERIFY(session.contains(second));
        QVERIFY(!session.contains(0));

        auto transaction = session.transaction(QStringLiteral("Remove"));
        session.removeItems(1, 1);
        transaction.commit();
        QVERIFY(!session.contains(second));

        session.undo();
        QVERIFY(session.contains(second));
    }

    void a_transaction_without_commit_is_rolled_back() {
        TestSession session;
        {
            auto transaction = session.transaction(QStringLiteral("Discarded"));
            session.setTitle(QStringLiteral("x"));
            session.removeItems(0, 1);
            session.setTag(QStringLiteral("a"), QVariant());
            QVERIFY(session.inTransaction());
        }
        QVERIFY(!session.inTransaction());
        QVERIFY(!session.canUndo());
        QCOMPARE(session.title(), QStringLiteral("title"));
        QCOMPARE(session.names(), initialNames());
        QCOMPARE(session.tagKeys(), QStringList({QStringLiteral("a")}));
    }

    void a_committed_transaction_is_one_undo_step_with_its_message() {
        TestSession session;
        auto transaction = session.transaction(QString::fromUtf8("移动 1 个项目"));
        session.setTitle(QStringLiteral("x"));
        session.moveItems(0, 1, 1);
        transaction.commit();

        QVERIFY(session.canUndo());
        QCOMPARE(session.undoMessage(), QString::fromUtf8("移动 1 个项目"));
        QVERIFY(session.redoMessage().isEmpty());

        session.undo();
        QCOMPARE(session.names(), initialNames());
        QCOMPARE(session.title(), QStringLiteral("title"));
        QVERIFY(!session.canUndo());
        QCOMPARE(session.redoMessage(), QString::fromUtf8("移动 1 个项目"));

        session.redo();
        QCOMPARE(session.names(), QStringList({QStringLiteral("second"), QStringLiteral("first")}));
        QCOMPARE(session.title(), QStringLiteral("x"));
    }

    // A function that begins a transaction can be called within another transaction, and its
    // modifications then belong to the undo step of the outermost transaction.
    void a_nested_transaction_joins_the_outermost_one() {
        TestSession session;
        auto outer = session.transaction(QStringLiteral("Outer"));
        session.setTitle(QStringLiteral("outer"));
        {
            auto inner = session.transaction(QStringLiteral("Inner"));
            session.moveItems(0, 1, 1);
            QVERIFY(inner.commit());
        }
        QVERIFY(session.inTransaction());
        DiagnosticList diagnostics;
        QVERIFY(outer.commit(diagnostics));
        QVERIFY(diagnostics.isEmpty());

        QVERIFY(!session.inTransaction());
        QCOMPARE(session.currentStep(), 1);
        QCOMPARE(session.undoMessage(), QStringLiteral("Outer"));
        session.undo();
        QCOMPARE(session.title(), QStringLiteral("title"));
        QCOMPARE(session.names(), initialNames());
    }

    // A nested transaction that ends without commit discards the outermost one, whose commit
    // then rolls back and reports the reason.
    void a_discarded_nested_transaction_discards_the_outermost_one() {
        TestSession session;
        auto outer = session.transaction(QStringLiteral("Outer"));
        session.setTitle(QStringLiteral("outer"));
        {
            auto inner = session.transaction(QStringLiteral("Inner"));
            session.moveItems(0, 1, 1);
        }
        QVERIFY(session.inTransaction());
        session.setTag(QStringLiteral("after"), 1);

        DiagnosticList diagnostics;
        QVERIFY(!outer.commit(diagnostics));
        QCOMPARE(diagnostics.size(), 1);
        QVERIFY(hasError(diagnostics));

        QVERIFY(!session.inTransaction());
        QVERIFY(!session.canUndo());
        QCOMPARE(session.title(), QStringLiteral("title"));
        QCOMPARE(session.names(), initialNames());
        QCOMPARE(session.tagKeys(), QStringList({QStringLiteral("a")}));

        // The session accepts the next transaction as usual.
        auto next = session.transaction(QStringLiteral("Next"));
        session.setTitle(QStringLiteral("next"));
        QVERIFY(next.commit());
        QCOMPARE(session.undoMessage(), QStringLiteral("Next"));
    }

    void a_transaction_that_introduces_a_violation_is_rolled_back() {
        TestSession session;
        const auto first = session.itemAt(0);
        QSignalSpy stepChanged(&session, &EditSession::stepChanged);

        auto transaction = session.transaction(QStringLiteral("Rename"));
        session.setTitle(QStringLiteral("renamed"));
        session.setName(first, QString());
        DiagnosticList diagnostics;
        QVERIFY(!transaction.commit(diagnostics));

        QCOMPARE(diagnostics.size(), 1);
        QVERIFY(hasError(diagnostics));
        QCOMPARE(diagnostics.first().message, QStringLiteral("empty name"));
        QVERIFY(!session.canUndo());
        QCOMPARE(stepChanged.count(), 0);
        QCOMPARE(session.title(), QStringLiteral("title"));
        QCOMPARE(session.names(), initialNames());
    }

    // A document read from a file may violate its constraints. A modification that does not
    // introduce a violation is committed, including a modification of the violating record.
    void a_violation_from_before_the_transaction_does_not_prevent_the_commit() {
        TestSession session({QStringLiteral("first"), QString()});
        const auto second = session.itemAt(1);

        auto transaction = session.transaction(QStringLiteral("Unrelated"));
        session.setTitle(QStringLiteral("renamed"));
        session.removeValuesOf(second);
        QVERIFY(transaction.commit());

        auto repair = session.transaction(QStringLiteral("Repair"));
        session.setName(second, QStringLiteral("second"));
        QVERIFY(repair.commit());

        auto breaking = session.transaction(QStringLiteral("Break"));
        session.setName(session.itemAt(0), QString());
        QVERIFY(!breaking.commit());
    }

    // An inserted record has no state before the transaction, therefore each of its violations is
    // introduced by the transaction. The record that holds the list is validated as well.
    void inserted_records_and_their_list_are_validated() {
        TestSession session;
        auto insertEmpty = session.transaction(QStringLiteral("Insert"));
        session.insertItems(1, {QString()});
        DiagnosticList diagnostics;
        QVERIFY(!insertEmpty.commit(diagnostics));
        QCOMPARE(diagnostics.first().message, QStringLiteral("empty name"));

        auto insertMany = session.transaction(QStringLiteral("Insert"));
        session.insertItems(0, {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")});
        diagnostics.clear();
        QVERIFY(!insertMany.commit(diagnostics));
        QCOMPARE(diagnostics.first().message, QStringLiteral("5 items"));
        QCOMPARE(session.names(), initialNames());

        // A violating record removed by the transaction no longer counts.
        auto insertAndRemove = session.transaction(QStringLiteral("Insert"));
        session.insertItems(0, {QString()});
        session.removeItems(0, 1);
        QVERIFY(insertAndRemove.commit());
    }

    // The validation belongs to the outermost transaction, which a nested one joins.
    void a_nested_transaction_is_validated_with_the_outermost_one() {
        TestSession session;
        auto outer = session.transaction(QStringLiteral("Outer"));
        {
            auto inner = session.transaction(QStringLiteral("Inner"));
            session.setName(session.itemAt(0), QString());
            QVERIFY(inner.commit());
        }
        QVERIFY(!outer.commit());
        QCOMPARE(session.names(), initialNames());
    }

    void the_step_numbers_follow_commits_undo_and_redo() {
        TestSession session;
        QCOMPARE(session.currentStep(), 0);
        QCOMPARE(session.minimumStep(), 0);
        QCOMPARE(session.maximumStep(), 0);

        QSignalSpy stepChanged(&session, &EditSession::stepChanged);
        for (int i = 1; i <= 3; ++i) {
            auto transaction = session.transaction(QStringLiteral("Step %1").arg(i));
            session.setTitle(QString::number(i));
            transaction.commit();
        }
        QCOMPARE(session.currentStep(), 3);
        QCOMPARE(session.maximumStep(), 3);
        QCOMPARE(session.stepMessage(1), QStringLiteral("Step 1"));
        QCOMPARE(session.stepMessage(3), QStringLiteral("Step 3"));
        QVERIFY(session.stepMessage(4).isEmpty());

        session.undo();
        session.undo();
        QCOMPARE(session.currentStep(), 1);
        QCOMPARE(session.maximumStep(), 3);
        QCOMPARE(session.title(), QStringLiteral("1"));

        // A commit discards the undone steps, and the new step follows the current position.
        auto transaction = session.transaction(QStringLiteral("Branch"));
        session.setTitle(QStringLiteral("branch"));
        transaction.commit();
        QCOMPARE(session.currentStep(), 2);
        QCOMPARE(session.maximumStep(), 2);
        QCOMPARE(session.stepMessage(2), QStringLiteral("Branch"));

        QList<int> steps;
        for (const auto &arguments : std::as_const(stepChanged)) {
            steps.push_back(arguments.at(0).toInt());
        }
        QCOMPARE(steps, QList<int>({1, 2, 3, 2, 1, 2}));
    }

    void a_transaction_without_changes_creates_no_undo_step() {
        TestSession session;
        auto transaction = session.transaction(QStringLiteral("Nothing"));
        session.setTitle(QStringLiteral("title"));
        session.setTag(QStringLiteral("a"), 1);
        transaction.commit();
        QVERIFY(!session.canUndo());
    }

    void every_built_in_change_is_reported_in_the_applied_direction() {
        TestSession session;
        const auto items = session.items();
        const auto first = session.itemAt(0);
        const auto second = session.itemAt(1);
        const auto values = session.valuesOf(first);

        QSignalSpy changed(&session, &EditSession::changed);
        QSignalSpy stepChanged(&session, &EditSession::stepChanged);

        auto transaction = session.transaction(QStringLiteral("Changes"));
        session.setTitle(QStringLiteral("x"));
        session.setTag(QStringLiteral("new"), 2);
        session.editValues(values)->remove(0, 1);
        session.editValues(values)->insert(0, {5});
        session.editValues(values)->replace(0, {6});
        session.removeValuesOf(second);
        session.insertItems(2, {QStringLiteral("third"), QStringLiteral("fourth")});
        session.moveItems(0, 1, 3);
        QCOMPARE(stepChanged.count(), 0);
        transaction.commit();
        QCOMPARE(stepChanged.count(), 1);

        QCOMPARE(changed.count(), 8);
        const auto title = changeAt(changed, 0)->as<ValueChange>();
        QVERIFY(title);
        QCOMPARE(title->node(), session.root());
        QCOMPARE(title->slot(), 0);
        QCOMPARE(title->oldValue(), QVariant(QStringLiteral("title")));
        QCOMPARE(title->newValue(), QVariant(QStringLiteral("x")));
        const auto tag = changeAt(changed, 1)->as<EntryChange>();
        QVERIFY(tag);
        QCOMPARE(tag->node(), session.tags());
        QCOMPARE(tag->key(), QStringLiteral("new"));
        QVERIFY(!tag->oldValue().isValid());
        QCOMPARE(tag->newValue(), QVariant(2));
        for (int i = 2; i < 5; ++i) {
            const auto array = changeAt(changed, i)->as<ArrayChange>();
            QVERIFY(array);
            QCOMPARE(array->node(), values);
        }
        const auto child = changeAt(changed, 5)->as<ValueChange>();
        QVERIFY(child);
        QCOMPARE(child->node(), second);
        QCOMPARE(child->slot(), 1);
        verifyList(changeAt(changed, 6), ListChange::Inserted, items, 2, 2);
        verifyMove(changeAt(changed, 7), items, 0, 1, 3);

        // Undo applies the inverse changes in reverse order: the move back, then the removal of
        // the inserted items, which is announced while they are still in the list.
        changed.clear();
        session.undo();
        QCOMPARE(stepChanged.count(), 2);
        QCOMPARE(changed.count(), 9);
        verifyMove(changeAt(changed, 0), items, 3, 1, 0);
        verifyList(changeAt(changed, 1), ListChange::AboutToBeRemoved, items, 2, 2);
        verifyList(changeAt(changed, 2), ListChange::Removed, items, 2, 2);
        QVERIFY(changeAt(changed, 3)->as<ValueChange>());
        const auto titleBack = changeAt(changed, 8)->as<ValueChange>();
        QVERIFY(titleBack);
        QCOMPARE(titleBack->oldValue(), QVariant(QStringLiteral("x")));
        QCOMPARE(titleBack->newValue(), QVariant(QStringLiteral("title")));
        const auto tagBack = changeAt(changed, 7)->as<EntryChange>();
        QVERIFY(tagBack);
        QCOMPARE(tagBack->oldValue(), QVariant(2));
        QVERIFY(!tagBack->newValue().isValid());
    }

    void a_rollback_reports_the_inverse_changes() {
        TestSession session;
        QSignalSpy changed(&session, &EditSession::changed);
        QSignalSpy stepChanged(&session, &EditSession::stepChanged);
        {
            auto transaction = session.transaction(QStringLiteral("Discarded"));
            session.insertItems(0, {QStringLiteral("discarded")});
        }
        QCOMPARE(changed.count(), 3);
        verifyList(changeAt(changed, 0), ListChange::Inserted, session.items(), 0, 1);
        verifyList(changeAt(changed, 1), ListChange::AboutToBeRemoved, session.items(), 0, 1);
        verifyList(changeAt(changed, 2), ListChange::Removed, session.items(), 0, 1);
        QCOMPARE(stepChanged.count(), 0);
    }

    // A change holds no pointer into the tree, therefore it can be kept after the signal.
    void a_change_remains_valid_after_the_node_is_destroyed() {
        TestSession session;
        ChangePtr kept;
        QObject::connect(&session, &EditSession::changed, &session,
                         [&kept](const ChangePtr &change) { kept = change; });
        {
            auto transaction = session.transaction(QStringLiteral("Discarded"));
            session.insertItems(0, {QStringLiteral("discarded")});
        }
        QVERIFY(kept);
        QVERIFY(kept->as<ListChange>());
        QVERIFY(!kept->as<MoveChange>());
    }
};

QTEST_APPLESS_MAIN(test_EditSession)

#include "test_EditSession.moc"
