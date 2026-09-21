#include <QtCore/QString>
#include <QtTest/QTest>

#include <hellokit/Support/TextCodec.h>

using namespace hello::kit;

class test_TextCodec : public QObject {
    Q_OBJECT

private:
    static const TextCodec &shiftJis() {
        static const TextCodec codec(QStringLiteral("Shift_JIS"));
        return codec;
    }

    static const TextCodec &utf8() {
        static const TextCodec codec(QStringLiteral("UTF-8"));
        return codec;
    }

    // Escaping and unescaping are one thing written twice, so what matters is that the pair
    // holds, not what either half does on its own.
    static void checkRoundTrip(const QString &text, const TextCodec &target) {
        const QString escaped = target.escape(text);
        QVERIFY2(target.canEncode(escaped), qPrintable(escaped)); // it must be writable
        QCOMPARE(TextCodec::unescape(escaped), text);
    }

private Q_SLOTS:
    void a_name_qt_does_not_know_gives_an_invalid_codec() {
        QVERIFY(!TextCodec(QStringLiteral("Klingon-1")).isValid());
        QVERIFY(utf8().isValid());
        QVERIFY(TextCodec().isValid()); // the system encoding
    }

    // The ones this project exists for are not in QStringConverter::Encoding. They come from ICU
    // and are reachable only by name, so resolving a name through the enum leaves every one of
    // them looking unavailable.
    void the_legacy_encodings_are_reachable_by_name() {
        for (const auto &name : {"Shift_JIS", "GBK", "Big5", "EUC-KR", "GB18030"}) {
            QVERIFY2(TextCodec(QLatin1String(name)).isValid(), name);
        }
        QCOMPARE(shiftJis().name(), QStringLiteral("Shift_JIS"));
    }

    // The code page path exists so that these four never depend on ICU being present, which on
    // Windows means depending on the operating system's version. Aliases have to land on the
    // same canonical name, because that name is what gets written into a control note.
    void the_names_that_matter_resolve_to_one_canonical_spelling() {
        const std::pair<const char *, const char *> aliases[] = {
            {"Shift_JIS",   "Shift_JIS"},
            {"shift-jis",   "Shift_JIS"},
            {"sjis",        "Shift_JIS"},
            {"cp932",       "Shift_JIS"},
            {"windows-932", "Shift_JIS"},
            {"GBK",         "GBK"      },
            {"gb2312",      "GBK"      },
            {"cp936",       "GBK"      },
            {"Big5",        "Big5"     },
            {"cp950",       "Big5"     },
            {"EUC-KR",      "EUC-KR"   },
            {"cp949",       "EUC-KR"   },
            {"GB18030",     "GB18030"  },
        };
        for (const auto &[requested, canonical] : aliases) {
            TextCodec codec{QLatin1String(requested)};
            QVERIFY2(codec.isValid(), requested);
            QCOMPARE(codec.name(), QLatin1String(canonical));
        }
    }

    // Bytes that are not valid in the chosen encoding mean the wrong encoding was chosen, and
    // the user has to be told rather than shown a page of replacement characters.
    void bytes_that_do_not_decode_are_refused_not_patched_up() {
        QVERIFY(!utf8().decode(QByteArray("\xff\xfe\xfd", 3)).has_value());

        const QByteArray japanese = shiftJis().encode(QString::fromUtf8("あいうえお"));
        QVERIFY(!japanese.isEmpty());
        const auto back = shiftJis().decode(japanese);
        QVERIFY(back.has_value());
        QCOMPARE(*back, QString::fromUtf8("あいうえお"));
    }

    // Bytes that are not valid in a legacy code page have to be refused there too, not only on
    // the paths Qt handles itself.
    void the_code_page_path_refuses_bytes_that_do_not_decode() {
        // A lead byte with nothing after it, a lead byte with a trail byte that is not one,
        // and a pair that is simply not assigned.
        QVERIFY(!shiftJis().decode(QByteArray("\x82", 1)).has_value());
        QVERIFY(!shiftJis().decode(QByteArray("\x82\x20", 2)).has_value());
        QVERIFY(!shiftJis().decode(QByteArray("\x85\x40", 2)).has_value());

        const QByteArray good = shiftJis().encode(QString::fromUtf8("あい"));
        QCOMPARE(good.size(), 4);
        const auto back = shiftJis().decode(good);
        QVERIFY(back.has_value());
        QCOMPARE(*back, QString::fromUtf8("あい"));
    }

