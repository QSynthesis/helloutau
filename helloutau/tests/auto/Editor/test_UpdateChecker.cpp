#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <helloutau/Editor/UpdateChecker.h>

using namespace hello::daw;
using hello::kit::SoftwareRelease;

namespace {

    SoftwareRelease releaseOf(const QVersionNumber &version, const QStringList &assets) {
        SoftwareRelease release;
        release.version = version;
        for (const auto &name : assets) {
            release.assets.push_back({name, QUrl(QStringLiteral("https://example.com/") + name)});
        }
        return release;
    }

    bool writeFile(const QString &fileName, const QByteArray &content) {
        QFile file(fileName);
        return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
    }

}

class test_UpdateChecker : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void the_automatic_source_follows_the_time_zone() {
        QCOMPARE(UpdateChecker::hostOf(AppSettings::AutomaticUpdateSource, QLocale::China),
                 SoftwareRelease::Gitee);
        QCOMPARE(UpdateChecker::hostOf(AppSettings::AutomaticUpdateSource, QLocale::Taiwan),
                 SoftwareRelease::GitHub);
        QCOMPARE(UpdateChecker::hostOf(AppSettings::AutomaticUpdateSource, QLocale::Japan),
                 SoftwareRelease::GitHub);
        QCOMPARE(UpdateChecker::hostOf(AppSettings::GitHubUpdateSource, QLocale::China),
                 SoftwareRelease::GitHub);
        QCOMPARE(UpdateChecker::hostOf(AppSettings::GiteeUpdateSource, QLocale::Japan),
                 SoftwareRelease::Gitee);
    }

    void asset_names_follow_the_release_assets() {
        const QVersionNumber version({0, 1, 6, 0});
        QCOMPARE(UpdateChecker::assetName("app", version, "x64-windows-msvc", false),
                 QStringLiteral("app-0.1.6.0-x64-windows-msvc.zip"));
        QCOMPARE(UpdateChecker::assetName("app", version, "x64-windows-msvc", true),
                 QStringLiteral("app-0.1.6.0-x64-windows-msvc-setup.exe"));
    }

    // A release is offered if it is newer and has both its asset and the checksums.
    void only_a_newer_and_complete_release_is_offered() {
        const QVersionNumber current({0, 1, 6, 0});
        const auto asset = QStringLiteral("app.zip");
        QVERIFY(
            UpdateChecker::offers(releaseOf({0, 1, 7, 0}, {asset, "SHA256SUMS"}), current, asset));
        QVERIFY(
            !UpdateChecker::offers(releaseOf({0, 1, 6}, {asset, "SHA256SUMS"}), current, asset));
        QVERIFY(
            !UpdateChecker::offers(releaseOf({0, 1, 5, 9}, {asset, "SHA256SUMS"}), current, asset));
        QVERIFY(!UpdateChecker::offers(releaseOf({0, 1, 7, 0}, {asset}), current, asset));
        QVERIFY(!UpdateChecker::offers(releaseOf({0, 1, 7, 0}, {"SHA256SUMS"}), current, asset));
    }

    void the_configuration_requires_every_value() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fileName = dir.filePath(QStringLiteral("UpdateConfig.json"));
        QString error;
        QVERIFY(!UpdateChecker::Configuration::read(fileName, &error));
        QVERIFY(!error.isEmpty());

        QVERIFY(writeFile(fileName, "{\"gitHubRepository\": \"a/b\", "
                                    "\"giteeRepository\": \"c/d\", \"assetPrefix\": \"app\"}"));
        error.clear();
        QVERIFY(!UpdateChecker::Configuration::read(fileName, &error));
        QVERIFY(!error.isEmpty());

        QVERIFY(writeFile(fileName, "{\"gitHubRepository\": \"a/b\", "
                                    "\"giteeRepository\": \"c/d\", \"assetPrefix\": \"app\", "
                                    "\"innoSetupAppId\": \"org.app\"}"));
        const auto configuration = UpdateChecker::Configuration::read(fileName, &error);
        QVERIFY(configuration);
        QCOMPARE(configuration->gitHubRepository, QStringLiteral("a/b"));
        QCOMPARE(configuration->giteeRepository, QStringLiteral("c/d"));
        QCOMPARE(configuration->assetPrefix, QStringLiteral("app"));
        QCOMPARE(configuration->innoSetupAppId, QStringLiteral("org.app"));
    }
};

QTEST_MAIN(test_UpdateChecker)

#include "test_UpdateChecker.moc"
