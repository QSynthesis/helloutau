#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QTextEdit>

#include <helloutau/Editor/Dialogs/ReplaceLyricsDialog.h>

using namespace hello::daw;

namespace {

    QCheckBox *checkBoxOf(const ReplaceLyricsDialog &dialog, const QString &text) {
        for (const auto box : dialog.findChildren<QCheckBox *>()) {
            if (box->text() == text) {
                return box;
            }
        }
        return nullptr;
    }

}

class test_ReplaceLyricsDialog : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The lyrics are shown selected, so that typing replaces them, and returned as typed.
    void the_lyrics_are_shown_selected() {
        ReplaceLyricsDialog dialog;
        dialog.setLyrics(QStringLiteral("a i  u"));
        QCOMPARE(dialog.lyrics(), QStringLiteral("a i  u"));
        const auto edit = dialog.findChild<QTextEdit *>();
        QVERIFY(edit);
        QVERIFY(edit->textCursor().hasSelection());
        QCOMPARE(edit->textCursor().selectedText(), QStringLiteral("a i  u"));
    }

    void the_options_are_off_by_default() {
        ReplaceLyricsDialog dialog;
        QVERIFY(!dialog.repeat());
        QVERIFY(!dialog.splitCharacters());

        const auto repeat = checkBoxOf(dialog, QStringLiteral("Repeat to fill the selected notes"));
        const auto split = checkBoxOf(dialog, QStringLiteral("Split by character"));
        QVERIFY(repeat && split);
        repeat->setChecked(true);
        QVERIFY(dialog.repeat());
        QVERIFY(!dialog.splitCharacters());
        split->setChecked(true);
        QVERIFY(dialog.splitCharacters());
    }
};

QTEST_MAIN(test_ReplaceLyricsDialog)

#include "test_ReplaceLyricsDialog.moc"
