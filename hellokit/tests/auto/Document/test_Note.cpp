#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtTest/QTest>

#include <hellokit/Document/Note.h>

using namespace hello::kit;

class test_Note : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void whitespace_lyrics_are_rests() {
        QVERIFY(Note::isRestLyric(QStringLiteral("")));
        QVERIFY(Note::isRestLyric(QStringLiteral("   \t\n")));
        QVERIFY(Note::isRestLyric(QStringLiteral("R")));
        QVERIFY(Note::isRestLyric(QStringLiteral("  r  ")));
        QVERIFY(!Note::isRestLyric(QStringLiteral("Ra")));
        QVERIFY(!Note::isRestLyric(QStringLiteral(" a ")));
    }

    void every_field_of_a_note_survives_a_json_round_trip() {
        Note note;
        note.lyric = QStringLiteral("la");
        note.length = 480;
        note.noteNum = 60;
        note.intensity = 80;
        note.modulation = 0;
        note.velocity = 120;
        note.preUtterance = 30;
        note.voiceOverlap = 10;
        note.startPoint = 5;
        note.tempo = 128.5;
        note.flags = QStringLiteral("g-5");
        note.envelope = Envelope::fromTimeOrder({
            {0,  0  },
            {5,  100},
            {20, 80 },
            {35, 100},
            {0,  0  }
        });
        note.vibrato = Vibrato{65, 180, 35, 20, 20, 0, 0, 1};
        note.portamento = {
            {-40, 0,  PortamentoPoint::S     },
            {50,  10, PortamentoPoint::R     },
            {20,  0,  PortamentoPoint::J     },
            {10,  -5, PortamentoPoint::Linear}
        };
        note.pitchBend = PitchBend{
            -20.0, {0, 10.5, -20}
        };
        note.label = QStringLiteral("verse");
        note.direct = QStringLiteral("direct.wav");
        note.patch = QStringLiteral("resampler.exe");
        note.regions = {QStringLiteral("A"), QStringLiteral("B")};
        note.regionEnds = {QStringLiteral("B")};
        note.userData.insert(QStringLiteral("$whatever"), QStringLiteral("kept"));

        DiagnosticList diagnostics;
        const auto back = Note::fromJson(note.toJson(), diagnostics);
        QVERIFY(back.has_value());
        QVERIFY(diagnostics.isEmpty());
        QCOMPARE(back->lyric, note.lyric);
        QCOMPARE(back->length, note.length);
        QCOMPARE(back->noteNum, note.noteNum);
        QCOMPARE(back->intensity, note.intensity);
        QCOMPARE(back->modulation, note.modulation);
        QCOMPARE(back->velocity, note.velocity);
        QCOMPARE(back->preUtterance, note.preUtterance);
        QCOMPARE(back->voiceOverlap, note.voiceOverlap);
        QCOMPARE(back->startPoint, note.startPoint);
        QCOMPARE(back->tempo, note.tempo);
        QCOMPARE(back->flags, note.flags);
        QVERIFY(back->envelope == note.envelope);
        QVERIFY(back->vibrato == note.vibrato);
        QCOMPARE(back->portamento.size(), note.portamento.size());
        for (int i = 0; i < note.portamento.size(); ++i) {
            QCOMPARE(back->portamento.at(i).x, note.portamento.at(i).x);
            QCOMPARE(back->portamento.at(i).y, note.portamento.at(i).y);
            QCOMPARE(back->portamento.at(i).type, note.portamento.at(i).type);
        }
        QVERIFY(back->pitchBend == note.pitchBend);
        QCOMPARE(back->label, note.label);
        QCOMPARE(back->direct, note.direct);
        QCOMPARE(back->patch, note.patch);
        QCOMPARE(back->regions, note.regions);
        QCOMPARE(back->regionEnds, note.regionEnds);
        QCOMPARE(back->userData, note.userData);
    }

    // An empty optional field is omitted rather than written as null or zero.
    void empty_fields_of_a_note_are_omitted() {
        Note note;
        note.lyric = QStringLiteral("la");
        note.length = 480;
        note.noteNum = 60;
        QCOMPARE(note.toJson().keys(),
                 QStringList({QStringLiteral("length"), QStringLiteral("lyric"),
                              QStringLiteral("noteNum")}));
    }

    void a_note_without_its_lyric_length_or_pitch_is_refused() {
        const QJsonObject complete{
            {QStringLiteral("lyric"),   QStringLiteral("a")},
            {QStringLiteral("length"),  480                },
            {QStringLiteral("noteNum"), 60                 },
        };
        DiagnosticList diagnostics;
        QVERIFY(Note::fromJson(complete, diagnostics).has_value());
        QVERIFY(diagnostics.isEmpty());

        for (const auto &key : complete.keys()) {
            auto incomplete = complete;
            incomplete.remove(key);
            diagnostics.clear();
            QVERIFY2(!Note::fromJson(incomplete, diagnostics).has_value(), qPrintable(key));
            QVERIFY(hasError(diagnostics));
        }
    }

    // A malformed optional field is reported and read as absent, so that the rest of the note is
    // still read.
    void a_malformed_optional_field_is_reported_and_read_as_absent() {
        const QJsonObject object{
            {QStringLiteral("lyric"),     QStringLiteral("a")   },
            {QStringLiteral("length"),    480                   },
            {QStringLiteral("noteNum"),   60                    },
            {QStringLiteral("intensity"), QStringLiteral("loud")},
        };
        DiagnosticList diagnostics;
        const auto note = Note::fromJson(object, diagnostics);
        QVERIFY(note.has_value());
        QVERIFY(!note->intensity.has_value());
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(diagnostics.at(0).severity, DiagnosticSeverity::Warning);
    }

    // A name of a region must be writable to UST, which joins the names with | and drops empty
    // ones. Elements that are not such names are therefore omitted on reading.
    void region_names_that_ust_cannot_hold_are_omitted() {
        const QJsonObject object{
            {QStringLiteral("lyric"),      QStringLiteral("a")                                                                       },
            {QStringLiteral("length"),     480                                                                                       },
            {QStringLiteral("noteNum"),    60                                                                                        },
            {QStringLiteral("regions"),    QJsonArray{QStringLiteral("A"), 1, QString(),
                                                   QStringLiteral("B|C"), QStringLiteral("D")}},
            {QStringLiteral("regionEnds"), QJsonArray{true, QStringLiteral("A")}                                                     },
        };
        DiagnosticList diagnostics;
        const auto note = Note::fromJson(object, diagnostics);
        QVERIFY(note.has_value());
        QCOMPARE(note->regions, (QStringList{QStringLiteral("A"), QStringLiteral("D")}));
        QCOMPARE(note->regionEnds, QStringList{QStringLiteral("A")});
    }

    void empty_region_lists_are_not_written() {
        Note note;
        note.lyric = QStringLiteral("la");
        note.length = 480;
        note.noteNum = 60;
        note.regions = {QStringLiteral("A")};
        const auto json = note.toJson();
        QCOMPARE(json.value(QStringLiteral("regions")).toArray(), QJsonArray{QStringLiteral("A")});
        QVERIFY(!json.contains(QStringLiteral("regionEnds")));
    }
};

QTEST_APPLESS_MAIN(test_Note)

#include "test_Note.moc"
