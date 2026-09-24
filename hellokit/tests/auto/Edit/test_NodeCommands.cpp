#include <QtCore/QJsonArray>
#include <QtCore/QDebug>
#include <QtCore/QJsonObject>
#include <QtTest/QTest>

#include "NodeCommands_p.h"
#include "TestSession.h"

using namespace hello::kit;

// The command lines are ordinary string literals rather than raw string literals, because moc
// does not recognize the class of a file whose raw strings contain unpaired quotes.
class test_NodeCommands : public QObject {
    Q_OBJECT

private:
    // The field table of the tree of TestSession.
    static const RecordInfo &itemRecord() {
        static const RecordInfo record{
            "item",
            TestSession::ItemType,
            {
                        valueField(Slot<QString>{0, "name"}),
                        arrayField(ChildSlot{1, "values"}, TestSession::ValuesType, ValueFormats::number),
                        },
            [](const QJsonObject &json, DiagnosticList &) -> std::unique_ptr<ss::Node> {
                auto node = std::make_unique<TestSession::Item>(TestSession::ItemType);
                node->setAt(0, QVariant(json.value(QStringLiteral("name")).toString()));
                node->setAt(1, std::make_unique<TestSession::Values>(TestSession::ValuesType));
                return node;
                        },
            [](const ss::Node *tree) {
                return QJsonObject{
                    {QStringLiteral("name"),
                        static_cast<const TestSession::Item &>(*tree).variant(0).toString()},
                };
                        },
        };
        return record;
    }

    static const RecordInfo &rootRecord() {
        static const RecordInfo record{
            "root",
            TestSession::RootType,
            {
                        valueField(Slot<QString>{0, "title"}),
                        listField(ChildSlot{1, "items"}, itemRecord()),
                        mappingField(ChildSlot{2, "tags"}, ValueFormats::integer),
                        },
        };
        return record;
    }

    // Executes line as ProjectCommands does: one transaction, committed if the command succeeds.
    static bool run(TestSession &session, const QString &line, DiagnosticList &diagnostics) {
        const auto arguments = CommandSyntax::split(line, diagnostics);
        if (!arguments || arguments->isEmpty()) {
            return false;
        }
        auto transaction = session.transaction(line);
        if (!NodeCommands::execute(session, rootRecord(), arguments->first().text(),
                                   arguments->mid(1), diagnostics)) {
            return false;
        }
        return transaction.commit(diagnostics);
    }

    static bool run(TestSession &session, const QString &line) {
        DiagnosticList diagnostics;
        const auto executed = run(session, line, diagnostics);
        if (!executed) {
            qDebug().noquote() << line
                               << (diagnostics.isEmpty() ? QString() : diagnostics.first().message);
        }
        return executed;
    }

    // Verifies that line is refused with an error and leaves the tree and the history unchanged.
    static void verifyRefused(TestSession &session, const QString &line) {
        const auto names = session.names();
        const auto title = session.title();
        const auto step = session.currentStep();
        DiagnosticList diagnostics;
        QVERIFY2(!run(session, line, diagnostics), qPrintable(line));
        QVERIFY2(hasError(diagnostics), qPrintable(line));
        QCOMPARE(session.names(), names);
        QCOMPARE(session.title(), title);
        QCOMPARE(session.currentStep(), step);
    }

    static std::optional<NodeCommands::Target> resolve(const TestSession &session,
                                                       const QString &path) {
        DiagnosticList diagnostics;
        return NodeCommands::resolve(session, rootRecord(), path, diagnostics);
    }

private Q_SLOTS:
    void a_path_ends_at_a_record_or_a_field() {
        TestSession session;

        const auto root = resolve(session, QStringLiteral("/"));
        QVERIFY(root);
        QCOMPARE(root->record->id(), session.root());
        QVERIFY(!root->field);

        const auto item = resolve(session, QStringLiteral("/items/1"));
        QVERIFY(item);
        QCOMPARE(item->record->id(), session.itemAt(1));
        QCOMPARE(item->info, &itemRecord());
        QVERIFY(!item->field);

        const auto values = resolve(session, QStringLiteral("/items/0/values"));
        QVERIFY(values);
        QCOMPARE(values->record->id(), session.itemAt(0));
        QCOMPARE(values->field->kind, FieldInfo::Array);

        const auto member = resolve(session, QStringLiteral("/title/a/0"));
        QVERIFY(member);
        QCOMPARE(member->field->kind, FieldInfo::Value);
        QCOMPARE(member->members, QStringList({QStringLiteral("a"), QStringLiteral("0")}));
    }

