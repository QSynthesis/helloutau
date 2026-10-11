#include <QtCore/QDir>
#include <QtCore/QUuid>
#include <QtTest/QTest>

#include <hellokit/Update/Windows/InnoSetup.h>

using namespace hello::kit;

class test_InnoSetup : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // A random AppId has no uninstall key in either root.
    void an_application_without_an_uninstall_key_is_not_installed() {
        const auto appId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QVERIFY(!InnoSetup::installLocation(appId));
        QVERIFY(!InnoSetup::isInstalledIn(appId, QDir::tempPath().toStdU16String()));
    }
};

QTEST_APPLESS_MAIN(test_InnoSetup)

#include "test_InnoSetup.moc"
