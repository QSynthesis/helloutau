#include <QtTest/QTest>

#include <hellokit/Support/BinaryStream.h>

using namespace hello::kit;

class test_BinaryStream : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Each number takes the bytes of its type in the byte order of the stream.
    void numbers_read_in_either_byte_order() {
        const QByteArray bytes("\x01\x02\x03\x04", 4);
        BinaryStream little(bytes);
        QCOMPARE(little.read<quint16>(), std::optional<quint16>(0x0201));
        QCOMPARE(little.pos(), 2);
        QCOMPARE(little.bytesAvailable(), 2);
        BinaryStream big(bytes, 0, BinaryStream::BigEndian);
        QCOMPARE(big.read<quint32>(), std::optional<quint32>(0x01020304));

        // A float takes 4 bytes and a double 8, whatever the other values of the stream.
        QByteArray numbers;
        BinaryStream writer(&numbers);
        writer.write(1.5f);
        writer.write(2.25);
        QCOMPARE(numbers.size(), 12);
        BinaryStream reader(numbers);
        QCOMPARE(reader.read<float>(), std::optional<float>(1.5f));
        QCOMPARE(reader.read<double>(), std::optional<double>(2.25));
    }

    // A number or bytes that the bytes do not hold read as nothing, and the position stays.
    void a_read_past_the_end_reads_nothing() {
        const QByteArray bytes("\x01\x02\x03", 3);
        BinaryStream stream(bytes, 1);
        QVERIFY(!stream.read<quint32>());
        QCOMPARE(stream.pos(), 1);
        QVERIFY(!stream.readBytes(3));
        QCOMPARE(stream.readBytes(2), std::optional<QByteArray>("\x02\x03"));
        QVERIFY(!stream.read<quint8>());
        QVERIFY(!stream.skip(1));
        QVERIFY(!stream.seek(4));
        QVERIFY(stream.seek(3));
        QVERIFY(!stream.isWritable());
    }

    // A write overwrites the bytes at the position and appends past the end.
    void a_write_overwrites_and_appends() {
        QByteArray bytes("abcdef");
        BinaryStream stream(&bytes, 2, BinaryStream::BigEndian);
        QVERIFY(stream.isWritable());
        stream.write(quint16(0x5859));
        QCOMPARE(bytes, QByteArray("abXYef"));
        QVERIFY(stream.seek(5));
        stream.writeBytes("ZZZ");
        QCOMPARE(bytes, QByteArray("abXYeZZZ"));
        QCOMPARE(stream.pos(), 8);
    }
};

QTEST_GUILESS_MAIN(test_BinaryStream)

#include "test_BinaryStream.moc"
