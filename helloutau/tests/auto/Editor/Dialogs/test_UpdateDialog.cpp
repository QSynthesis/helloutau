#include <QtTest/QTest>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextBrowser>

#include <helloutau/Editor/Dialogs/UpdateDialog.h>

using namespace hello::daw;

class test_UpdateDialog : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void the_notes_are_shown_as_markdown() {
        UpdateDialog dialog(QStringLiteral("0.1.7.0"), QStringLiteral("0.1.6.0"),
                            QStringLiteral("# Changes"));
        QCOMPARE(dialog.notesBrowser()->toPlainText(), QStringLiteral("Changes"));
        QCOMPARE(dialog.choice(), UpdateDialog::Later);
    }

    void each_button_records_its_choice_data() {
        QTest::addColumn<QString>("button");
        QTest::addColumn<int>("choice");
        QTest::addColumn<int>("result");
        QTest::newRow("download") << "download" << int(UpdateDialog::Download)
                                  << int(QDialog::Accepted);
        QTest::newRow("skip") << "skip" << int(UpdateDialog::Skip) << int(QDialog::Rejected);
        QTest::newRow("later") << "later" << int(UpdateDialog::Later) << int(QDialog::Rejected);
    }

    void each_button_records_its_choice() {
        QFETCH(QString, button);
        QFETCH(int, choice);
        QFETCH(int, result);
        UpdateDialog dialog(QStringLiteral("0.1.7.0"), QStringLiteral("0.1.6.0"), QString());
        const auto clicked = dialog.findChild<QPushButton *>(button);
        QVERIFY(clicked);
        clicked->click();
        QCOMPARE(int(dialog.choice()), choice);
        QCOMPARE(dialog.result(), result);
    }
};

QTEST_MAIN(test_UpdateDialog)

#include "test_UpdateDialog.moc"
