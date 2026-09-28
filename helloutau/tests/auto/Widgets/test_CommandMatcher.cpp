#include <QtTest/QTest>

#include <helloutau/Widgets/CommandMatcher.h>

using namespace hello::daw;

class test_CommandMatcher : public QObject {
    Q_OBJECT

private:
    static QStringList labelsRanked(const QString &query, const QList<CommandEntry> &entries) {
        QStringList labels;
        for (const auto &ranked : CommandMatcher::rank(query, entries)) {
            labels.push_back(entries[ranked.index].label);
        }
        return labels;
    }

    static QList<CommandEntry> entriesOf(const QStringList &labels) {
        QList<CommandEntry> entries;
        for (const auto &label : labels) {
            entries.push_back({label, label, {}, {}});
        }
        return entries;
    }

private Q_SLOTS:
    void characters_match_in_order_ignoring_case() {
        QVERIFY(CommandMatcher::match(u"fs", u"File: Save"));
        QVERIFY(CommandMatcher::match(u"FILE", u"File: Save"));
        QVERIFY(!CommandMatcher::match(u"sf", u"File: Save"));
        QVERIFY(!CommandMatcher::match(u"x", u"File: Save"));
        QVERIFY(!CommandMatcher::match(u"File: Save!", u"File: Save"));
    }

    void an_empty_query_matches_everything() {
        const auto match = CommandMatcher::match(u"", u"File: Save");
        QVERIFY(match);
        QCOMPARE(match->score, 0);
        QVERIFY(match->positions.isEmpty());
    }

    // The positions are those of the best match: "sa" at the start of "Save", not the scattered
    // s and a of "Settings".
    void the_best_placement_is_chosen() {
        const auto match = CommandMatcher::match(u"sa", u"Settings: Save");
        QVERIFY(match);
        QCOMPARE(match->positions, (QList<qsizetype>{10, 11}));
    }

    void word_starts_and_consecutive_characters_rank_first() {
        const auto entries =
            entriesOf({QStringLiteral("Tools: Settings..."), QStringLiteral("File: Save"),
                       QStringLiteral("File: Save As...")});
        QCOMPARE(labelsRanked(QStringLiteral("sa"), entries),
                 (QStringList{QStringLiteral("File: Save"), QStringLiteral("File: Save As...")}));
        QCOMPARE(labelsRanked(QStringLiteral("st"), entries).first(),
                 QStringLiteral("Tools: Settings..."));
        QCOMPARE(labelsRanked(QStringLiteral("fsa"), entries).first(),
                 QStringLiteral("File: Save"));
    }

    void entries_that_do_not_match_are_left_out() {
        const auto entries =
            entriesOf({QStringLiteral("Edit: Undo"), QStringLiteral("Edit: Redo")});
        QCOMPARE(labelsRanked(QStringLiteral("un"), entries),
                 QStringList{QStringLiteral("Edit: Undo")});
    }

    // The order does not depend on the order in which the entries were given.
    void ties_are_ordered_by_length_and_text() {
        const auto entries = entriesOf(
            {QStringLiteral("Edit: Redo"), QStringLiteral("Edit: Undo"), QStringLiteral("Edit")});
        QCOMPARE(labelsRanked(QString(), entries),
                 (QStringList{QStringLiteral("Edit"), QStringLiteral("Edit: Redo"),
                              QStringLiteral("Edit: Undo")}));
    }

    // A translated command can be found by the name it was declared with, and a Chinese label
    // by its characters.
    void the_untranslated_text_also_matches() {
        const QList<CommandEntry> entries{
            {QStringLiteral("save"),
             QString::fromUtf8("文件: 保存"),
             QStringLiteral("File: Save"),
             {}},
        };
        const auto byEnglish = CommandMatcher::rank(u"save", entries);
        QCOMPARE(byEnglish.size(), 1);
        QVERIFY(byEnglish.first().positions.isEmpty());

        const auto byChinese = CommandMatcher::rank(QString::fromUtf8("保存"), entries);
        QCOMPARE(byChinese.size(), 1);
        QCOMPARE(byChinese.first().positions, (QList<qsizetype>{4, 5}));
    }
};

QTEST_APPLESS_MAIN(test_CommandMatcher)

#include "test_CommandMatcher.moc"
