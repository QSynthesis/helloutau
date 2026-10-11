#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Update/Sha256Sums.h>

using namespace hello::kit;

namespace {

    // The SHA-256 digest of "abc", from FIPS 180-2
    constexpr char abcDigest[] = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

}

class test_Sha256Sums : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void the_digest_of_a_listed_file_is_found() {
        const QByteArray sums =
            QByteArray(abcDigest).toUpper() + "  app.zip\r\n" +
            "0000000000000000000000000000000000000000000000000000000000000001 *app-setup.exe\n";
        QCOMPARE(Sha256Sums::digest(sums, QStringLiteral("app.zip")), QByteArray(abcDigest));
        QCOMPARE(Sha256Sums::digest(sums, QStringLiteral("app-setup.exe")),
                 QByteArray("0000000000000000000000000000000000000000000000000000000000000001"));
    }

    void a_name_matches_only_in_full() {
        const QByteArray sums = QByteArray(abcDigest) + "  xapp.zip\n";
        QVERIFY(!Sha256Sums::digest(sums, QStringLiteral("app.zip")));
        QVERIFY(!Sha256Sums::digest(sums, QStringLiteral("xapp")));
    }

    void a_malformed_line_is_skipped() {
        const QByteArray sums = "abc  app.zip\n" + QByteArray(abcDigest) + " app.zip\n" +
                                QByteArray(abcDigest).replace('a', 'g') + "  app.zip\n";
        QVERIFY(!Sha256Sums::digest(sums, QStringLiteral("app.zip")));
    }

    void the_digest_of_a_file_is_computed() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath(QStringLiteral("abc.txt"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("abc");
        file.close();
        QCOMPARE(Sha256Sums::fileDigest(path), QByteArray(abcDigest));
        QVERIFY(!Sha256Sums::fileDigest(directory.filePath(QStringLiteral("missing.txt"))));
    }
};

QTEST_APPLESS_MAIN(test_Sha256Sums)

#include "test_Sha256Sums.moc"
