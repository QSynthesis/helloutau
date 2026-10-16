#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPlainTextEdit>

#include <helloutau/Editor/Dialogs/NoteEntriesDialog.h>

using namespace hello;
using namespace hello::daw;

class test_NoteEntriesDialog : public QObject {
    Q_OBJECT

private:
    static kit::Note labeled(const QString &label) {
        kit::Note note;
        note.lyric = QStringLiteral("a");
        note.length = 480;
        note.noteNum = 60;
        note.label = label;
        note.userData.insert(QStringLiteral("$plugin"), QStringLiteral("1"));
        return note;
    }

private Q_SLOTS:
    // With the same entries on every note, the entries are shown and edited, and the dialog
    // gives the edited note. Unchanged entries give nothing.
    void the_entries_of_the_same_notes_are_edited() {
        const auto note = labeled(QStringLiteral("verse"));
        NoteEntriesDialog dialog({note, note});
        QVERIFY(dialog.isEditable());
        QVERIFY(!dialog.builtInEdit()->isReadOnly());
        QCOMPARE(dialog.builtInEdit()->toPlainText(), QStringLiteral("Label=verse"));
        QCOMPARE(dialog.userDataEdit()->toPlainText(), QStringLiteral("$plugin=1"));

        dialog.accept();
        QVERIFY(!dialog.others());

        dialog.builtInEdit()->setPlainText(QStringLiteral("Label=chorus\n$direct=True"));
        dialog.userDataEdit()->setPlainText(QStringLiteral("$plugin=2\n$other=x"));
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        const auto others = dialog.others();
        QVERIFY(others);
        QCOMPARE(others->label, QStringLiteral("chorus"));
        QCOMPARE(others->direct, QStringLiteral("True"));
        QCOMPARE(others->userData.size(), 2);
        QCOMPARE(others->userData.value(QStringLiteral("$plugin")), QStringLiteral("2"));
    }

    // With entries that differ between the notes, the entries are read only and give nothing.
    void the_entries_of_different_notes_are_read_only() {
        NoteEntriesDialog dialog({labeled(QStringLiteral("a")), labeled(QStringLiteral("b"))});
        QVERIFY(!dialog.isEditable());
        QVERIFY(dialog.builtInEdit()->isReadOnly() && dialog.userDataEdit()->isReadOnly());
        QCOMPARE(dialog.builtInEdit()->toPlainText(), QStringLiteral("Label=a"));
        dialog.accept();
        QVERIFY(!dialog.others());
    }

    // A refused line of either box is an error that names the box, and gives no note.
    void a_refused_line_gives_no_note() {
        NoteEntriesDialog dialog({labeled(QString())});
        dialog.builtInEdit()->setPlainText(QStringLiteral("Lyric=b"));
        dialog.userDataEdit()->setPlainText(QStringLiteral("Envelope=1"));
        kit::DiagnosticList diagnostics;
        QVERIFY(!dialog.editedNote(diagnostics));
        QCOMPARE(diagnostics.size(), 2);
        QVERIFY(diagnostics[0].message != diagnostics[1].message);
    }

    // The warnings follow the user data: none for an entry that UTAU keeps, one for an entry
    // without $.
    void the_user_data_have_their_warnings() {
        NoteEntriesDialog dialog({labeled(QString())});
        QVERIFY(dialog.warningLabel()->text().isEmpty());
        dialog.userDataEdit()->setPlainText(QStringLiteral("$plugin=1\nplain=1"));
        QVERIFY(!dialog.warningLabel()->text().isEmpty());
        // In the color of the warnings, which a theme sets
        QVERIFY(dialog.warningLabel()->text().contains(dialog.warningColor().name()));
        dialog.setWarningColor(QColor(1, 2, 3));
        QVERIFY(dialog.warningLabel()->text().contains(QColor(1, 2, 3).name()));
        dialog.userDataEdit()->setPlainText(QStringLiteral("$plugin=1"));
        QVERIFY(dialog.warningLabel()->text().isEmpty());
    }
};

QTEST_MAIN(test_NoteEntriesDialog)

#include "test_NoteEntriesDialog.moc"
