#include <QtTest/QTest>

#include <hellokit/Support/TextSearch.h>

using namespace hello::kit;

class test_TextSearch : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // An empty pattern would match everywhere. It matches nothing instead, as the find widget
    // of VS Code reports no results for an empty query.
    void an_empty_pattern_matches_nothing() {
        const TextSearch search;
        QVERIFY(!search.isValid());
        QVERIFY(!search.matches(QStringLiteral("a")));
        QCOMPARE(search.replaced(QStringLiteral("a"), QStringLiteral("b")), QStringLiteral("a"));
        QVERIFY(!TextSearch(QString(), TextSearch::RegularExpression).isValid());
    }

    void a_literal_pattern_matches_its_text_exactly() {
        const TextSearch search(QStringLiteral("a.(b"), TextSearch::NoOption);
        QVERIFY(search.isValid());
        QVERIFY(search.matches(QStringLiteral("xa.(by")));
        QVERIFY(!search.matches(QStringLiteral("xaz(by")));
    }

    void the_case_is_ignored_unless_it_is_required() {
        QVERIFY(
            TextSearch(QStringLiteral("ka"), TextSearch::NoOption).matches(QStringLiteral("KA")));
        QVERIFY(!TextSearch(QStringLiteral("ka"), TextSearch::CaseSensitive)
                     .matches(QStringLiteral("KA")));
        QVERIFY(TextSearch(QStringLiteral("ka"), TextSearch::CaseSensitive)
                    .matches(QStringLiteral("ka")));
    }

    // Word characters include those of every script, so that a kana lyric followed by another
    // kana is not a whole word.
    void a_whole_word_is_bounded_by_non_word_characters() {
        const TextSearch search(QStringLiteral("a"), TextSearch::WholeWord);
        QVERIFY(search.matches(QStringLiteral("a")));
        QVERIFY(search.matches(QStringLiteral("- a")));
        QVERIFY(!search.matches(QStringLiteral("ka")));
        QVERIFY(!search.matches(QStringLiteral("a_")));

        const TextSearch kana(QString::fromUtf8("\xe3\x81\x82"), TextSearch::WholeWord);
        QVERIFY(kana.matches(QString::fromUtf8("- \xe3\x81\x82")));
        QVERIFY(!kana.matches(QString::fromUtf8("\xe3\x81\x82\xe3\x81\x84")));
    }

    // The group added for whole words must not take the place of the first group of the
    // pattern.
    void a_whole_word_expression_keeps_its_groups() {
        const TextSearch search(QStringLiteral("(a)|b"),
                                TextSearch::RegularExpression | TextSearch::WholeWord);
        QVERIFY(search.matches(QStringLiteral("b")));
        QVERIFY(!search.matches(QStringLiteral("ab")));
        QCOMPARE(search.replaced(QStringLiteral("x a"), QStringLiteral("[$1]")),
                 QStringLiteral("x [a]"));
    }

    void an_invalid_expression_is_reported() {
        const TextSearch search(QStringLiteral("a)"), TextSearch::RegularExpression);
        QVERIFY(!search.isValid());
        QVERIFY(!search.errorString().isEmpty());
        QVERIFY(!search.matches(QStringLiteral("a)")));

        // The same text is a valid literal pattern.
        QVERIFY(TextSearch(QStringLiteral("a)"), TextSearch::NoOption).isValid());
    }

    // The group of whole words would balance the parenthesis if the pattern were not checked
    // alone.
    void an_unbalanced_expression_is_invalid_with_whole_words() {
        const TextSearch search(QStringLiteral("a)(?:b"),
                                TextSearch::RegularExpression | TextSearch::WholeWord);
        QVERIFY(!search.isValid());
    }

    void every_match_is_found_without_overlap() {
        const TextSearch search(QStringLiteral("aa"), TextSearch::NoOption);
        const QList<TextSearch::Match> expected{
            {0, 2},
            {2, 2}
        };
        QCOMPARE(search.matchesIn(QStringLiteral("aaaab")), expected);
    }

    void every_match_is_replaced() {
        const TextSearch search(QStringLiteral("a"), TextSearch::NoOption);
        QCOMPARE(search.replaced(QStringLiteral("a ka a"), QStringLiteral("o")),
                 QStringLiteral("o ko o"));
    }

    // A literal pattern inserts its replacement as written, dollar signs included.
    void a_literal_replacement_is_not_expanded() {
        const TextSearch search(QStringLiteral("a"), TextSearch::NoOption);
        QCOMPARE(search.replaced(QStringLiteral("a"), QStringLiteral("$0$$")),
                 QStringLiteral("$0$$"));
    }

    void groups_are_inserted_by_number() {
        const TextSearch search(QStringLiteral("(k)(a)"), TextSearch::RegularExpression);
        QCOMPARE(search.replaced(QStringLiteral("ka"), QStringLiteral("$2$1 $0 $$1 $3 $")),
                 QStringLiteral("ak ka $1 $3 $"));
    }

    // $10 denotes the tenth group only if the expression has ten groups. Otherwise it is the
    // first group followed by a zero.
    void two_digits_denote_a_group_only_if_it_exists() {
        const TextSearch one(QStringLiteral("(a)"), TextSearch::RegularExpression);
        QCOMPARE(one.replaced(QStringLiteral("a"), QStringLiteral("$10")), QStringLiteral("a0"));

        const TextSearch ten(QStringLiteral("(a)(b)(c)(d)(e)(f)(g)(h)(i)(j)"),
                             TextSearch::RegularExpression);
        QCOMPARE(ten.replaced(QStringLiteral("abcdefghij"), QStringLiteral("$10$1")),
                 QStringLiteral("ja"));
    }

    // An empty match lets a replacement add text, as ^ adds a prefix to every lyric.
    void an_empty_match_inserts_the_replacement() {
        const TextSearch search(QStringLiteral("^"), TextSearch::RegularExpression);
        QVERIFY(search.matches(QStringLiteral("a")));
        QCOMPARE(search.replaced(QStringLiteral("a"), QStringLiteral("- ")), QStringLiteral("- a"));
    }
};

QTEST_APPLESS_MAIN(test_TextSearch)

#include "test_TextSearch.moc"
