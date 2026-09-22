#include <QtCore/QByteArray>
#include <QtTest/QTest>

#include <hellokit/Document/PayloadCodec.h>

using namespace hello::kit;

class test_PayloadCodec : public QObject {
    Q_OBJECT

private:
    static QByteArray roundTrip(const QByteArray &data) {
        const auto decoded = PayloadCodec::decode(PayloadCodec::encode(data));
        return decoded.value_or(QByteArray("<refused>"));
    }

private Q_SLOTS:
    void test_encode() {
        QCOMPARE(PayloadCodec::encode(""), QByteArray(""));
        QCOMPARE(PayloadCodec::encode("f"), QByteArray("Zg"));
        QCOMPARE(PayloadCodec::encode("fo"), QByteArray("Zm8"));
        QCOMPARE(PayloadCodec::encode("foo"), QByteArray("Zm9v"));
        QCOMPARE(PayloadCodec::encode("foobar"), QByteArray("Zm9vYmFy"));
    }

    // The purpose of the alphabet. UTAU truncates at an equals sign, and base64url replaces the
    // plus sign and the slash of base64.
    void test_output_holds_nothing_utau_would_touch() {
        QByteArray data;
        for (int i = 0; i < 256; ++i) {
            data += static_cast<char>(i);
        }

        const auto text = PayloadCodec::encode(data);
        QVERIFY(!text.isEmpty());
        for (char c : text) {
            const bool allowed = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                                 (c >= '0' && c <= '9') || c == '-' || c == '_';
            QVERIFY2(allowed,
                     qPrintable(QStringLiteral("unexpected character in payload: %1").arg(int(c))));
        }
    }

    void test_round_trip() {
        QCOMPARE(roundTrip(""), QByteArray(""));
        QCOMPARE(roundTrip("a"), QByteArray("a"));
        QCOMPARE(roundTrip("ab"), QByteArray("ab"));
        QCOMPARE(roundTrip("abc"), QByteArray("abc"));
        QCOMPARE(roundTrip("abcd"), QByteArray("abcd"));

        // Shift_JIS bytes, because the codec operates on raw bytes rather than text.
        QCOMPARE(roundTrip("\x82\xA0\x82\xA2"), QByteArray("\x82\xA0\x82\xA2"));

        // Every byte value at every offset within a group.
        for (int offset = 0; offset < 3; ++offset) {
            QByteArray data(offset, 'x');
            for (int i = 0; i < 256; ++i) {
                data += static_cast<char>(i);
            }
            QCOMPARE(roundTrip(data), data);
        }
    }

    void test_decode_refuses_what_cannot_be_read_back() {
        // Padding does not survive UTAU, so accepting it would admit input that could never be
        // written back. Qt decodes all three of these without error.
        QVERIFY(!PayloadCodec::decode("Zg==").has_value());
        QVERIFY(!PayloadCodec::decode("Zm8=").has_value());
        QVERIFY(!PayloadCodec::decode("=").has_value());

        // Not in the alphabet. These two characters belong to base64 and are replaced in
        // base64url.
        QVERIFY(!PayloadCodec::decode("Zm+v").has_value());
        QVERIFY(!PayloadCodec::decode("Zm/v").has_value());
        QVERIFY(!PayloadCodec::decode("hello world").has_value());

        // Six leftover bits cannot originate from a byte. Qt discards them silently.
        QVERIFY(!PayloadCodec::decode("Z").has_value());
        QVERIFY(!PayloadCodec::decode("Zm9vZ").has_value());
    }
};

QTEST_APPLESS_MAIN(test_PayloadCodec)

#include "test_PayloadCodec.moc"
