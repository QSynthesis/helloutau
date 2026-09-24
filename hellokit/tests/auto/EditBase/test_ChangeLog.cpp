#include <QtCore/QJsonObject>
#include <QtTest/QTest>

#include <hellokit/EditBase/private/ChangeLog_p.h>

#include "TestSession.h"

using namespace hello::kit;
using namespace hello::kit::edit;

namespace {

    // A change of a kind added outside the library.
    class RenameChange : public Change {
    public:
        static constexpr int Kind = Change::User;

        RenameChange(NodeId node, const QString &name) : Change(Kind, node), m_name(name) {
        }

        const QString &name() const {
            return m_name;
        }

    private:
        QString m_name;
    };

}

// The entries of the generic change log on a tree without a field table. The entries with a
// field table are tested with a project in test_ProjectSession.
class test_ChangeLog : public QObject {
    Q_OBJECT

private:
    static const RecordInfo *noRecord(int) {
        return nullptr;
    }

    template <class Edit>
    static QList<QJsonObject> logOf(TestSession &session, Edit edit) {
        QList<QJsonObject> entries;
        const auto connection = QObject::connect(
            &session, &EditSession::changed, &session, [&](const ChangePtr &change) {
                if (const auto entry = ChangeLog::entryOf(session, *change, noRecord)) {
                    entries.push_back(*entry);
                }
            });
        auto transaction = session.transaction(QStringLiteral("Edit"));
        edit();
        transaction.commit();
        QObject::disconnect(connection);
        return entries;
    }

private Q_SLOTS:
    // Without a field table, a slot is named by its index and its values are written as
    // QVariant converts them.
    void a_slot_without_a_field_is_logged_by_index() {
        TestSession session;
        const auto entries = logOf(session, [&] { session.setTitle(QStringLiteral("new")); });
        QCOMPARE(entries, QList<QJsonObject>({
                              QJsonObject{{QStringLiteral("node"), qint64(session.root())},
                                          {QStringLiteral("shape"), QStringLiteral("set")},
                                          {QStringLiteral("slot"), 0},
                                          {QStringLiteral("before"), QStringLiteral("title")},
                                          {QStringLiteral("after"), QStringLiteral("new")}},
        }));
    }

    void a_mapping_entry_is_logged_with_its_key() {
        TestSession session;
        const auto entries = logOf(session, [&] {
            session.setTag(QStringLiteral("a"), QVariant(2));
            session.setTag(QStringLiteral("a"), QVariant());
        });
        const auto id = qint64(session.tags());
        QCOMPARE(entries, QList<QJsonObject>({
                              QJsonObject{{QStringLiteral("node"), id},
                                          {QStringLiteral("shape"), QStringLiteral("entry")},
                                          {QStringLiteral("key"), QStringLiteral("a")},
                                          {QStringLiteral("before"), 1},
                                          {QStringLiteral("after"), 2}},
                              QJsonObject{{QStringLiteral("node"), id},
                                          {QStringLiteral("shape"), QStringLiteral("entry")},
                                          {QStringLiteral("key"), QStringLiteral("a")},
                                          {QStringLiteral("before"), 2}},
        }));
    }

    // A kind without a writer is recorded rather than omitted, so that the log shows every
    // change.
    void a_kind_without_a_writer_is_logged_by_its_number() {
        TestSession session;
        const RenameChange change(session.root(), QStringLiteral("x"));
        QCOMPARE(ChangeLog::entryOf(session, change, noRecord),
                 std::optional<QJsonObject>(QJsonObject{
                     {QStringLiteral("kind"), Change::User          },
                     {QStringLiteral("node"), qint64(session.root())},
        }));
    }

    // A kind added outside the library is written by its own writer, registered through the
    // interface of the kinds of the library.
    void a_kind_is_logged_by_its_registered_writer() {
        TestSession session;
        EditSessionPrivate::registerLogWriter(
            session, RenameChange::Kind,
            [](const EditSession &, const Change &change, RecordLookup) {
                return std::optional<QJsonObject>(QJsonObject{
                    {QStringLiteral("shape"), QStringLiteral("rename")         },
                    {QStringLiteral("name"),  change.as<RenameChange>()->name()},
                });
            });
        const RenameChange change(session.root(), QStringLiteral("x"));
        QCOMPARE(ChangeLog::entryOf(session, change, noRecord),
                 std::optional<QJsonObject>(QJsonObject{
                     {QStringLiteral("shape"), QStringLiteral("rename")},
                     {QStringLiteral("name"),  QStringLiteral("x")     },
                     {QStringLiteral("node"),  qint64(session.root())  },
        }));
    }

    void a_writer_may_omit_a_change() {
        TestSession session;
        EditSessionPrivate::registerLogWriter(
            session, RenameChange::Kind,
            [](const EditSession &, const Change &, RecordLookup) -> std::optional<QJsonObject> {
                return std::nullopt;
            });
        QVERIFY(!ChangeLog::entryOf(session, RenameChange(session.root(), QStringLiteral("x")),
                                    noRecord));
    }
};

QTEST_APPLESS_MAIN(test_ChangeLog)

#include "test_ChangeLog.moc"
