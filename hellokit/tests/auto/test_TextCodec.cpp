#define BOOST_TEST_MAIN
#define BOOST_TEST_MODULE test_TextCodec

#include <boost/test/unit_test.hpp>

#include <QtCore/QString>

#include <hellokit/Support/TextCodec.h>

using namespace hello::kit;

namespace {

    const TextCodec &shiftJis() {
        static const TextCodec codec(QStringLiteral("Shift_JIS"));
        return codec;
    }

    const TextCodec &utf8() {
        static const TextCodec codec(QStringLiteral("UTF-8"));
        return codec;
    }

    QString u(const char *utf8Text) {
        return QString::fromUtf8(utf8Text);
    }

    // Escaping and unescaping are one thing written twice, so what matters is that the pair
    // holds, not what either half does on its own.
    void checkRoundTrip(const QString &text, const TextCodec &target) {
        const QString escaped = target.escape(text);
        BOOST_TEST_CONTEXT("escaped: " << escaped.toStdString()) {
            BOOST_CHECK(target.canEncode(escaped)); // the escaped form must be writable
            BOOST_CHECK(TextCodec::unescape(escaped) == text);
        }
    }

}

BOOST_AUTO_TEST_SUITE(test_TextCodec)

BOOST_AUTO_TEST_CASE(a_name_qt_does_not_know_gives_an_invalid_codec) {
    BOOST_CHECK(!TextCodec(QStringLiteral("Klingon-1")).isValid());
    BOOST_CHECK(utf8().isValid());
    BOOST_CHECK(TextCodec().isValid()); // the system encoding
}

// The ones this project exists for are not in QStringConverter::Encoding. They come from ICU and
// are reachable only by name, so resolving a name through the enum leaves every one of them
// looking unavailable.
BOOST_AUTO_TEST_CASE(the_legacy_encodings_are_reachable_by_name) {
    for (const auto &name : {"Shift_JIS", "GBK", "Big5", "EUC-KR", "GB18030"}) {
        BOOST_TEST_CONTEXT(name) {
            BOOST_CHECK(TextCodec(QLatin1String(name)).isValid());
        }
    }
    BOOST_CHECK_EQUAL(shiftJis().name().toStdString(), "Shift_JIS");
}

// Bytes that are not valid in the chosen encoding mean the wrong encoding was chosen, and the
// user has to be told rather than shown a page of replacement characters.
BOOST_AUTO_TEST_CASE(bytes_that_do_not_decode_are_refused_not_patched_up) {
    const QByteArray notUtf8("\xff\xfe\xfd", 3);
    BOOST_CHECK(!utf8().decode(notUtf8).has_value());

    const QByteArray japanese = shiftJis().encode(u("あいうえお"));
    BOOST_REQUIRE(!japanese.isEmpty());
    auto back = shiftJis().decode(japanese);
    BOOST_REQUIRE(back.has_value());
    BOOST_CHECK(*back == u("あいうえお"));
}

BOOST_AUTO_TEST_CASE(what_an_encoding_can_hold_is_asked_of_the_encoding) {
    BOOST_CHECK(shiftJis().canEncode(u("あ")));
    BOOST_CHECK(shiftJis().canEncode(QStringLiteral("la")));
    BOOST_CHECK(!shiftJis().canEncode(u("你")));  // simplified only, no Shift_JIS spelling
    BOOST_CHECK(!shiftJis().canEncode(u("😀"))); // nor outside the basic plane
    BOOST_CHECK(utf8().canEncode(u("你好 あ 😀")));
}

// A question mark is what Qt writes for a character it cannot hold, so a text that was already
// a question mark must not be mistaken for one that failed.
BOOST_AUTO_TEST_CASE(a_question_mark_is_not_mistaken_for_a_failure) {
    BOOST_CHECK(shiftJis().canEncode(QStringLiteral("?")));
    BOOST_CHECK(shiftJis().canEncode(QStringLiteral("what?")));
}

