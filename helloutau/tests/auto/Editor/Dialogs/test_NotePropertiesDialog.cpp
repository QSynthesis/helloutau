#include <optional>

#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QLineEdit>

#include <stdutau/utaconst.h>

#include <helloutau/Editor/Dialogs/NotePropertiesDialog.h>

using namespace hello;
using namespace hello::daw;

class test_NotePropertiesDialog : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The dialog shows what the notes share, "(various)" where they differ, and gives only the
    // fields edited: an emptied number back to the default, one that does not read left out.
    void the_note_properties_dialog_gives_what_was_edited() {
        kit::Note a;
        a.lyric = QStringLiteral("a");
        a.length = 480;
        a.intensity = 80;
        a.flags = QStringLiteral("g-2");
        auto b = a;
        b.lyric = QStringLiteral("ka");
        b.tempo = 150;
        using F = NotePropertiesDialog;
        const QList<NotePropertiesDialog::Defaults> defaults = {
            {120, 12, 3},
            {150, 12, 3},
        };
        NotePropertiesDialog dialog({a, b}, defaults);
        QVERIFY(dialog.changes().isEmpty());
        QCOMPARE(dialog.field(F::Lyric)->text(), QString());
        QCOMPARE(dialog.field(F::Lyric)->placeholderText(), QStringLiteral("(various)"));
        QCOMPARE(dialog.field(F::Length)->text(), QStringLiteral("480"));
        QCOMPARE(dialog.field(F::Intensity)->text(), QStringLiteral("80"));
        QCOMPARE(dialog.field(F::Tempo)->placeholderText(), QStringLiteral("(various)"));
        QCOMPARE(dialog.field(F::Modulation)->placeholderText(), QStringLiteral("(default: 100)"));
        QCOMPARE(dialog.field(F::PreUtterance)->placeholderText(), QStringLiteral("(default: 12)"));
        QCOMPARE(dialog.field(F::Flags)->text(), QStringLiteral("g-2"));

        QTest::keyClicks(dialog.field(F::Tempo), QStringLiteral("140"));
        dialog.field(F::Intensity)->clear();
        Q_EMIT dialog.field(F::Intensity)->textEdited(QString());
        QTest::keyClicks(dialog.field(F::Modulation), QStringLiteral("-"));
        auto changes = dialog.changes();
        using Change = std::optional<std::optional<double>>;
        QCOMPARE(changes.tempo, Change(std::optional(140.0)));
        QCOMPARE(changes.intensity, Change(std::optional<double>()));
        QVERIFY(!changes.modulation);
        QVERIFY(!changes.lyric && !changes.length && !changes.flags && !changes.velocity);

        TempoDialog tempo(std::nullopt, 120);
        QVERIFY(tempo.followBox()->isChecked());
        QCOMPARE(tempo.tempoBox()->value(), 120.0);
        QCOMPARE(tempo.tempo(), std::nullopt);
        tempo.followBox()->setChecked(false);
        tempo.tempoBox()->setValue(96);
        QCOMPARE(tempo.tempo(), std::optional(96.0));
    }

    // A default is shown as a number if every note has it and all are equal, and as (various)
    // if the notes differ. A pre-utterance or an overlap without a sample is left to the voice
    // bank. Without defaults, the defaults are unknown.
    void the_defaults_of_the_notes_are_shown() {
        kit::Note note;
        note.lyric = QStringLiteral("a");
        note.length = 480;
        using F = NotePropertiesDialog;
        const auto placeholder = [note](const QList<F::Defaults> &defaults, F::Field field) {
            NotePropertiesDialog dialog({note, note}, defaults);
            return dialog.field(field)->placeholderText();
        };
        const QList<F::Defaults> same = {
            {120, 12, 3},
            {120, 12, 3},
        };
        QCOMPARE(placeholder(same, F::Tempo), QStringLiteral("(default: 120)"));
        QCOMPARE(placeholder(same, F::PreUtterance), QStringLiteral("(default: 12)"));
        QCOMPARE(placeholder(same, F::VoiceOverlap), QStringLiteral("(default: 3)"));

        const QList<F::Defaults> unsampled = {
            {120, 12,           3           },
            {150, std::nullopt, std::nullopt},
        };
        QCOMPARE(placeholder(unsampled, F::Tempo), QStringLiteral("(various)"));
        QCOMPARE(placeholder(unsampled, F::PreUtterance), QStringLiteral("(default: voice bank)"));
        QCOMPARE(placeholder(unsampled, F::VoiceOverlap), QStringLiteral("(default: voice bank)"));

        QCOMPARE(placeholder({}, F::Tempo), QStringLiteral("(follows the tempo before)"));
        QCOMPARE(placeholder({}, F::PreUtterance), QStringLiteral("(default: voice bank)"));
    }

    // The tempo of the note is kept unless it is edited, even if it is out of the range of the
    // box, which is the range of UTAU.
    void the_tempo_dialog_keeps_an_unedited_tempo() {
        TempoDialog dialog(600.0, 120);
        QCOMPARE(dialog.tempoBox()->minimum(), utau::VALUE_TEMPO_MIN);
        QCOMPARE(dialog.tempoBox()->maximum(), utau::VALUE_TEMPO_MAX);
        QVERIFY(!dialog.followBox()->isChecked());
        QCOMPARE(dialog.tempo(), std::optional(600.0));

        dialog.tempoBox()->setValue(140);
        QCOMPARE(dialog.tempo(), std::optional(140.0));
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_NotePropertiesDialog test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_NotePropertiesDialog.moc"
