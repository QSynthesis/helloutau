#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>

#include <hellokit/Synth/SynthToolProcess.h>

#include <helloutau/Editor/Dialogs/OutputLogDialog.h>

using namespace hello;
using namespace hello::daw;

class test_OutputLogDialog : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The dialog shows the log under its title, and Clear empties the log and the view.
    void the_log_is_shown_and_cleared() {
        const auto log = std::make_shared<kit::SynthToolOutputLog>();
        log->record("resampler.exe", QStringLiteral("analyzed a.wav"));
        OutputLogDialog dialog(log, QStringLiteral("Frequency Map Log"));
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Frequency Map Log"));
        QVERIFY(dialog.textView()->toPlainText().contains(QStringLiteral("analyzed a.wav")));
        QVERIFY(dialog.textView()->isReadOnly());

        const auto buttons = dialog.findChild<QDialogButtonBox *>();
        QVERIFY(buttons);
        QPushButton *clear = nullptr;
        for (const auto button : buttons->buttons()) {
            if (buttons->buttonRole(button) == QDialogButtonBox::DestructiveRole) {
                clear = qobject_cast<QPushButton *>(button);
            }
        }
        QVERIFY(clear);
        clear->click();
        QVERIFY(log->text().isEmpty());
        QVERIFY(dialog.textView()->toPlainText().isEmpty());
    }
};

QTEST_MAIN(test_OutputLogDialog)

#include "test_OutputLogDialog.moc"