BOOST_AUTO_TEST_CASE(a_character_the_target_cannot_hold_becomes_an_escape) {
    BOOST_CHECK_EQUAL(shiftJis().escape(u("你")).toStdString(), "\\u4f60");
    BOOST_CHECK(shiftJis().escape(u("あ")) == u("あ")); // it can hold this one
}

// Outside the basic plane a character is two code units, and they mean nothing apart.
BOOST_AUTO_TEST_CASE(a_character_outside_the_basic_plane_becomes_two_escapes) {
    const QString escaped = shiftJis().escape(u("😀"));
    BOOST_CHECK_EQUAL(escaped.toStdString(), "\\ud83d\\ude00");
    BOOST_CHECK(TextCodec::unescape(escaped) == u("😀"));
}

// Without this there would be no telling an escape the lyric asked for from one the writer put
// there, and a lyric holding \u0041 would come back as A.
BOOST_AUTO_TEST_CASE(a_backslash_in_the_text_survives) {
    BOOST_CHECK_EQUAL(shiftJis().escape(QStringLiteral("a\\b")).toStdString(), "a\\\\b");
    BOOST_CHECK(TextCodec::unescape(QStringLiteral("a\\\\b")) == QStringLiteral("a\\b"));

    checkRoundTrip(QStringLiteral("\\u0041"), shiftJis());
    checkRoundTrip(QStringLiteral("\\\\"), shiftJis());
    checkRoundTrip(QStringLiteral("\\"), shiftJis());
}

BOOST_AUTO_TEST_CASE(the_pair_holds_on_everything_worth_trying) {
    for (const auto &text : {
             u(""),
             u("la"),
             u("あいうえお"),
             u("你好世界"),
             u("mixed 你 あ ascii"),
             u("😀🎵"),
             u("\\u4f60"),      // an escape written by hand
             u("C:\\utau\\voice"), // a Windows path
             u("?"),
             u("trailing\\"),
             u("\\u"),  // too short to be an escape
             u("\\uZZZZ"), // not hexadecimal
         }) {
        checkRoundTrip(text, shiftJis());
        checkRoundTrip(text, utf8());
    }
}

// UTAU writes two things and only two: UTF-8 with a Charset line, or the writer's own code page
// with nothing at all. So the list put to the user is short on purpose.
BOOST_AUTO_TEST_CASE(the_candidates_are_the_ones_utau_users_are_actually_on) {
    const auto candidates = TextCodec::ustCandidates();
    for (const auto &name : {"UTF-8", "Shift_JIS", "GBK", "Big5"}) {
        BOOST_TEST_CONTEXT(name) {
            BOOST_CHECK(candidates.contains(QLatin1String(name)));
            BOOST_CHECK(TextCodec(QLatin1String(name)).isValid());
        }
    }
    BOOST_CHECK(candidates.contains(TextCodec::systemName()));
    BOOST_CHECK_LE(candidates.size(), 5);
}

// The system encoding has to answer with a name that can be written into a control note. Qt
// calls its own "Locale", which tells a later reader nothing.
BOOST_AUTO_TEST_CASE(the_system_encoding_has_a_real_name) {
    const QString name = TextCodec::systemName();
    BOOST_CHECK(!name.isEmpty());
    BOOST_CHECK(name != QLatin1String("Locale"));
    BOOST_CHECK(TextCodec(name).isValid());
    BOOST_CHECK_EQUAL(TextCodec().name().toStdString(), TextCodec(name).name().toStdString());
}

// Something that is not one of the two forms came from somewhere, and guessing at it loses it.
BOOST_AUTO_TEST_CASE(an_escape_that_is_not_one_is_left_alone) {
    BOOST_CHECK(TextCodec::unescape(QStringLiteral("\\q")) == QStringLiteral("\\q"));
    BOOST_CHECK(TextCodec::unescape(QStringLiteral("\\u12")) == QStringLiteral("\\u12"));
    BOOST_CHECK(TextCodec::unescape(QStringLiteral("\\")) == QStringLiteral("\\"));
}

BOOST_AUTO_TEST_SUITE_END()


