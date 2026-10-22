#include <QtTest/QTest>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpinBox>

#include <hellokit/Document/DocumentConstants.h>

#include <helloutau/Editor/Dialogs/TransposeDialog.h>

using namespace hello;
using namespace hello::daw;

class test_TransposeDialog : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The box starts at 0 and accepts only the semitones that keep the notes within the keyboard
    // range, downwards as negative numbers.
    void the_interval_keeps_the_notes_within_the_keyboard() {
        TransposeDialog dialog(60, 72);
        const auto box = dialog.semitoneBox();
        QCOMPARE(dialog.semitones(), 0);
        QCOMPARE(box->minimum(), kit::lowestNoteNum - 60);
        QCOMPARE(box->maximum(), kit::highestNoteNum - 72);
        QVERIFY(box->minimum() < 0);
        box->setValue(1000);
        QCOMPARE(dialog.semitones(), kit::highestNoteNum - 72);
        box->setValue(-1000);
        QCOMPARE(dialog.semitones(), kit::lowestNoteNum - 60);
    }

    // The summary follows the interval.
    void the_summary_follows_the_interval() {
        TransposeDialog dialog(60, 60);
        const auto before = dialog.summaryLabel()->text();
        QVERIFY(!before.isEmpty());
        dialog.semitoneBox()->setValue(-3);
        QVERIFY(dialog.summaryLabel()->text() != before);
    }

    // Notes at both ends of the keyboard cannot be transposed at all.
    void notes_at_both_ends_cannot_move() {
        TransposeDialog dialog(kit::lowestNoteNum, kit::highestNoteNum);
        QCOMPARE(dialog.semitoneBox()->minimum(), 0);
        QCOMPARE(dialog.semitoneBox()->maximum(), 0);
    }
};

QTEST_MAIN(test_TransposeDialog)

#include "test_TransposeDialog.moc"
