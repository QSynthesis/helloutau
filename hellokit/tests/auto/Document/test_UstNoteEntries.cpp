#include <QtTest/QTest>

#include <hellokit/Document/UstNoteEntries.h>

using namespace hello::kit;

class test_UstNoteEntries : public QObject {
    Q_OBJECT

private:
    // A note with an entry of each built-in field and of the user data
    static Note richNote() {
        Note note;
        note.lyric = QStringLiteral("a");
        note.length = 480;
        note.noteNum = 60;
        note.intensity = 80;
        note.envelope = Envelope::fromTimeOrder({
            {0,  0  },
            {5,  100},
            {35, 100},
            {0,  0  }
        });
        Vibrato vibrato;
        vibrato.length = 65;
        vibrato.period = 180;
        vibrato.amplitude = 35;
        vibrato.attack = 20;
        vibrato.release = 20;
        note.vibrato = vibrato;
        PortamentoPoint first;
        first.x = -40;
        PortamentoPoint second;
        second.x = 20;
        note.portamento = {first, second};
        note.label = QStringLiteral("verse");
        note.direct = QStringLiteral("True");
        note.patch = QStringLiteral("breath.wav");
        note.regions = {QStringLiteral("A")};
        note.userData.insert(QStringLiteral("$plugin"), QStringLiteral("x=1"));
        return note;
    }

    static bool hasKey(const QStringList &lines, const QString &key) {
        for (const auto &line : lines) {
            if (line.startsWith(key + QLatin1Char('='))) {
                return true;
            }
        }
        return false;
    }

private Q_SLOTS:
    // The built-in lines are those of the fields without a place in the note properties, and are
    // read back to the same fields. The user data are lines of their own.
    void the_built_in_entries_are_written_and_read_back() {
        const auto note = richNote();
        const auto lines = UstNoteEntries::builtInLinesOf(note);
        for (const auto key :
             {"Envelope", "VBR", "PBS", "PBW", "Label", "$direct", "$patch", "$region"}) {
            QVERIFY2(hasKey(lines, QLatin1String(key)), key);
        }
        for (const auto key : {"Lyric", "Length", "NoteNum", "Intensity", "$plugin"}) {
            QVERIFY2(!hasKey(lines, QLatin1String(key)), key);
        }
        QCOMPARE(UstNoteEntries::userDataLinesOf(note), QStringList{QStringLiteral("$plugin=x=1")});

        DiagnosticList diagnostics;
        Note other;
        other.lyric = QStringLiteral("b");
        const auto read = UstNoteEntries::withBuiltInLines(other, lines, diagnostics);
        QVERIFY(read);
        QVERIFY(diagnostics.isEmpty());
        QCOMPARE(read->lyric, QStringLiteral("b"));
        QCOMPARE(UstNoteEntries::builtInLinesOf(*read), lines);

        // Without the lines, the fields are removed.
        const auto cleared = UstNoteEntries::withBuiltInLines(note, {}, diagnostics);
        QVERIFY(cleared);
        QVERIFY(!cleared->envelope && !cleared->vibrato && cleared->portamento.isEmpty());
        QVERIFY(cleared->label.isEmpty() && cleared->direct.isEmpty() && cleared->patch.isEmpty());
        QCOMPARE(cleared->userData, note.userData);
    }

    // A value written otherwise than a UST writes it is accepted if it reads to the same value:
    // a number formatted otherwise, a trailing part left out, another name of the Mode1 pitch,
    // and an empty value for no entry.
    void the_formatting_of_a_value_does_not_matter() {
        DiagnosticList diagnostics;
        const auto read = UstNoteEntries::withBuiltInLines(
            Note(),
            {QStringLiteral("VBR=65.0,180,35,20,20,0,0"),
             QStringLiteral("Envelope=0,5,35,0,100,100,0,%,0"), QStringLiteral(""),
             QStringLiteral("PBType=5"), QStringLiteral("PBStart=-10"),
             QStringLiteral("Pitches=1,2,3"), QStringLiteral("$direct=")},
            diagnostics);
        QVERIFY2(read, qPrintable(diagnostics.value(0).message));
        QCOMPARE(read->vibrato->length, 65.0);
        QVERIFY(read->envelope);
        QVERIFY(read->pitchBend);
        QVERIFY(read->direct.isEmpty());
    }

    // A line of another key, a repeated key, a line without =, and a value that a UST reads
    // only by correcting it are refused, each with an error.
    void a_line_that_is_not_a_built_in_entry_is_refused() {
        const QStringList wrong = {
            QStringLiteral("Lyric=a"), QStringLiteral("$plugin=1"), QStringLiteral("Label=a"),
            QStringLiteral("Label=b"), QStringLiteral("Envelope"),
        };
        DiagnosticList diagnostics;
        QVERIFY(!UstNoteEntries::withBuiltInLines(Note(), wrong, diagnostics));
        QCOMPARE(diagnostics.size(), 4);
        QVERIFY(hasError(diagnostics));

        for (const auto value :
             {"VBR=a,b,c", "VBR=65,180,35,20,20,0,0,0,9", "PBS=x", "Envelope=0,5"}) {
            diagnostics.clear();
            QVERIFY2(!UstNoteEntries::withBuiltInLines(Note(), {QLatin1String(value)}, diagnostics),
                     value);
            QCOMPARE(diagnostics.size(), 1);
        }
    }

    // The user data are the part before the first = as the key and the rest as the value. A key
    // that is empty, has a space or is read by UTAU, a repeated key and a line without = are
    // refused, each with an error.
    void the_user_data_are_keys_and_values() {
        DiagnosticList diagnostics;
        const auto userData = UstNoteEntries::userDataOf(
            {QStringLiteral("$a=x=y"), QStringLiteral(""), QStringLiteral("b=")}, diagnostics);
        QVERIFY(userData);
        QCOMPARE(userData->value(QStringLiteral("$a")), QStringLiteral("x=y"));
        QVERIFY(userData->contains(QStringLiteral("b")));

        const QStringList wrong = {
            QStringLiteral("=1"),         QStringLiteral("a b=1"), QStringLiteral("Lyric=x"),
            QStringLiteral("Envelope=x"), QStringLiteral("$c=1"),  QStringLiteral("$c=2"),
            QStringLiteral("none"),
        };
        diagnostics.clear();
        QVERIFY(!UstNoteEntries::userDataOf(wrong, diagnostics));
        QCOMPARE(diagnostics.size(), 6);
    }

    // An entry that UTAU drops or changes on saving has a warning: a key without $, and a value
    // that is empty or has a tab, = or a space.
    void the_changes_of_utau_are_warned() {
        QMap<QString, QString> userData;
        userData.insert(QStringLiteral("$kept"), QStringLiteral("a,b"));
        QVERIFY(UstNoteEntries::utauChangesOf(userData).isEmpty());
        userData.insert(QStringLiteral("plain"), QStringLiteral("a"));
        userData.insert(QStringLiteral("$empty"), QString());
        userData.insert(QStringLiteral("$tab"), QStringLiteral("a\tb"));
        userData.insert(QStringLiteral("$equal"), QStringLiteral("a=b"));
        userData.insert(QStringLiteral("$space"), QStringLiteral("a b"));
        const auto warnings = UstNoteEntries::utauChangesOf(userData);
        QCOMPARE(warnings.size(), 5);
        QVERIFY(!hasError(warnings));
    }
};

QTEST_GUILESS_MAIN(test_UstNoteEntries)

#include "test_UstNoteEntries.moc"
