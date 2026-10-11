#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <hellokit/Update/SoftwareReleaseDownload.h>

using namespace hello::kit;

namespace {

    // Returns a release with SHA256SUMS and two assets at the given URLs, one of which has a
    // name with a path.
    SoftwareRelease releaseOf(const QString &assetUrl, const QString &sumsUrl) {
        SoftwareRelease release;
        release.tag = QStringLiteral("v1.0");
        release.assets.push_back({QStringLiteral("app.zip"), QUrl(assetUrl)});
        release.assets.push_back({QStringLiteral("../app.zip"), QUrl(assetUrl)});
        release.assets.push_back({QStringLiteral("SHA256SUMS"), QUrl(sumsUrl)});
        return release;
    }

}

class test_SoftwareReleaseDownload : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Each of these releases is rejected before any request is sent.
    void an_unusable_release_is_rejected_data() {
        QTest::addColumn<QString>("assetName");
        QTest::addColumn<QString>("assetUrl");
        QTest::addColumn<QString>("sumsUrl");
        const QString https = QStringLiteral("https://example.com/file");
        QTest::newRow("missing asset") << "other.zip" << https << https;
        QTest::newRow("path in the name") << "../app.zip" << https << https;
        QTest::newRow("asset over HTTP") << "app.zip" << "http://example.com/app.zip" << https;
        QTest::newRow("sums over HTTP") << "app.zip" << https << "http://example.com/SHA256SUMS";
    }

    void an_unusable_release_is_rejected() {
        QFETCH(QString, assetName);
        QFETCH(QString, assetUrl);
        QFETCH(QString, sumsUrl);
        QTemporaryDir dir;
        SoftwareReleaseDownload download;
        QSignalSpy finished(&download, &SoftwareReleaseDownload::finished);
        QVERIFY(!download.start(releaseOf(assetUrl, sumsUrl), assetName, dir.path()));
        QVERIFY(!download.isRunning());
        QVERIFY(!download.errorString().isEmpty());
        QVERIFY(download.filePath().isEmpty());
        QCOMPARE(finished.count(), 0);
    }

    void a_release_without_sums_is_rejected() {
        SoftwareRelease release;
        release.tag = QStringLiteral("v1.0");
        release.assets.push_back(
            {QStringLiteral("app.zip"), QUrl(QStringLiteral("https://example.com/app.zip"))});
        QTemporaryDir dir;
        SoftwareReleaseDownload download;
        QVERIFY(!download.start(release, QStringLiteral("app.zip"), dir.path()));
        QVERIFY(!download.errorString().isEmpty());
    }
};

QTEST_GUILESS_MAIN(test_SoftwareReleaseDownload)

#include "test_SoftwareReleaseDownload.moc"
