#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QRandomGenerator>
#include <QtTest/QTest>

#include <hellokit/Edit/EditSession.h>

using namespace hello::kit;

class test_EditSession : public QObject {
    Q_OBJECT

private:
    // A project in which every field holds a value other than its default, including the values
    // that a conversion is most likely to lose: zero in an optional field, an envelope with and
    // without the middle anchor, every portamento type, and every kind of JSON value.
    static Project richProject() {
        Project project;
        project.settings.name = QString::fromUtf8("中文工程");
        project.settings.tempo = 134.5;
        project.settings.flags = QStringLiteral("g-3B40");
        project.settings.outputFile = QStringLiteral("out.wav");
        project.settings.cacheDir = QStringLiteral("song.cache");
        project.settings.wavtool = QStringLiteral("C:/tools/wavtool.exe");
        project.settings.resampler = QStringLiteral("C:/tools/resampler.exe");
        project.settings.mode2 = false;

        Note first;
        first.lyric = QString::fromUtf8("あ");
        first.length = 480;
        first.noteNum = 60;
        first.intensity = 0;
        first.modulation = 0;
        first.velocity = 150;
        first.preUtterance = 12.5;
        first.voiceOverlap = -3;
        first.startPoint = 7;
        first.tempo = 128;
        first.flags = QStringLiteral("B50");
        first.envelope = Envelope::fromTimeOrder({
            {0,  0  },
            {5,  100},
            {35, 90 },
            {10, 0  }
        });
        first.vibrato = Vibrato{65, 180, 35, 20, 20, 10, -5, 100};
        first.portamento = {
            {-40, 0,  PortamentoPoint::S     },
            {20,  -5, PortamentoPoint::Linear},
            {30,  3,  PortamentoPoint::R     },
            {10,  0,  PortamentoPoint::J     }
        };
        first.pitchBend = PitchBend{
            -20.0, {0, 10.5, -20, 0}
        };
        first.label = QStringLiteral("verse");
        first.direct = QStringLiteral("direct");
        first.patch = QStringLiteral("C:/tools/patch.exe");
        first.region = QStringLiteral("A");
        first.regionEnd = QStringLiteral("B");
        first.userData.insert(QStringLiteral("$custom"), QStringLiteral("kept"));
        first.userData.insert(QStringLiteral("Unknown"), QString());

        Note second;
        second.lyric = QStringLiteral("R");
        second.length = 240;
        second.noteNum = 60;
        second.tempo = 90;
        second.envelope = Envelope::fromTimeOrder({
            {0,  0  },
            {5,  100},
            {20, 80 },
            {35, 90 },
            {10, 0  }
        });
        second.pitchBend = PitchBend{std::nullopt, {}};

        Track track;
        track.name = QStringLiteral("vocal");
        track.voiceDir = QStringLiteral("%VOICE%uta");
        track.notes = {first, second};
        project.tracks.push_back(track);

        project.unknownFields.insert(QStringLiteral("object"), QJsonObject{
                                                                   {QStringLiteral("a"), 1}
        });
        project.unknownFields.insert(QStringLiteral("array"), QJsonArray{1, QStringLiteral("b")});
        project.unknownFields.insert(QStringLiteral("null"), QJsonValue::Null);
        project.unknownFields.insert(QStringLiteral("number"), 2.5);
        project.unknownFields.insert(QStringLiteral("string"), QStringLiteral("text"));
        project.unknownFields.insert(QStringLiteral("bool"), false);
        return project;
    }

    template <class T>
    static std::optional<T> maybe(QRandomGenerator &random, T value) {
        return random.bounded(2) ? std::optional<T>(value) : std::nullopt;
    }

    static QString maybeText(QRandomGenerator &random, const char *text) {
        return random.bounded(2) ? QString::fromUtf8(text) : QString();
    }

    // A project in which each optional part is present or absent at random.
    static Project randomProject(quint32 seed) {
        QRandomGenerator random(seed);

        Project project;
        project.settings.tempo = 60 + random.bounded(180);
        project.settings.mode2 = random.bounded(2);
        project.settings.flags = maybeText(random, "g-5");

        Track track;
        track.voiceDir = maybeText(random, "%VOICE%uta");
        for (int i = 0; i < 200; ++i) {
            Note note;
            note.lyric = random.bounded(4) ? QStringLiteral("a") : QStringLiteral("R");
            note.length = 15 * (1 + random.bounded(64));
            note.noteNum = 24 + random.bounded(84);
            note.intensity = maybe(random, double(random.bounded(200)));
            note.modulation = maybe(random, double(random.bounded(-200, 200)));
            note.velocity = maybe(random, double(random.bounded(200)));
            note.preUtterance = maybe(random, random.bounded(100.0));
            note.voiceOverlap = maybe(random, random.bounded(100.0));
            note.startPoint = maybe(random, random.bounded(100.0));
            note.tempo = maybe(random, 60 + random.bounded(180.0));
            note.flags = maybeText(random, "B40");

            if (random.bounded(2)) {
                Envelope envelope;
                envelope.hasMiddle = random.bounded(2);
                for (auto &anchor : envelope.anchors) {
                    anchor = {random.bounded(50.0), random.bounded(200.0)};
                }
                note.envelope = envelope;
            }
            if (random.bounded(2)) {
                note.vibrato =
                    Vibrato{random.bounded(100.0), random.bounded(512.0), random.bounded(100.0),
                            random.bounded(100.0), random.bounded(100.0), random.bounded(100.0),
                            random.bounded(100.0), random.bounded(100.0)};
            }
            for (int j = random.bounded(5); j > 0; --j) {
                note.portamento.push_back({random.bounded(100.0), random.bounded(100.0) - 50,
                                           PortamentoPoint::Type(random.bounded(4))});
            }
            if (random.bounded(2)) {
                PitchBend pitchBend;
                pitchBend.start = maybe(random, random.bounded(200.0) - 100);
                for (int j = random.bounded(50); j > 0; --j) {
                    pitchBend.values.push_back(random.bounded(400.0) - 200);
                }
                note.pitchBend = pitchBend;
            }
            note.label = maybeText(random, "label");
            note.region = maybeText(random, "A");
            for (int j = random.bounded(3); j > 0; --j) {
                note.userData.insert(QStringLiteral("$key%1").arg(j), QString::number(j));
            }
            track.notes.push_back(note);
        }
        project.tracks.push_back(track);
        return project;
    }

private Q_SLOTS:
    // Acceptance criterion 1 of docs/Editing.md. The comparison uses the .usth serialization,
    // which covers every field and distinguishes an absent optional field from zero.
    void every_field_survives_the_round_trip_through_the_tree() {
        const auto project = richProject();
        const EditSession session(project);
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    void a_random_project_survives_the_round_trip_through_the_tree() {
        for (quint32 seed = 1; seed <= 20; ++seed) {
            const auto project = randomProject(seed);
            const EditSession session(project);
            QVERIFY2(session.snapshot().toJson() == project.toJson(),
                     qPrintable(QStringLiteral("seed %1").arg(seed)));
        }
    }

    void an_empty_track_survives_the_round_trip_through_the_tree() {
        Project project;
        project.tracks.push_back(Track());
        const EditSession session(project);
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }
};

QTEST_APPLESS_MAIN(test_EditSession)

#include "test_EditSession.moc"
