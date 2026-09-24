#include <QtTest/QSignalSpy>
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
        QCOMPARE(ProjectRef(&session).tracks().size(), 1);
        const auto trackRef = ProjectRef(&session).tracks().at(0);
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
        QCOMPARE(pitchBend.valuesSize(), int(note.pitchBend->values.size()));
        QCOMPARE(pitchBend.values(), note.pitchBend->values);

        const auto userData = noteRef.userData();
        QCOMPARE(userData.keys(), QStringList(note.userData.keys()));
        for (const auto &key : userData.keys()) {
            QCOMPARE(userData.value(key), note.userData.value(key));
        }

        const auto unknownFields = ProjectRef(&session).unknownFields();
        QCOMPARE(unknownFields.keys(), project.unknownFields.keys());
        QVERIFY(unknownFields.contains(QStringLiteral("array")));
        QCOMPARE(unknownFields.value(QStringLiteral("number")), QJsonValue(2.5));
    }

    // Writing the current value of every field, entry and element changes nothing: no change is
    // reported and no undo step is created. This relies on the equality of every value type,
    // including the envelope, the vibrato and the pitch curve.
    void writing_the_current_values_changes_nothing() {
        const auto project = richProject();
        ProjectSession session(project);
        const auto projectRef = ProjectRef(&session);
        QSignalSpy changed(&session, &EditSession::changed);

        auto transaction = session.transaction(QStringLiteral("Nothing"));
        const auto settings = projectRef.settings();
        settings.setName(settings.name());
        settings.setTempo(settings.tempo());
        settings.setFlags(settings.flags());
        settings.setOutputFile(settings.outputFile());
        settings.setCacheDir(settings.cacheDir());
        settings.setWavtool(settings.wavtool());
        settings.setResampler(settings.resampler());
        settings.setMode2(settings.mode2());

        const auto unknownFields = projectRef.unknownFields();
        for (const auto &key : unknownFields.keys()) {
            unknownFields.setValue(key, unknownFields.value(key));
        }

        const auto track = projectRef.tracks().at(0);
        track.setName(track.name());
        track.setVoiceDir(track.voiceDir());

        const auto notes = track.notes();
        for (int i = 0; i < notes.size(); ++i) {
            const auto note = notes.at(i);
            note.setLyric(note.lyric());
            note.setLength(note.length());
            note.setNoteNum(note.noteNum());
            note.setIntensity(note.intensity());
            note.setModulation(note.modulation());
            note.setVelocity(note.velocity());
            note.setPreUtterance(note.preUtterance());
            note.setVoiceOverlap(note.voiceOverlap());
            note.setStartPoint(note.startPoint());
            note.setTempo(note.tempo());
            note.setFlags(note.flags());
            note.setEnvelope(note.envelope());
            note.setVibrato(note.vibrato());
            note.setLabel(note.label());
            note.setDirect(note.direct());
            note.setPatch(note.patch());
            note.setRegion(note.region());
            note.setRegionEnd(note.regionEnd());

            const auto portamento = note.portamento();
            for (int j = 0; j < portamento.size(); ++j) {
                const auto point = portamento.at(j);
                point.setX(point.x());
                point.setY(point.y());
                point.setType(point.type());
            }

            const auto pitchBend = note.pitchBend();
            if (pitchBend.isValid()) {
                pitchBend.setStart(pitchBend.start());
                pitchBend.replaceValues(0, pitchBend.values());
                note.setPitchBend(pitchBend.toPitchBend());
            } else {
                note.setPitchBend(std::nullopt);
            }

            const auto userData = note.userData();
            for (const auto &key : userData.keys()) {
                userData.setValue(key, userData.value(key));
            }
        }
        transaction.commit();

        QCOMPARE(changed.count(), 0);
        QVERIFY(!session.canUndo());
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    // Each record handle returns a copy of its record, which the serialization compares field
    // by field.
    void every_record_handle_returns_a_copy_of_its_record() {
        const auto project = richProject();
        ProjectSession session(project);
        const auto projectRef = ProjectRef(&session);
        const auto trackRef = projectRef.tracks().at(0);
        const auto noteRef = trackRef.notes().at(0);

        QCOMPARE(projectRef.toProject().toJson(), project.toJson());

        Project copy;
        copy.settings = projectRef.settings().toProjectSettings();
        copy.tracks = {trackRef.toTrack()};
        copy.unknownFields = project.unknownFields;
        QCOMPARE(copy.toJson(), project.toJson());

        auto note = project.tracks[0].notes[0];
        QCOMPARE(noteRef.toNote().lyric, note.lyric);
        QCOMPARE(noteRef.pitchBend().toPitchBend().values, note.pitchBend->values);
        QCOMPARE(noteRef.pitchBend().toPitchBend().start, note.pitchBend->start);
        const auto point = noteRef.portamento().at(2).toPortamentoPoint();
        QCOMPARE(point.x, note.portamento[2].x);
        QCOMPARE(point.type, note.portamento[2].type);

        QCOMPARE(NoteRef().toNote().lyric, QString());
    }

    // Each setter is applied to the session and the same change to a copy of the project. The
    // serializations are then equal only if every setter writes its own field.
    void every_setter_writes_its_field() {
        auto project = richProject();
        ProjectSession session(project);
        auto settingsRef = ProjectRef(&session).settings();
        auto trackRef = ProjectRef(&session).tracks().at(0);
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
        const auto unknownFieldsRef = ProjectRef(&session).unknownFields();
        unknownFieldsRef.setValue(QStringLiteral("number"), QJsonValue(3));
        unknownFieldsRef.remove(QStringLiteral("null"));
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
        project.unknownFields.insert(QStringLiteral("number"), 3);
        project.unknownFields.remove(QStringLiteral("null"));

        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    void a_pitch_curve_is_added_and_removed_as_a_whole() {
        auto project = richProject();
        ProjectSession session(project);
        auto first = ProjectRef(&session).tracks().at(0).notes().at(0);
        auto second = ProjectRef(&session).tracks().at(0).notes().at(1);

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
        auto notes = ProjectRef(&session).tracks().at(0).notes();
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

    // The list of tracks follows the convention of every list. A project with more than one
    // track is not valid, which the validation at commit reports, not the list.
    void the_track_list_has_the_operations_of_every_list() {
        const auto project = richProject();
        ProjectSession session(project);
        const auto tracks = ProjectRef(&session).tracks();

        Track added;
        added.name = QStringLiteral("added");

        auto transaction = session.transaction(QStringLiteral("Tracks"));
        tracks.insert(1, {added});
        QCOMPARE(tracks.size(), 2);
        QCOMPARE(tracks.at(1).name(), QStringLiteral("added"));
        tracks.move(1, 1, 0);
        QCOMPARE(tracks.at(0).name(), QStringLiteral("added"));
        tracks.remove(0, 1);
        transaction.commit();

        QCOMPARE(tracks.size(), 1);
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }
};

QTEST_APPLESS_MAIN(test_ProjectRefs)

#include "test_ProjectRefs.moc"
