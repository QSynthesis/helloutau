#include <QtTest/QTest>

#include <hellokit/Edit/EditSession.h>
#include <hellokit/Edit/NodeRef.h>
#include <hellokit/Edit/ProjectRefs.h>

#include "ProjectSamples.h"

using namespace hello::kit;

class test_NodeRef : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void a_default_handle_is_invalid_and_reads_defaults() {
        const NoteRef note;
        QVERIFY(!note.isValid());
        QCOMPARE(note.id(), NodeId(0));
        QVERIFY(note.lyric().isEmpty());
        QVERIFY(!note.intensity().has_value());
        QCOMPARE(note.portamento().size(), 0);
    }

    // Acceptance criterion 5 of docs/Editing.md. A handle holds the identifier, which removal,
    // undo and redo leave unchanged.
    void a_handle_of_a_removed_node_is_valid_again_after_undo() {
        EditSession session(richProject());
        auto notes = ProjectRef(&session).track(0).notes();
        const auto second = notes.at(1);
        const auto lyric = second.lyric();

        auto transaction = session.transaction(QStringLiteral("Remove"));
        notes.remove(1, 1);
        transaction.commit();
        QVERIFY(!second.isValid());
        QVERIFY(second.lyric().isEmpty());

        session.undo();
        QVERIFY(second.isValid());
        QVERIFY(notes.at(1) == second);
        QCOMPARE(second.lyric(), lyric);

        session.redo();
        QVERIFY(!second.isValid());
    }

    void a_moved_node_keeps_its_handle() {
        EditSession session(richProject());
        auto notes = ProjectRef(&session).track(0).notes();
        const auto first = notes.at(0);

        auto transaction = session.transaction(QStringLiteral("Move"));
        notes.move(0, 1, 1);
        transaction.commit();
        QVERIFY(first.isValid());
        QVERIFY(notes.at(1) == first);
        QVERIFY(notes.at(0) != first);
    }

    // The identifier of a node is not reused, so a handle of a destroyed node does not refer to
    // a node inserted later at the same position.
    void a_handle_of_a_destroyed_node_stays_invalid() {
        EditSession session(richProject());
        auto notes = ProjectRef(&session).track(0).notes();

        auto insert = session.transaction(QStringLiteral("Insert"));
        notes.insert(1, {Note()});
        insert.commit();
        const auto inserted = notes.at(1);
        QVERIFY(inserted.isValid());

        // After the undo, the insertion owns the node. The next commit discards the insertion
        // from the redo branch, which destroys the node.
        session.undo();
        auto other = session.transaction(QStringLiteral("Insert again"));
        notes.insert(1, {Note()});
        other.commit();

        QVERIFY(!inserted.isValid());
        QVERIFY(notes.at(1) != inserted);
        QVERIFY(!session.canRedo());
    }
};

QTEST_APPLESS_MAIN(test_NodeRef)

#include "test_NodeRef.moc"
