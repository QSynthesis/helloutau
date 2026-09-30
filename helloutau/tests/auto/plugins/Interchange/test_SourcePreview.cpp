#include <QtTest/QTest>

#include <hellokit/Support/TextCodec.h>

#include <Interchange/SourcePreview.h>

using namespace hello;
using namespace hello::daw;

namespace {

    QByteArray encoded(const char *encoding, const QString &text) {
        return kit::TextCodec(QString::fromLatin1(encoding)).encode(text);
    }

    kit::InterchangeSource sourceOf(const QByteArray &name, const QList<QByteArray> &lyrics,
                                    const QList<QByteArray> &labels = {}) {
        kit::InterchangeSource source;
        kit::InterchangeEntry entry;
        entry.index = 0;
        entry.rawName = name;
        entry.rawLyrics = lyrics;
        source.entries.push_back(entry);
        source.rawLabels = labels;
        return source;
    }

}

class test_SourcePreview : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void the_texts_are_names_lyrics_and_labels() {
        const auto source = sourceOf("n", {"a", "b"}, {"m"});
        QCOMPARE(SourcePreview::textsOf(source), QList<QByteArray>({"n", "a", "b", "m"}));
    }

    void an_encoding_decodes_or_not() {
        const auto sjis = encoded("Shift_JIS", QStringLiteral("あ"));
        QVERIFY(SourcePreview::decodes({sjis}, QStringLiteral("Shift_JIS")));
        QVERIFY(!SourcePreview::decodes({"a", sjis}, QStringLiteral("UTF-8")));
        QVERIFY(!SourcePreview::decodes({"a"}, QStringLiteral("no such encoding")));
        QVERIFY(!SourcePreview::decodes({}, QStringLiteral("no such encoding")));
    }

    // UTF-8 is the default if it decodes every text, else the best ranked candidate.
    void the_default_is_utf8_or_else_ranked() {
        const QStringList candidates = {QStringLiteral("UTF-8"), QStringLiteral("Shift_JIS")};
        QCOMPARE(SourcePreview::defaultEncoding({"la", encoded("UTF-8", QStringLiteral("あ"))},
                                                candidates),
                 QStringLiteral("UTF-8"));
        QCOMPARE(SourcePreview::defaultEncoding(
                     {encoded("Shift_JIS", QStringLiteral("あいうえお"))}, candidates),
                 QStringLiteral("Shift_JIS"));
    }

    // Before the ranking, the system encoding is the default if it decodes every text.
    void the_system_encoding_precedes_the_ranking() {
        // Shift_JIS text, which ISO 8859-1 decodes into control characters. EUC-KR rejects the
        // single half-width katakana byte, which EUC-KR reads as an incomplete lead byte.
        const QList<QByteArray> texts = {encoded("Shift_JIS", QStringLiteral("あいうえお")),
                                         encoded("Shift_JIS", QStringLiteral("ｱ"))};
        QVERIFY(!SourcePreview::decodes(texts, QStringLiteral("EUC-KR")));
        const QStringList candidates = {QStringLiteral("UTF-8"), QStringLiteral("Shift_JIS"),
                                        QStringLiteral("ISO 8859-1"), QStringLiteral("EUC-KR")};
        QCOMPARE(SourcePreview::defaultEncoding(texts, candidates, QStringLiteral("ISO 8859-1")),
                 QStringLiteral("ISO 8859-1"));
        QCOMPARE(SourcePreview::defaultEncoding(texts, candidates, QStringLiteral("EUC-KR")),
                 QStringLiteral("Shift_JIS"));
        QCOMPARE(SourcePreview::defaultEncoding(texts, candidates, QStringLiteral("GBK")),
                 QStringLiteral("Shift_JIS"));
    }

    // The preview shows each text decoded, and a marker for a text that is invalid.
    void the_preview_marks_invalid_text() {
        const auto source = sourceOf(encoded("Shift_JIS", QStringLiteral("歌")),
                                     {"la", encoded("Shift_JIS", QStringLiteral("あ"))}, {"cue"});
        const auto sjis = SourcePreview::preview(source, QStringLiteral("Shift_JIS"));
        QVERIFY(sjis.contains(QStringLiteral("歌")));
        QVERIFY(sjis.contains(QStringLiteral("la あ")));
        QVERIFY(sjis.contains(QStringLiteral("cue")));
        const auto utf8 = SourcePreview::preview(source, QStringLiteral("UTF-8"));
        QVERIFY(!utf8.contains(QStringLiteral("歌")));
        QVERIFY(utf8.contains(QStringLiteral("(not valid in this encoding)")));
        QVERIFY(utf8.contains(QStringLiteral("la ")));
    }
};

QTEST_APPLESS_MAIN(test_SourcePreview)

#include "test_SourcePreview.moc"
