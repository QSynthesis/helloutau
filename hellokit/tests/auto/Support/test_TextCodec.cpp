#include <QtCore/QString>
#include <QtCore/QStringDecoder>
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

    // Returns whether GB18030 is available on this system. GB18030 is not an ANSI code page, so
    // winacp does not provide it. It is converted by the Windows code page functions on Windows,
    // and elsewhere only by ICU. The Qt distribution for macOS is built without ICU, so GB18030
    // is unavailable there and must be reported as such rather than converted incorrectly.
    static bool gb18030Available() {
#ifdef _WIN32
        return true;
#else
        return QStringDecoder("GB18030").isValid();
#endif
    }

    // Escaping and unescaping are inverse operations, so the requirement under test is the
    // round-trip property rather than the behavior of either half in isolation.
    static void checkRoundTrip(const QString &text, const TextCodec &target) {
        const QString escaped = target.escape(text);
        QVERIFY2(target.canEncode(escaped), qPrintable(escaped)); // must be representable
        QCOMPARE(TextCodec::unescape(escaped), text);
    }

private Q_SLOTS:
    // The encoding a text was written in reads best of the candidates, and bytes that read
    // alike in all keep the order given.
    void the_encoding_of_a_text_reads_best_data() {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("charset");

        QTest::newRow("Japanese in Shift_JIS")
            << QString::fromUtf8("あ.wav=- あ,6,52,-400,31,3\r\nか.wav=- か,12,90,-300,60,20\r\n"
                                 "歌声合成のための音源です。")
            << QStringLiteral("Shift_JIS");
        QTest::newRow("Chinese in GBK")
            << QString::fromUtf8("a.wav=啊,6,52,-400,31,3\r\n这是一个中文音源，用于歌声合成。"
                                 "请在使用前阅读说明。")
            << QStringLiteral("GBK");
        QTest::newRow("Chinese in Big5")
            << QString::fromUtf8("a.wav=啊,6,52,-400,31,3\r\n這是一個中文音源，用於歌聲合成。"
                                 "請在使用前閱讀說明。")
            << QStringLiteral("Big5");
        QTest::newRow("Japanese in UTF-8")
            << QString::fromUtf8("あ.wav=- あ,6,52,-400,31,3\r\n歌声合成のための音源です。")
            << QStringLiteral("UTF-8");
        QTest::newRow("Chinese in UTF-8")
            << QString::fromUtf8("这是一个中文音源，用于歌声合成。") << QStringLiteral("UTF-8");
    }

    void the_encoding_of_a_text_reads_best() {
        QFETCH(QString, text);
        QFETCH(QString, charset);
        const auto candidates = QStringList{QStringLiteral("UTF-8"), QStringLiteral("Shift_JIS"),
                                            QStringLiteral("GBK"), QStringLiteral("Big5")};
        const auto bytes = TextCodec(charset).encode(text);
        QCOMPARE(TextCodec::ranked({bytes}, candidates).first(), charset);
    }

    // A character of the private use area counts against the encoding, and one of valid UTF-8
    // for UTF-8, even a Latin-1 letter.
    void misread_and_utf8_characters_count() {
        QVERIFY(TextCodec::plausibility({QByteArray("\xaa\xa1")}, QStringLiteral("GBK")) < 0);
        QVERIFY(TextCodec::plausibility({QByteArray("\xc3\xa9")}, QStringLiteral("UTF-8")) > 0);
    }

    void ascii_keeps_the_order_given() {
        const auto candidates = QStringList{QStringLiteral("Shift_JIS"), QStringLiteral("UTF-8"),
                                            QStringLiteral("GBK")};
        QCOMPARE(TextCodec::ranked({QByteArray("a.wav=a,6,52,-400,31,3\r\n")}, candidates),
                 candidates);
        // Several files count together, and an unknown name comes last.
        const auto japanese = TextCodec(QStringLiteral("Shift_JIS"))
                                  .encode(QString::fromUtf8("あいうえお、かきくけこ"));
        QCOMPARE(TextCodec::ranked({QByteArray("ascii"), japanese},
                                   {QStringLiteral("no-such-code"), QStringLiteral("GBK"),
                                    QStringLiteral("Shift_JIS")}),
                 (QStringList{QStringLiteral("Shift_JIS"), QStringLiteral("GBK"),
                              QStringLiteral("no-such-code")}));
    }

    void an_unknown_name_gives_an_invalid_codec() {
        QVERIFY(!TextCodec(QStringLiteral("Klingon-1")).isValid());
        QVERIFY(utf8().isValid());
        QVERIFY(TextCodec().isValid()); // the system encoding
    }

    // The legacy encodings required by this project are absent from QStringConverter::Encoding
    // and are reachable only by name, so resolving a name through the enumeration would make
    // all of them appear unavailable.
    void the_legacy_encodings_are_reachable_by_name() {
        for (const auto &name : {"Shift_JIS", "GBK", "Big5", "EUC-KR"}) {
            QVERIFY2(TextCodec(QLatin1String(name)).isValid(), name);
        }
        QCOMPARE(TextCodec(QStringLiteral("GB18030")).isValid(), gb18030Available());
        QCOMPARE(shiftJis().name(), QStringLiteral("Shift_JIS"));
    }

    // These encodings are converted without ICU, which is unavailable in some Qt builds and
    // depends on the operating system version on Windows. Every alias must resolve to the same
    // canonical name, because that name is recorded in the control note.
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
        };
        for (const auto &[requested, canonical] : aliases) {
            TextCodec codec{QLatin1String(requested)};
            QVERIFY2(codec.isValid(), requested);
            QCOMPARE(codec.name(), QLatin1String(canonical));
        }
        if (gb18030Available()) {
            QCOMPARE(TextCodec(QStringLiteral("GB18030")).name(), QStringLiteral("GB18030"));
        }
    }

    // Invalid bytes indicate an incorrect encoding choice, which must be reported to the user
    // instead of displaying replacement characters.
    void bytes_that_do_not_decode_are_refused_not_patched_up() {
        QVERIFY(!utf8().decode(QByteArray("\xff\xfe\xfd", 3)).has_value());

        const QByteArray japanese = shiftJis().encode(QString::fromUtf8("あいうえお"));
        QVERIFY(!japanese.isEmpty());
        const auto back = shiftJis().decode(japanese);
        QVERIFY(back.has_value());
        QCOMPARE(*back, QString::fromUtf8("あいうえお"));
    }

    // Invalid bytes must also be rejected on the code page path, not only on the Qt paths.
    void the_code_page_path_rejects_invalid_byte_sequences() {
        // A truncated sequence, a lead byte followed by an invalid trail byte, and an
        // unassigned pair.
        QVERIFY(!shiftJis().decode(QByteArray("\x82", 1)).has_value());
        QVERIFY(!shiftJis().decode(QByteArray("\x82\x20", 2)).has_value());
        QVERIFY(!shiftJis().decode(QByteArray("\x85\x40", 2)).has_value());

        const QByteArray good = shiftJis().encode(QString::fromUtf8("あい"));
        QCOMPARE(good.size(), 4);
        const auto back = shiftJis().decode(good);
        QVERIFY(back.has_value());
        QCOMPARE(*back, QString::fromUtf8("あい"));
    }

    // A sequence cut off at the end of the bytes is invalid, not left pending for more bytes.
    void a_truncated_sequence_is_invalid() {
        QVERIFY(!utf8().decode(QByteArray("\xe3\x81", 2)).has_value());
        QVERIFY(!utf8().decode(QByteArray("a\xc3", 2)).has_value());
    }

    // Each invalid sequence becomes one replacement character, and decoding resumes after it,
    // so that the valid text around it survives.
    void replacing_keeps_the_valid_text_and_counts_the_invalid_sequences() {
        qsizetype invalid = -1;
        const QByteArray japanese = shiftJis().encode(QString::fromUtf8("あ")) +
                                    QByteArray("\x82\x20", 2) +
                                    shiftJis().encode(QString::fromUtf8("い"));
        QCOMPARE(shiftJis().decodeReplacing(japanese, &invalid),
                 QString::fromUtf8("あ\xef\xbf\xbd い"));
        QCOMPARE(invalid, 1);

        // A stray byte, a valid two-byte character, and a three-byte character cut off after
        // its second byte, whose two bytes are each invalid.
        QCOMPARE(utf8().decodeReplacing(QByteArray("a\xff\xc3\xa9\xe3\x81", 6), &invalid),
                 QString::fromUtf8("a\xef\xbf\xbd\xc3\xa9\xef\xbf\xbd\xef\xbf\xbd"));
        QCOMPARE(invalid, 3);

        // Valid bytes decode as by decode(), with nothing counted.
        QCOMPARE(shiftJis().decodeReplacing(japanese.left(2), &invalid), QString::fromUtf8("あ"));
        QCOMPARE(invalid, 0);
        QVERIFY(TextCodec(QStringLiteral("Klingon-1")).decodeReplacing("abc").isEmpty());
    }

    // Every ANSI code page must be available, so that the system encoding of any Windows
    // machine is supported, on every system rather than only on Windows.
    void every_ansi_code_page_is_reachable_by_name() {
        for (const auto *name :
             {"windows-874", "windows-1250", "windows-1251", "windows-1252", "windows-1253",
              "windows-1254", "windows-1255", "windows-1256", "windows-1257", "windows-1258"}) {
            const TextCodec codec{QLatin1String(name)};
            QVERIFY2(codec.isValid(), name);
            QCOMPARE(codec.name(), QLatin1String(name));
        }
        // Windows-1252, not Latin-1, which lacks the euro sign.
        QCOMPARE(TextCodec(QStringLiteral("windows-1252")).decode(QByteArray("\x80", 1)),
                 QString(QChar(0x20AC)));
    }

    // UTAU encodes a character with both an NEC and an IBM encoding as Windows does. This
    // implementation does the same, regardless of the sequence it was decoded from, so that an
    // oto.ini saved here contains the bytes UTAU would have written.
    void a_character_with_two_spellings_is_written_as_utau_writes_it() {
        const QString kanji(QChar(0x7E8A));
        QCOMPARE(shiftJis().decode(QByteArray("\xed\x40", 2)), kanji);
        QCOMPARE(shiftJis().decode(QByteArray("\xfa\x5c", 2)), kanji);
        QCOMPARE(shiftJis().encode(kanji), QByteArray("\xfa\x5c", 2));
    }

    // Windows maps the user-defined rows into the Private Use Area, and a voice bank that uses
    // them must be readable on every system.
    void the_user_defined_rows_are_read() {
        const QString first(QChar(0xE000));
        QCOMPARE(TextCodec(QStringLiteral("Big5")).decode(QByteArray("\xfa\x40", 2)), first);
        QCOMPARE(TextCodec(QStringLiteral("GBK")).decode(QByteArray("\xaa\xa1", 2)), first);
        QCOMPARE(shiftJis().decode(QByteArray("\xf0\x40", 2)), first);
    }

    void what_an_encoding_can_hold_is_asked_of_the_encoding() {
        QVERIFY(shiftJis().canEncode(QString::fromUtf8("あ")));
        QVERIFY(shiftJis().canEncode(QStringLiteral("la")));
        QVERIFY(!shiftJis().canEncode(QString::fromUtf8("你"))); // simplified Chinese only
        QVERIFY(!shiftJis().canEncode(QString::fromUtf8("😀"))); // outside the BMP
        QVERIFY(utf8().canEncode(QString::fromUtf8("你好 あ 😀")));
    }

    // Qt substitutes a question mark for an unrepresentable character, so a literal question
    // mark must not be mistaken for a failed conversion.
    void a_question_mark_is_not_mistaken_for_a_failure() {
        QVERIFY(shiftJis().canEncode(QStringLiteral("?")));
        QVERIFY(shiftJis().canEncode(QStringLiteral("what?")));
    }

    // UTAU writes exactly two kinds of file: UTF-8 with a Charset line, or the ANSI code page of
    // the writing machine without a declaration. The list offered to the user is therefore
    // deliberately short.
    void the_candidates_are_the_ones_utau_users_are_actually_on() {
        const auto candidates = TextCodec::ustCandidates();
        for (const auto &name : {"UTF-8", "Shift_JIS", "GBK", "Big5"}) {
            QVERIFY2(candidates.contains(QLatin1String(name)), name);
            QVERIFY2(TextCodec(QLatin1String(name)).isValid(), name);
        }
        QVERIFY(candidates.contains(TextCodec::systemName()));
        QVERIFY(candidates.size() <= 5);
    }

    // The system encoding must resolve to a name that can be recorded in a control note. Qt
    // names it "Locale", which is meaningless to a later reader.
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

    // A character outside the Basic Multilingual Plane is a surrogate pair, whose units are
    // meaningless in isolation.
    void a_character_outside_the_basic_plane_becomes_two_escapes() {
        const QString escaped = shiftJis().escape(QString::fromUtf8("😀"));
        QCOMPARE(escaped, QStringLiteral("\\ud83d\\ude00"));
        QCOMPARE(TextCodec::unescape(escaped), QString::fromUtf8("😀"));
    }

    // Otherwise an escape sequence in the lyric could not be distinguished from one inserted by
    // the writer, and a lyric containing \u0041 would be read back as A.
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
                 QStringLiteral("\\u4f60"),         // an escape sequence typed by the user
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

    // A backslash sequence matching neither form is part of the original text, and
    // reinterpreting it would lose data.
    void an_escape_that_is_not_one_is_left_alone() {
        QCOMPARE(TextCodec::unescape(QStringLiteral("\\q")), QStringLiteral("\\q"));
        QCOMPARE(TextCodec::unescape(QStringLiteral("\\u12")), QStringLiteral("\\u12"));
        QCOMPARE(TextCodec::unescape(QStringLiteral("\\")), QStringLiteral("\\"));
    }
};

QTEST_APPLESS_MAIN(test_TextCodec)

#include "test_TextCodec.moc"
