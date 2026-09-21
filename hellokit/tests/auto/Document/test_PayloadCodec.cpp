#define BOOST_TEST_MAIN

#include <string>

#include <QtCore/QByteArray>

#include <hellokit/Document/PayloadCodec.h>

#include <boost/test/unit_test.hpp>

using namespace hello::kit;

BOOST_AUTO_TEST_SUITE(test_PayloadCodec)

namespace {

    // Boost.Test cannot print a QByteArray, and a failure that says nothing about what came back
    // is half a failure.
    std::string encoded(const QByteArray &data) {
        return PayloadCodec::encode(data).toStdString();
    }

    std::string roundTrip(const QByteArray &data) {
        auto decoded = PayloadCodec::decode(PayloadCodec::encode(data));
        BOOST_REQUIRE(decoded.has_value());
        return decoded->toStdString();
    }

}

BOOST_AUTO_TEST_CASE(test_encode) {
    BOOST_CHECK_EQUAL(encoded(""), "");
    BOOST_CHECK_EQUAL(encoded("f"), "Zg");
    BOOST_CHECK_EQUAL(encoded("fo"), "Zm8");
    BOOST_CHECK_EQUAL(encoded("foo"), "Zm9v");
    BOOST_CHECK_EQUAL(encoded("foobar"), "Zm9vYmFy");
}

// The whole point of the alphabet. An equals sign would be truncated by UTAU, a plus and a slash
// are what base64url replaces to begin with.
BOOST_AUTO_TEST_CASE(test_output_holds_nothing_utau_would_touch) {
    QByteArray data;
    for (int i = 0; i < 256; ++i) {
        data += static_cast<char>(i);
    }

    auto text = PayloadCodec::encode(data);
    BOOST_REQUIRE(!text.isEmpty());
    for (char c : text) {
        bool allowed = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                       c == '-' || c == '_';
        BOOST_CHECK_MESSAGE(allowed, "unexpected character in payload: " << int(c));
    }
}

BOOST_AUTO_TEST_CASE(test_round_trip) {
    BOOST_CHECK_EQUAL(roundTrip(""), "");
    BOOST_CHECK_EQUAL(roundTrip("a"), "a");
    BOOST_CHECK_EQUAL(roundTrip("ab"), "ab");
    BOOST_CHECK_EQUAL(roundTrip("abc"), "abc");
    BOOST_CHECK_EQUAL(roundTrip("abcd"), "abcd");

    // Shift_JIS bytes, since what goes through here is raw bytes rather than text.
    BOOST_CHECK_EQUAL(roundTrip("\x82\xA0\x82\xA2"), "\x82\xA0\x82\xA2");

    // Every byte value, at every offset within a group.
    for (int offset = 0; offset < 3; ++offset) {
        QByteArray data(offset, 'x');
        for (int i = 0; i < 256; ++i) {
            data += static_cast<char>(i);
        }
        BOOST_CHECK(PayloadCodec::decode(PayloadCodec::encode(data)) == data);
    }
}

BOOST_AUTO_TEST_CASE(test_decode_refuses_what_cannot_be_read_back) {
    // Padding cannot survive the trip through UTAU, so accepting it here would take in what could
    // never be written out. Qt would decode all three of these without complaint.
    BOOST_CHECK(!PayloadCodec::decode("Zg=="));
    BOOST_CHECK(!PayloadCodec::decode("Zm8="));
    BOOST_CHECK(!PayloadCodec::decode("="));

    // Not in the alphabet. These two are base64's own characters, which base64url replaces.
    BOOST_CHECK(!PayloadCodec::decode("Zm+v"));
    BOOST_CHECK(!PayloadCodec::decode("Zm/v"));
    BOOST_CHECK(!PayloadCodec::decode("hello world"));

    // Six bits on their own did not come from a byte. Qt drops them silently.
    BOOST_CHECK(!PayloadCodec::decode("Z"));
    BOOST_CHECK(!PayloadCodec::decode("Zm9vZ"));
}

BOOST_AUTO_TEST_SUITE_END()