    void a_malformed_path_is_refused() {
        TestSession session;
        for (const auto &path :
             {QStringLiteral("title"), QStringLiteral("/nothing"), QStringLiteral("/items/2"),
              QStringLiteral("/items/x"), QStringLiteral("/items/-1"), QStringLiteral("/items/"),
              QStringLiteral("/items//name"), QStringLiteral("/tags/a"),
              QStringLiteral("/items/0/values/0"), QStringLiteral("/items/0/nothing")}) {
            DiagnosticList diagnostics;
            QVERIFY2(!NodeCommands::resolve(session, rootRecord(), path, diagnostics),
                     qPrintable(path));
            QVERIFY(hasError(diagnostics));
        }
    }

    void set_writes_a_value() {
        TestSession session;
        QVERIFY(run(session, QStringLiteral("set /title \"a new title\"")));
        QCOMPARE(session.title(), QStringLiteral("a new title"));
        QCOMPARE(session.undoMessage(), QStringLiteral("set /title \"a new title\""));

        QVERIFY(run(session, QStringLiteral("set /items/1/name renamed")));
        QCOMPARE(session.names(),
                 QStringList({QStringLiteral("first"), QStringLiteral("renamed")}));
    }

    // Writing the current value creates no change, therefore no undo step.
    void setting_the_current_value_creates_no_step() {
        TestSession session;
        QVERIFY(run(session, QStringLiteral("set /title title")));
        QCOMPARE(session.currentStep(), 0);
    }

    // The value of an argument does not depend on the field, therefore a word that reads as a
    // number is refused by a string field.
    void set_refuses_a_value_of_another_type() {
        TestSession session;
        verifyRefused(session, QStringLiteral("set /title 12"));
        verifyRefused(session, QStringLiteral("set /title null"));
        verifyRefused(session, QStringLiteral("set /title {\"a\": 1}"));
        QVERIFY(run(session, QStringLiteral("set /title \"12\"")));
        QCOMPARE(session.title(), QStringLiteral("12"));
    }

    // A member of a value exists only in a value written as a JSON object or array.
    void set_refuses_a_member_of_a_scalar() {
        TestSession session;
        verifyRefused(session, QStringLiteral("set /title/a b"));
    }

    void set_refuses_a_record_a_list_and_an_array() {
        TestSession session;
        verifyRefused(session, QStringLiteral("set / {}"));
        verifyRefused(session, QStringLiteral("set /items/0 {\"name\": \"a\"}"));
        verifyRefused(session, QStringLiteral("set /items []"));
        verifyRefused(session, QStringLiteral("set /items/0/values [1]"));
    }

    void set_and_remove_edit_a_mapping_by_key() {
        TestSession session;
        QVERIFY(run(session, QStringLiteral("set /tags b 2")));
        QCOMPARE(session.tagKeys(), QStringList({QStringLiteral("a"), QStringLiteral("b")}));
        QVERIFY(run(session, QStringLiteral("remove /tags a")));
        QCOMPARE(session.tagKeys(), QStringList({QStringLiteral("b")}));

        verifyRefused(session, QStringLiteral("set /tags c \"2\""));
        verifyRefused(session, QStringLiteral("set /tags 12 2"));
        verifyRefused(session, QStringLiteral("remove /tags missing"));
        verifyRefused(session, QStringLiteral("set /tags c"));
        QCOMPARE(session.tagKeys(), QStringList({QStringLiteral("b")}));
    }

    void insert_creates_items_from_their_json() {
        TestSession session;
        QVERIFY(
            run(session, QStringLiteral("insert /items 1 {\"name\": \"x\"} {\"name\": \"y\"}")));
        QCOMPARE(session.names(), QStringList({QStringLiteral("first"), QStringLiteral("x"),
                                               QStringLiteral("y"), QStringLiteral("second")}));
    }

    // An unknown field or a value of another type would be ignored or corrected by reading, and
    // is refused by a command.
    void insert_refuses_a_malformed_item() {
        TestSession session;
        verifyRefused(session, QStringLiteral("insert /items 0 {\"nam\": \"x\"}"));
        verifyRefused(session, QStringLiteral("insert /items 0 {\"name\": 1}"));
        verifyRefused(session, QStringLiteral("insert /items 0 {\"name\": \"x\", \"extra\": 1}"));
        verifyRefused(session,
                      QStringLiteral("insert /items 0 {\"name\": \"x\", \"values\": [\"a\"]}"));
        verifyRefused(session, QStringLiteral("insert /items 0 {\"name\": \"x\"} x"));
        verifyRefused(session, QStringLiteral("insert /items 3 {\"name\": \"x\"}"));
        verifyRefused(session, QStringLiteral("insert /items -1 {\"name\": \"x\"}"));
        verifyRefused(session, QStringLiteral("insert /items 0"));
    }

