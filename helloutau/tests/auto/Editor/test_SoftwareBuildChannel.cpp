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
    // or otherwise the version followed by the channel and the first seven digits of the commit
    // hash, as in the names of the assets. The hash is left out if the build records none.
    void the_build_name_follows_the_channel() {
        const QString channel = QStringLiteral(HELLOKIT_BUILD_CHANNEL);
        QCOMPARE(SoftwareBuildChannel::isRelease(), channel == QLatin1String("release"));
        const auto name = SoftwareBuildChannel::buildName();
        if (SoftwareBuildChannel::isRelease()) {
            QCOMPARE(name, QStringLiteral(HELLOKIT_VERSION));
            return;
        }
        const auto match =
            QRegularExpression(QLatin1Char('^') +
                               QRegularExpression::escape(QStringLiteral(HELLOKIT_VERSION)) +
                               QStringLiteral("-(ci|dev)(?:\\.([0-9a-f]{7}))?$"))
                .match(name);
        QVERIFY2(match.hasMatch(), qPrintable(name));
        QCOMPARE(match.captured(1), channel);
        const QString commit = QStringLiteral(HELLOKIT_GIT_LAST_COMMIT_HASH);
        if (QRegularExpression(QStringLiteral("^[0-9a-f]{7,}$")).match(commit).hasMatch()) {
            QCOMPARE(match.captured(2), commit.left(7));
        } else {
            QVERIFY2(match.captured(2).isEmpty(), qPrintable(name));
        }
    }

    void the_application_title_ends_with_the_build_name() {
        QCOMPARE(SoftwareBuildChannel::applicationTitle(),
                 QStringLiteral(HELLOUTAU_APPLICATION_NAME) + QLatin1Char(' ') +
                     SoftwareBuildChannel::buildName());
    }
};

QTEST_APPLESS_MAIN(test_SoftwareBuildChannel)

#include "test_SoftwareBuildChannel.moc"
