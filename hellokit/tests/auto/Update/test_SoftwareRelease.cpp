#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtTest/QTest>

#include <hellokit/Update/SoftwareRelease.h>

using namespace hello::kit;

namespace {

    // The fields of a release that the interface of GitHub returns, with some omitted
    QJsonObject gitHubRelease() {
        return QJsonObject{
            {"html_url",   "https://github.com/owner/app/releases/tag/v0.1.6.0"},
            {"tag_name",   "v0.1.6.0"                                          },
            {"name",       "0.1.6.0"                                           },
            {"draft",      false                                               },
            {"prerelease", false                                               },
            {"body",       "Notes"                                             },
            {"assets",
             QJsonArray{
                 QJsonObject{{"name", "app-0.1.6.0-x64-windows-msvc.zip"},
                             {"size", 100},
                             {"browser_download_url",
                              "https://github.com/owner/app/releases/download/v0.1.6.0/"
                              "app-0.1.6.0-x64-windows-msvc.zip"}},
                 QJsonObject{{"name", "insecure.zip"},
                             {"browser_download_url", "http://example.com/insecure.zip"}},
             }                                                                 },
        };
    }

    // The fields of a release that the interface of Gitee returns, with some omitted
    QJsonObject giteeRelease() {
        return QJsonObject{
            {"id",         1         },
            {"tag_name",   "v0.1.6.0"},
            {"prerelease", false     },
            {"name",       "0.1.6.0" },
            {"body",       "Notes"   },
            {"assets",
             QJsonArray{
                 QJsonObject{{"browser_download_url",
                              "https://gitee.com/owner/app/releases/download/v0.1.6.0/SHA256SUMS"},
                             {"name", "SHA256SUMS"}},
                 QJsonObject{{"browser_download_url",
                              "https://gitee.com/owner/app/archive/refs/tags/v0.1.6.0.zip"},
                             {"name", "v0.1.6.0.zip"}},
             }                       },
        };
    }

}

class test_SoftwareRelease : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void a_version_tag_is_v_followed_by_segments_data() {
        QTest::addColumn<QString>("tag");
        QTest::addColumn<QString>("version");
        QTest::newRow("four segments") << "v0.1.6.0" << "0.1.6.0";
        QTest::newRow("one segment") << "v2" << "2";
        QTest::newRow("no prefix") << "0.1.6.0" << QString();
        QTest::newRow("capital prefix") << "V0.1.6.0" << QString();
        QTest::newRow("prefix only") << "v" << QString();
        QTest::newRow("suffix") << "v0.1.6-beta" << QString();
        QTest::newRow("empty segment") << "v0..1" << QString();
        QTest::newRow("trailing dot") << "v0.1." << QString();
        QTest::newRow("space") << "v 0.1" << QString();
        QTest::newRow("plus sign") << "v+0.1" << QString();
        QTest::newRow("minus sign in a segment") << "v0.-1" << QString();
    }

    void a_version_tag_is_v_followed_by_segments() {
        QFETCH(QString, tag);
        QFETCH(QString, version);
        const auto result = SoftwareRelease::versionOfTag(tag);
        QCOMPARE(result.has_value(), !version.isEmpty());
        if (result) {
            QCOMPARE(result->toString(), version);
        }
    }

    void a_github_release_has_its_page_and_https_assets() {
        const auto release =
            SoftwareRelease::fromJson(gitHubRelease(), SoftwareRelease::GitHub, "owner/app");
        QVERIFY(release);
        QCOMPARE(release->tag, QStringLiteral("v0.1.6.0"));
        QCOMPARE(release->version, QVersionNumber({0, 1, 6, 0}));
        QCOMPARE(release->title, QStringLiteral("0.1.6.0"));
        QCOMPARE(release->notes, QStringLiteral("Notes"));
        QCOMPARE(release->page,
                 QUrl(QStringLiteral("https://github.com/owner/app/releases/tag/v0.1.6.0")));
        QCOMPARE(release->assets.size(), 1);
        QVERIFY(release->asset(QStringLiteral("app-0.1.6.0-x64-windows-msvc.zip")));
        QVERIFY(!release->asset(QStringLiteral("insecure.zip")));
    }

    void a_gitee_release_has_a_page_built_from_the_tag() {
        const auto release =
            SoftwareRelease::fromJson(giteeRelease(), SoftwareRelease::Gitee, "owner/app");
        QVERIFY(release);
        QCOMPARE(release->page,
                 QUrl(QStringLiteral("https://gitee.com/owner/app/releases/tag/v0.1.6.0")));
        const auto sums = release->asset(QStringLiteral("SHA256SUMS"));
        QVERIFY(sums);
        QCOMPARE(sums->url.host(), QStringLiteral("gitee.com"));
    }

    void drafts_prereleases_and_other_tags_are_not_releases_data() {
        QTest::addColumn<QString>("field");
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("draft") << "draft" << QJsonValue(true);
        QTest::newRow("prerelease") << "prerelease" << QJsonValue(true);
        QTest::newRow("tag") << "tag_name" << QJsonValue(QStringLiteral("nightly"));
    }

    void drafts_prereleases_and_other_tags_are_not_releases() {
        QFETCH(QString, field);
        QFETCH(QJsonValue, value);
        auto object = gitHubRelease();
        QVERIFY(SoftwareRelease::fromJson(object, SoftwareRelease::GitHub, "owner/app"));
        object.insert(field, value);
        QVERIFY(!SoftwareRelease::fromJson(object, SoftwareRelease::GitHub, "owner/app"));
    }
};

QTEST_APPLESS_MAIN(test_SoftwareRelease)

#include "test_SoftwareRelease.moc"
