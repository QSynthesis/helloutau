#include <QtTest/QTest>

#include <hellokit/VoiceBank/VoiceAliasRule.h>

using namespace hello::kit;

namespace {

    VoiceAliasRule rule(VoiceAliasRule::Kind kind, const char *text, const char *replacement = "") {
        VoiceAliasRule rule;
        rule.kind = kind;
        rule.text = QString::fromUtf8(text);
        rule.replacement = QString::fromUtf8(replacement);
        return rule;
    }

    QStringList aliasesOf(const QList<VoiceAliasRule::Change> &changes) {
        QStringList aliases;
        for (const auto &change : changes) {
            aliases.push_back(change.to);
        }
        return aliases;
    }

    QList<bool> problemsOf(const QList<VoiceAliasRule::Change> &changes) {
        QList<bool> problems;
        for (const auto &change : changes) {
            problems.push_back(!change.problem.isEmpty());
        }
        return problems;
    }

}

class test_VoiceAliasRule : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void each_kind_derives_an_alias() {
        QCOMPARE(rule(VoiceAliasRule::AddPrefix, "- ").apply("a"), QStringLiteral("- a"));
        QCOMPARE(rule(VoiceAliasRule::AddSuffix, "_2").apply("a"), QStringLiteral("a_2"));
        QCOMPARE(rule(VoiceAliasRule::Replace, "a", "i").apply("a a"), QStringLiteral("i i"));
        QCOMPARE(rule(VoiceAliasRule::Replace, "A", "i").apply("a"), QStringLiteral("a"));
        QCOMPARE(rule(VoiceAliasRule::RemovePrefix, "- ").apply("- a"), QStringLiteral("a"));
        QCOMPARE(rule(VoiceAliasRule::RemovePrefix, "- ").apply("a -"), QStringLiteral("a -"));
        QCOMPARE(rule(VoiceAliasRule::RemoveSuffix, "_2").apply("a_2"), QStringLiteral("a"));
        QCOMPARE(rule(VoiceAliasRule::RemoveSuffix, "_2").apply("_2a"), QStringLiteral("_2a"));
        QCOMPARE(rule(VoiceAliasRule::AddSuffix, "").apply("a"), QStringLiteral("a"));
        // An empty text would otherwise match between every two characters.
        QCOMPARE(rule(VoiceAliasRule::Replace, "", "z").apply("ab"), QStringLiteral("ab"));
    }

    // An empty alias denotes the stem of the file name.
    void an_empty_alias_is_the_stem() {
        QCOMPARE(VoiceAliasRule::nameOf("ka.wav", ""), QStringLiteral("ka"));
        QCOMPARE(VoiceAliasRule::nameOf("ka.wav", "- ka"), QStringLiteral("- ka"));
        const auto changes = rule(VoiceAliasRule::AddPrefix, "- ")
                                 .plan(
                                     {
                                         {"ka.wav", ""}
        },
                                     {0}, false);
        QCOMPARE(changes.size(), 1);
        QCOMPARE(changes[0].from, QStringLiteral("ka"));
        QCOMPARE(changes[0].to, QStringLiteral("- ka"));
    }

    // A renaming conflicts with the unselected entries of the same file and with the other
    // renamings, and not with entries of other files.
    void a_renaming_conflicts_with_equal_names() {
        const QList<VoiceAliasRule::Entry> entries = {
            {"a.wav", "x"  },
            {"a.wav", "x_2"},
            {"a.wav", "y"  },
            {"b.wav", "y_2"},
        };
        const auto suffix = rule(VoiceAliasRule::AddSuffix, "_2");
        auto changes = suffix.plan(entries, {0, 2}, false);
        QCOMPARE(aliasesOf(changes), QStringList({"x_2", "y_2"}));
        QCOMPARE(problemsOf(changes), QList<bool>({true, false}));

        // x_2 is free once x_2 itself is renamed.
        changes = suffix.plan(entries, {0, 1}, false);
        QCOMPARE(aliasesOf(changes), QStringList({"x_2", "x_2_2"}));
        QCOMPARE(problemsOf(changes), QList<bool>({false, false}));

        // Two renamings to one name conflict with each other.
        changes = rule(VoiceAliasRule::Replace, "_2", "")
                      .plan(
                          {
                              {"a.wav", "x"  },
                              {"a.wav", "x_2"},
                              {"a.wav", "z"  }
        },
                          {0, 1}, false);
        QCOMPARE(problemsOf(changes), QList<bool>({true, true}));
    }

    // A copy conflicts with every entry of the same file, its original included.
    void a_copy_conflicts_with_every_entry() {
        const QList<VoiceAliasRule::Entry> entries = {
            {"a.wav", "x"  },
            {"a.wav", "x_2"}
        };
        auto changes = rule(VoiceAliasRule::AddSuffix, "_2").plan(entries, {0, 1}, true);
        QCOMPARE(aliasesOf(changes), QStringList({"x_2", "x_2_2"}));
        QCOMPARE(problemsOf(changes), QList<bool>({true, false}));

        changes = rule(VoiceAliasRule::RemoveSuffix, "_9").plan(entries, {0}, true);
        QCOMPARE(problemsOf(changes), QList<bool>({true}));
    }

    // An unchanged renaming is no problem, and an empty alias is one.
    void unchanged_and_empty_aliases() {
        const QList<VoiceAliasRule::Entry> entries = {
            {"a.wav", "x" },
            {"a.wav", "- "}
        };
        const auto changes = rule(VoiceAliasRule::RemovePrefix, "- ").plan(entries, {0, 1}, false);
        QCOMPARE(aliasesOf(changes), QStringList({"x", ""}));
        QCOMPARE(problemsOf(changes), QList<bool>({false, true}));
        QCOMPARE(changes[0].from, changes[0].to);
    }

    // Indices outside the list and repeated indices are ignored.
    void invalid_indices_are_ignored() {
        const auto changes = rule(VoiceAliasRule::AddSuffix, "_2")
                                 .plan(
                                     {
                                         {"a.wav", "x"}
        },
                                     {-1, 0, 0, 3}, false);
        QCOMPARE(changes.size(), 1);
        QCOMPARE(changes[0].index, 0);
    }

    // Given aliases are checked as the aliases of a rule, and the from of each change is set
    // to the name that it replaces.
    void given_aliases_are_checked() {
        const QList<VoiceAliasRule::Entry> entries = {
            {"a.wav", "x"},
            {"a.wav", "y"},
            {"b.wav", ""},
        };
        VoiceAliasRule::Change toY;
        toY.index = 0;
        toY.to = QStringLiteral("y");
        VoiceAliasRule::Change toZ;
        toZ.index = 2;
        toZ.to = QStringLiteral("z");
        const auto changes = VoiceAliasRule::check(entries, {toY, toZ}, false);
        QCOMPARE(changes[0].from, QStringLiteral("x"));
        QCOMPARE(changes[1].from, QStringLiteral("b"));
        QCOMPARE(problemsOf(changes), QList<bool>({true, false}));
    }
};

QTEST_APPLESS_MAIN(test_VoiceAliasRule)

#include "test_VoiceAliasRule.moc"
