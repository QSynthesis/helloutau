#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>

#include <helloutau/Editor/Dialogs/AboutDialog.h>

using namespace hello::daw;

class test_AboutDialog : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The title names the application, and the text holds its version and that of Qt.
    void the_dialog_shows_the_name_and_the_versions() {
        QApplication::setApplicationDisplayName(QStringLiteral("HelloTest"));
        QApplication::setApplicationVersion(QStringLiteral("9.8.7"));
        AboutDialog dialog;
        QCOMPARE(dialog.windowTitle(), QStringLiteral("About HelloTest"));
        const auto label = dialog.findChild<QLabel *>();
        QVERIFY(label);
        QVERIFY(label->text().contains(QStringLiteral("HelloTest")));
        QVERIFY(label->text().contains(QStringLiteral("9.8.7")));
        QVERIFY(label->text().contains(QStringLiteral(QT_VERSION_STR)));
    }
};

QTEST_MAIN(test_AboutDialog)

#include "test_AboutDialog.moc"