    void what_an_encoding_can_hold_is_asked_of_the_encoding() {
        QVERIFY(shiftJis().canEncode(QString::fromUtf8("あ")));
        QVERIFY(shiftJis().canEncode(QStringLiteral("la")));
        QVERIFY(!shiftJis().canEncode(QString::fromUtf8("你"))); // simplified only
        QVERIFY(!shiftJis().canEncode(QString::fromUtf8("😀"))); // nor outside the basic plane
        QVERIFY(utf8().canEncode(QString::fromUtf8("你好 あ 😀")));
    }

    // A question mark is what Qt writes for a character it cannot hold, so a text that was
    // already a question mark must not be mistaken for one that failed.
    void a_question_mark_is_not_mistaken_for_a_failure() {
        QVERIFY(shiftJis().canEncode(QStringLiteral("?")));
        QVERIFY(shiftJis().canEncode(QStringLiteral("what?")));
    }

    // UTAU writes two things and only two: UTF-8 with a Charset line, or the writer's own code
    // page with nothing at all. So the list put to the user is short on purpose.
    void the_candidates_are_the_ones_utau_users_are_actually_on() {
        const auto candidates = TextCodec::ustCandidates();
        for (const auto &name : {"UTF-8", "Shift_JIS", "GBK", "Big5"}) {
            QVERIFY2(candidates.contains(QLatin1String(name)), name);
            QVERIFY2(TextCodec(QLatin1String(name)).isValid(), name);
        }
        QVERIFY(candidates.contains(TextCodec::systemName()));
        QVERIFY(candidates.size() <= 5);
    }

    // The system encoding has to answer with a name that can be written into a control note. Qt
    // calls its own "Locale", which tells a later reader nothing.
    void the_system_encoding_has_a_real_name() {
        const QString name = TextCodec::systemName();
        QVERIFY(!name.isEmpty());
        QVERIFY(name != QLatin1String("Locale"));
        QVERIFY(TextCodec(name).isValid());
        QCOMPARE(TextCodec().name(), TextCodec(name).name());
    }

    void a_character_the_target_cannot_hold_becomes_an_escape() {
        QCOMPARE(shiftJis().escape(QString::fromUtf8("你")), QStringLiteral("\\u4f60"));
        QCOMPARE(shiftJis().escape(QString::fromUtf8("あ")), QString::fromUtf8("あ"));
    }

    // Outside the basic plane a character is two code units, and they mean nothing apart.
    void a_character_outside_the_basic_plane_becomes_two_escapes() {
        const QString escaped = shiftJis().escape(QString::fromUtf8("😀"));
        QCOMPARE(escaped, QStringLiteral("\\ud83d\\ude00"));
        QCOMPARE(TextCodec::unescape(escaped), QString::fromUtf8("😀"));
    }

    // Without this there would be no telling an escape the lyric asked for from one the writer
    // put there, and a lyric holding \u0041 would come back as A.
    void a_backslash_in_the_text_survives() {
        QCOMPARE(shiftJis().escape(QStringLiteral("a\\b")), QStringLiteral("a\\\\b"));
        QCOMPARE(TextCodec::unescape(QStringLiteral("a\\\\b")), QStringLiteral("a\\b"));

        checkRoundTrip(QStringLiteral("\\u0041"), shiftJis());
        checkRoundTrip(QStringLiteral("\\\\"), shiftJis());
        checkRoundTrip(QStringLiteral("\\"), shiftJis());
    }

    void the_pair_holds_on_everything_worth_trying() {
        for (const auto &text : {
                 QString(),
                 QStringLiteral("la"),
                 QString::fromUtf8("あいうえお"),
                 QString::fromUtf8("你好世界"),
                 QString::fromUtf8("mixed 你 あ ascii"),
                 QString::fromUtf8("😀🎵"),
                 QStringLiteral("\\u4f60"),         // an escape written by hand
                 QStringLiteral("C:\\utau\\voice"), // a Windows path
                 QStringLiteral("?"),
                 QStringLiteral("trailing\\"),
                 QStringLiteral("\\u"),     // too short to be an escape
                 QStringLiteral("\\uZZZZ"), // not hexadecimal
             }) {
            checkRoundTrip(text, shiftJis());
            checkRoundTrip(text, utf8());
        }
    }

    // Something that is not one of the two forms came from somewhere, and guessing at it loses
    // it.
    void an_escape_that_is_not_one_is_left_alone() {
        QCOMPARE(TextCodec::unescape(QStringLiteral("\\q")), QStringLiteral("\\q"));
        QCOMPARE(TextCodec::unescape(QStringLiteral("\\u12")), QStringLiteral("\\u12"));
        QCOMPARE(TextCodec::unescape(QStringLiteral("\\")), QStringLiteral("\\"));
    }
};

QTEST_APPLESS_MAIN(test_TextCodec)

#include "test_TextCodec.moc"
