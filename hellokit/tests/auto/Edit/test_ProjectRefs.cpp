#include <QtTest/QTest>

#include <hellokit/Edit/ProjectSession.h>
#include <hellokit/Edit/ProjectRefs.h>

#include "ProjectSamples.h"

using namespace hello::kit;

class test_ProjectRefs : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void every_getter_reads_its_field() {
        const auto project = richProject();
        ProjectSession session(project);

        const auto &settings = project.settings;
        const auto settingsRef = ProjectRef(&session).settings();
        QCOMPARE(settingsRef.name(), settings.name);
        QCOMPARE(settingsRef.tempo(), settings.tempo);
        QCOMPARE(settingsRef.flags(), settings.flags);
        QCOMPARE(settingsRef.outputFile(), settings.outputFile);
        QCOMPARE(settingsRef.cacheDir(), settings.cacheDir);
        QCOMPARE(settingsRef.wavtool(), settings.wavtool);
        QCOMPARE(settingsRef.resampler(), settings.resampler);
        QCOMPARE(settingsRef.mode2(), settings.mode2);

        const auto &track = project.tracks.first();
        QCOMPARE(ProjectRef(&session).trackCount(), 1);
        const auto trackRef = ProjectRef(&session).track(0);
        QCOMPARE(trackRef.name(), track.name);
        QCOMPARE(trackRef.voiceDir(), track.voiceDir);
        QCOMPARE(trackRef.notes().size(), int(track.notes.size()));

        const auto &note = track.notes.first();
        const auto noteRef = trackRef.notes().at(0);
        QCOMPARE(noteRef.lyric(), note.lyric);
        QCOMPARE(noteRef.length(), note.length);
        QCOMPARE(noteRef.noteNum(), note.noteNum);
        QCOMPARE(noteRef.intensity(), note.intensity);
        QCOMPARE(noteRef.modulation(), note.modulation);
        QCOMPARE(noteRef.velocity(), note.velocity);
        QCOMPARE(noteRef.preUtterance(), note.preUtterance);
        QCOMPARE(noteRef.voiceOverlap(), note.voiceOverlap);
        QCOMPARE(noteRef.startPoint(), note.startPoint);
        QCOMPARE(noteRef.tempo(), note.tempo);
        QCOMPARE(noteRef.flags(), note.flags);
        QVERIFY(noteRef.envelope() == note.envelope);
        QVERIFY(noteRef.vibrato() == note.vibrato);
        QCOMPARE(noteRef.label(), note.label);
        QCOMPARE(noteRef.direct(), note.direct);
        QCOMPARE(noteRef.patch(), note.patch);
        QCOMPARE(noteRef.region(), note.region);
        QCOMPARE(noteRef.regionEnd(), note.regionEnd);

        const auto portamento = noteRef.portamento();
        QCOMPARE(portamento.size(), int(note.portamento.size()));
        for (int i = 0; i < portamento.size(); ++i) {
            const auto point = portamento.at(i);
            QCOMPARE(point.x(), note.portamento[i].x);
            QCOMPARE(point.y(), note.portamento[i].y);
            QCOMPARE(point.type(), note.portamento[i].type);
        }

        const auto pitchBend = noteRef.pitchBend();
        QVERIFY(pitchBend.isValid());
        QCOMPARE(pitchBend.start(), note.pitchBend->start);
        QCOMPARE(pitchBend.size(), int(note.pitchBend->values.size()));
        QCOMPARE(pitchBend.values(), note.pitchBend->values);

        const auto userData = noteRef.userData();
        QCOMPARE(userData.keys(), QStringList(note.userData.keys()));
        for (const auto &key : userData.keys()) {
            QCOMPARE(userData.value(key), note.userData.value(key));
        }