    void remove_and_move_edit_a_list() {
        TestSession session({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")});
        QVERIFY(run(session, QStringLiteral("move /items 0 2 1")));
        QCOMPARE(session.names(),
                 QStringList({QStringLiteral("c"), QStringLiteral("a"), QStringLiteral("b")}));
        QVERIFY(run(session, QStringLiteral("remove /items 1 2")));
        QCOMPARE(session.names(), QStringList({QStringLiteral("c")}));
        QVERIFY(run(session, QStringLiteral("remove /items 0")));
        QCOMPARE(session.names(), QStringList());
    }

    void remove_and_move_refuse_items_outside_the_list() {
        TestSession session({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")});
        verifyRefused(session, QStringLiteral("remove /items 3"));
        verifyRefused(session, QStringLiteral("remove /items 2 2"));
        verifyRefused(session, QStringLiteral("remove /items 0 0"));
        verifyRefused(session, QStringLiteral("remove /items -1"));
        verifyRefused(session, QStringLiteral("move /items 0 2 2"));
        verifyRefused(session, QStringLiteral("move /items 2 2 0"));
        verifyRefused(session, QStringLiteral("move /items 0 1 -1"));
        verifyRefused(session, QStringLiteral("move /items 0 1"));
    }

    // Moving items to their own position changes nothing.
    void moving_items_to_their_position_creates_no_step() {
        TestSession session({QStringLiteral("a"), QStringLiteral("b")});
        QVERIFY(run(session, QStringLiteral("move /items 1 1 1")));
        QCOMPARE(session.currentStep(), 0);
    }

    void insert_remove_and_replace_edit_an_array() {
        TestSession session;
        const auto array = session.valuesOf(session.itemAt(0));
        QVERIFY(run(session, QStringLiteral("insert /items/0/values 1 9 8.5")));
        QCOMPARE(session.values(array), QList<double>({1, 9, 8.5, 2, 3}));
        QVERIFY(run(session, QStringLiteral("remove /items/0/values 1 2")));
        QCOMPARE(session.values(array), QList<double>({1, 2, 3}));
        QVERIFY(run(session, QStringLiteral("replace /items/0/values 2 7 6")));
        QCOMPARE(session.values(array), QList<double>({1, 2, 7, 6}));
        QVERIFY(run(session, QStringLiteral("replace /items/0/values 4 5")));
        QCOMPARE(session.values(array), QList<double>({1, 2, 7, 6, 5}));

        verifyRefused(session, QStringLiteral("insert /items/0/values 0 a"));
        verifyRefused(session, QStringLiteral("insert /items/0/values 6 1"));
        verifyRefused(session, QStringLiteral("replace /items/0/values 6 1"));
        verifyRefused(session, QStringLiteral("remove /items/0/values 4 2"));
        QCOMPARE(session.values(array), QList<double>({1, 2, 7, 6, 5}));
    }

    void each_command_applies_to_its_kinds_of_field() {
        TestSession session;
        verifyRefused(session, QStringLiteral("insert /title 0 x"));
        verifyRefused(session, QStringLiteral("insert /tags 0 1"));
        verifyRefused(session, QStringLiteral("remove /title 0"));
        verifyRefused(session, QStringLiteral("move /tags 0 1 1"));
        verifyRefused(session, QStringLiteral("move /items/0/values 0 1 1"));
        verifyRefused(session, QStringLiteral("replace /items 0 1"));
        verifyRefused(session, QStringLiteral("remove /items/0"));
    }

    void an_unknown_command_or_a_missing_path_is_refused() {
        TestSession session;
        verifyRefused(session, QStringLiteral("rename /title x"));
        verifyRefused(session, QStringLiteral("set"));
        verifyRefused(session, QStringLiteral("set 12 x"));
    }

    // The commit validates the result, therefore a command that introduces a violation is
    // rolled back.
    void a_command_that_violates_a_constraint_is_rolled_back() {
        TestSession session;
        verifyRefused(session, QStringLiteral("set /items/0/name \"\""));
        verifyRefused(session, QStringLiteral("insert /items 0 {\"name\": \"a\"} {\"name\": \"b\"} "
                                              "{\"name\": \"c\"}"));
    }

    void names_lists_every_command() {
        QCOMPARE(
            NodeCommands::names(),
            QStringList({QStringLiteral("set"), QStringLiteral("insert"), QStringLiteral("remove"),
                         QStringLiteral("move"), QStringLiteral("replace")}));
    }
};

QTEST_APPLESS_MAIN(test_NodeCommands)

#include "test_NodeCommands.moc"
