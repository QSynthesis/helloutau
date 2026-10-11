#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
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

    // Returns a configuration with two sources, A for any territory and B for mainland China
    // with a page template.
    QJsonObject validConfiguration() {
        return QJsonObject{
            {"assetPrefix",    "app"    },
            {"innoSetupAppId", "org.app"},
            {"sources",
             QJsonArray{
                 QJsonObject{{"name", "A"}, {"latestRelease", "https://a.example/latest"}},
                 QJsonObject{{"name", "B"},
                             {"latestRelease", "https://b.example/latest"},
                             {"releasePage", "https://b.example/tag/${tag}"},
                             {"territories", QJsonArray{"CN"}}},
             }                          },
        };
    }

    bool writeConfiguration(const QString &fileName, const QJsonObject &object) {
        QFile file(fileName);
        const auto content = QJsonDocument(object).toJson();
        return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
    }

}

class test_UpdateChecker : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // A named source is chosen by its name. Otherwise the first source for the territory is
    // chosen, or else the first source.
    void the_automatic_selection_follows_the_territories() {
        QTemporaryDir dir;
        const auto fileName = dir.filePath(QStringLiteral("UpdateConfig.json"));
        QVERIFY(writeConfiguration(fileName, validConfiguration()));
        QString error;
        const auto configuration = UpdateChecker::Configuration::read(fileName, &error);
        QVERIFY2(configuration, qPrintable(error));
        QCOMPARE(configuration->sourceOf(QString(), QLocale::China).name, QStringLiteral("B"));
        QCOMPARE(configuration->sourceOf(QString(), QLocale::Japan).name, QStringLiteral("A"));
        QCOMPARE(configuration->sourceOf(QStringLiteral("A"), QLocale::China).name,
                 QStringLiteral("A"));
        QCOMPARE(configuration->sourceOf(QStringLiteral("Missing"), QLocale::China).name,
                 QStringLiteral("B"));
        QVERIFY(configuration->hasSource(QStringLiteral("B")));
        QVERIFY(!configuration->hasSource(QStringLiteral("Missing")));
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

    void the_configuration_is_read() {
        QTemporaryDir dir;
        const auto fileName = dir.filePath(QStringLiteral("UpdateConfig.json"));
        QVERIFY(writeConfiguration(fileName, validConfiguration()));
        QString error;
        const auto configuration = UpdateChecker::Configuration::read(fileName, &error);
        QVERIFY2(configuration, qPrintable(error));
        QCOMPARE(configuration->assetPrefix, QStringLiteral("app"));
        QCOMPARE(configuration->innoSetupAppId, QStringLiteral("org.app"));
        QCOMPARE(configuration->sources.size(), 2);
        const auto &b = configuration->sources.at(1);
        QCOMPARE(b.latestRelease, QUrl(QStringLiteral("https://b.example/latest")));
        QCOMPARE(b.releasePage, QStringLiteral("https://b.example/tag/${tag}"));
        QCOMPARE(b.territories, QList<QLocale::Territory>{QLocale::China});
    }

    void an_invalid_configuration_is_rejected_data() {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QJsonValue>("value");
        const auto source = [](const QJsonObject &changes) {
            QJsonObject object{
                {"name",          "C"                       },
                {"latestRelease", "https://c.example/latest"}
            };
            for (auto it = changes.begin(); it != changes.end(); ++it) {
                object.insert(it.key(), it.value());
            }
            return QJsonArray{object};
        };
        QTest::newRow("no prefix") << "assetPrefix" << QJsonValue();
        QTest::newRow("no identifier") << "innoSetupAppId" << QJsonValue();
        QTest::newRow("no source") << "sources" << QJsonValue(QJsonArray());
        QTest::newRow("no name") << "sources"
                                 << QJsonValue(source({
                                        {"name", ""}
        }));
        QTest::newRow("same name")
            << "sources" << QJsonValue(QJsonArray{source({}).first(), source({}).first()});
        QTest::newRow("not https") << "sources"
                                   << QJsonValue(source({
                                          {"latestRelease", "http://c.example/latest"}
        }));
        QTest::newRow("unknown territory") << "sources"
                                           << QJsonValue(source({
                                                  {"territories", QJsonArray{"Mainland"}}
        }));
    }

    void an_invalid_configuration_is_rejected() {
        QFETCH(QString, key);
        QFETCH(QJsonValue, value);
        QTemporaryDir dir;
        const auto fileName = dir.filePath(QStringLiteral("UpdateConfig.json"));
        QString error;
        QVERIFY(!UpdateChecker::Configuration::read(fileName, &error));
        QVERIFY(!error.isEmpty());

        auto object = validConfiguration();
        if (value.isUndefined()) {
            object.remove(key);
        } else {
            object.insert(key, value);
        }
        QVERIFY(writeConfiguration(fileName, object));
        error.clear();
        QVERIFY(!UpdateChecker::Configuration::read(fileName, &error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_MAIN(test_UpdateChecker)

#include "test_UpdateChecker.moc"