        QCOMPARE(noteRef.toNote().lyric, note.lyric);
    }

    // Each setter is applied to the session and the same change to a copy of the project. The
    // serializations are then equal only if every setter writes its own field.
    void every_setter_writes_its_field() {
        auto project = richProject();
        ProjectSession session(project);
        auto settingsRef = ProjectRef(&session).settings();
        auto trackRef = ProjectRef(&session).track(0);
        auto noteRef = trackRef.notes().at(0);
        auto pointRef = noteRef.portamento().at(1);
        auto pitchBendRef = noteRef.pitchBend();
        auto userDataRef = noteRef.userData();

        const Vibrato vibrato{50, 100, 20, 10, 10, 5, 5, 50};
        const auto envelope = Envelope::fromTimeOrder({
            {1, 2 },
            {3, 4 },
            {5, 6 },
            {7, 8 },
            {9, 10}
        });

        auto transaction = session.transaction(QStringLiteral("Edit every field"));
        settingsRef.setName(QStringLiteral("renamed"));
        settingsRef.setTempo(99);
        settingsRef.setFlags(QStringLiteral("Y0"));
        settingsRef.setOutputFile(QStringLiteral("other.wav"));
        settingsRef.setCacheDir(QStringLiteral("other.cache"));
        settingsRef.setWavtool(QStringLiteral("w.exe"));
        settingsRef.setResampler(QStringLiteral("r.exe"));
        settingsRef.setMode2(true);
        trackRef.setName(QStringLiteral("lead"));
        trackRef.setVoiceDir(QStringLiteral("%VOICE%other"));
        noteRef.setLyric(QStringLiteral("i"));
        noteRef.setLength(960);
        noteRef.setNoteNum(62);
        noteRef.setIntensity(std::nullopt);
        noteRef.setModulation(10);
        noteRef.setVelocity(std::nullopt);
        noteRef.setPreUtterance(1);
        noteRef.setVoiceOverlap(2);
        noteRef.setStartPoint(std::nullopt);
        noteRef.setTempo(std::nullopt);
        noteRef.setFlags(QStringLiteral("g+5"));
        noteRef.setEnvelope(envelope);
        noteRef.setVibrato(vibrato);
        noteRef.setLabel(QStringLiteral("chorus"));
        noteRef.setDirect(QString());
        noteRef.setPatch(QStringLiteral("p.exe"));
        noteRef.setRegion(QStringLiteral("C"));
        noteRef.setRegionEnd(QStringLiteral("D"));
        pointRef.setX(11);
        pointRef.setY(12);
        pointRef.setType(PortamentoPoint::J);
        pitchBendRef.setStart(std::nullopt);
        pitchBendRef.replaceValues(1, {7, 8});
        pitchBendRef.insertValues(0, {1});
        pitchBendRef.removeValues(4, 1);
        userDataRef.setValue(QStringLiteral("$custom"), QStringLiteral("changed"));
        userDataRef.setValue(QStringLiteral("$added"), QStringLiteral("new"));
        userDataRef.remove(QStringLiteral("Unknown"));
        transaction.commit();

        auto &settings = project.settings;
        settings.name = QStringLiteral("renamed");
        settings.tempo = 99;
        settings.flags = QStringLiteral("Y0");
        settings.outputFile = QStringLiteral("other.wav");
        settings.cacheDir = QStringLiteral("other.cache");
        settings.wavtool = QStringLiteral("w.exe");
        settings.resampler = QStringLiteral("r.exe");
        settings.mode2 = true;
        auto &track = project.tracks[0];
        track.name = QStringLiteral("lead");
        track.voiceDir = QStringLiteral("%VOICE%other");
        auto &note = track.notes[0];
        note.lyric = QStringLiteral("i");
        note.length = 960;
        note.noteNum = 62;
        note.intensity = std::nullopt;
        note.modulation = 10;
        note.velocity = std::nullopt;
        note.preUtterance = 1;
        note.voiceOverlap = 2;
        note.startPoint = std::nullopt;
        note.tempo = std::nullopt;
        note.flags = QStringLiteral("g+5");
        note.envelope = envelope;
        note.vibrato = vibrato;
        note.label = QStringLiteral("chorus");
        note.direct = QString();
        note.patch = QStringLiteral("p.exe");
        note.region = QStringLiteral("C");
        note.regionEnd = QStringLiteral("D");
        note.portamento[1] = {11, 12, PortamentoPoint::J};
        note.pitchBend->start = std::nullopt;
        note.pitchBend->values = {1, 0, 7, 8}; // {0, 10.5, -20, 0}, then replace, insert, remove
        note.userData[QStringLiteral("$custom")] = QStringLiteral("changed");
        note.userData[QStringLiteral("$added")] = QStringLiteral("new");
        note.userData.remove(QStringLiteral("Unknown"));

        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    void a_pitch_curve_is_added_and_removed_as_a_whole() {
        auto project = richProject();
        ProjectSession session(project);
        auto first = ProjectRef(&session).track(0).notes().at(0);
        auto second = ProjectRef(&session).track(0).notes().at(1);

        auto transaction = session.transaction(QStringLiteral("Pitch"));
        first.setPitchBend(std::nullopt);
        second.setPitchBend(PitchBend{
            5.0, {1, 2, 3}
        });
        transaction.commit();

        QVERIFY(!first.pitchBend().isValid());
        QCOMPARE(second.pitchBend().values(), QList<double>({1, 2, 3}));

        project.tracks[0].notes[0].pitchBend = std::nullopt;
        project.tracks[0].notes[1].pitchBend = PitchBend{
            5.0, {1, 2, 3}
        };
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    void lists_insert_remove_and_move_items() {
        auto project = richProject();
        ProjectSession session(project);
        auto notes = ProjectRef(&session).track(0).notes();
        auto portamento = notes.at(0).portamento();

        Note added;
        added.lyric = QStringLiteral("u");
        added.length = 120;
        added.noteNum = 64;

        auto transaction = session.transaction(QStringLiteral("Lists"));
        notes.insert(1, {added, added});
        notes.move(0, 1, 2);
        notes.remove(3, 1);
        portamento.insert(4, {
                                 {5, 6, PortamentoPoint::R}
        });
        portamento.remove(0, 1);
        portamento.move(0, 2, 1);
        transaction.commit();

        auto &track = project.tracks[0];
        auto &points = track.notes[0].portamento;
        points.insert(4, {5, 6, PortamentoPoint::R});
        points.remove(0);
        points.move(2, 0); // moving the first two by one equals moving the third to the front
        track.notes.insert(1, 2, added);
        track.notes.move(0, 2);
        track.notes.remove(3);

        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }
};

QTEST_APPLESS_MAIN(test_ProjectRefs)

#include "test_ProjectRefs.moc"
