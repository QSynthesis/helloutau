#include <QtCore/QRegularExpression>
#include <QtTest/QTest>

#include <hellokit/BuildInfo.h>
#include <hellokit/Config.h>

#include <helloutau/Config.h>
#include <helloutau/Editor/SoftwareBuildChannel.h>

using namespace hello::daw;

class test_SoftwareBuildChannel : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The name of the build follows the channel of the configuration: the version of a release,
    // or otherwise the channel and the first seven digits of the commit.
    void the_build_name_follows_the_channel() {
        const QString channel = QStringLiteral(HELLOKIT_BUILD_CHANNEL);
        QCOMPARE(SoftwareBuildChannel::isRelease(), channel == QLatin1String("release"));
        const auto name = SoftwareBuildChannel::buildName();
        if (SoftwareBuildChannel::isRelease()) {
            QCOMPARE(name, QStringLiteral(HELLOKIT_VERSION));
            return;
        }
        const auto match =
            QRegularExpression(QStringLiteral("^(CI|dev) ([0-9a-f]{7})$")).match(name);
        QVERIFY2(match.hasMatch(), qPrintable(name));
        QCOMPARE(match.captured(1),
                 channel == QLatin1String("ci") ? QStringLiteral("CI") : channel);
        QVERIFY(QStringLiteral(HELLOKIT_GIT_LAST_COMMIT_HASH).startsWith(match.captured(2)));
    }

    void the_application_title_ends_with_the_build_name() {
        QCOMPARE(SoftwareBuildChannel::applicationTitle(),
                 QStringLiteral(HELLOUTAU_APPLICATION_NAME) + QLatin1Char(' ') +
                     SoftwareBuildChannel::buildName());
    }
};

QTEST_APPLESS_MAIN(test_SoftwareBuildChannel)

#include "test_SoftwareBuildChannel.moc"
