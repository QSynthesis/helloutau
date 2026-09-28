#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Edit/ProjectSession.h>
#include <hellokit/Edit/TrackTimeline.h>

using namespace hello::kit;

class test_TrackTimeline : public QObject {
    Q_OBJECT

private:
    // A quarter note la, a rest of a half note that sets 60, and a quarter note li
    static Project threeNotes() {
        Note la;
        la.lyric = QStringLiteral("la");
        la.length = 480;
        la.noteNum = 60;
        Note rest;
        rest.lyric = QStringLiteral("R");
        rest.length = 960;
        rest.noteNum = 60;
        rest.tempo = 60;
        Note li;
        li.lyric = QStringLiteral("li");
        li.length = 480;
        li.noteNum = 64;

        Track track;
        track.notes = {la, rest, li};
        Project project;
        project.settings.tempo = 120;
        project.tracks.push_back(track);
        return project;
    }

private Q_SLOTS:
    void the_notes_are_laid_out_one_after_another() {
        ProjectSession session(threeNotes());
        TrackTimeline timeline(&session);

        QCOMPARE(timeline.noteCount(), 3);
        QCOMPARE(timeline.note(0).start, 0);
        QCOMPARE(timeline.note(1).start, 480);
        QCOMPARE(timeline.note(2).start, 1440);
        QCOMPARE(timeline.length(), 1920);
        QVERIFY(!timeline.note(0).rest);
        QVERIFY(timeline.note(1).rest);
        QCOMPARE(timeline.note(1).tempo, std::optional<double>(60));
        QCOMPARE(timeline.note(2).key, 64);
        QCOMPARE(timeline.note(2).lyric, QStringLiteral("li"));
        QCOMPARE(timeline.note(1).id, ProjectRef(&session).tracks().at(0).notes().at(1).id());

        // 480 ticks at 120 last 500 ms, then 960 at 60 last 2000 ms
        QCOMPARE(timeline.tempoMap().startTime(2), 2500.0);
    }

    void the_notes_between_two_ticks_are_found() {
        ProjectSession session(threeNotes());
        TrackTimeline timeline(&session);

        QCOMPARE(timeline.notesBetween(0, 100), std::make_pair(0, 1));
        QCOMPARE(timeline.notesBetween(400, 500), std::make_pair(0, 2));
        QCOMPARE(timeline.notesBetween(-1000, 5000), std::make_pair(0, 3));
        QCOMPARE(timeline.notesBetween(5000, 6000), std::make_pair(3, 3));
        QCOMPARE(timeline.noteAt(1500), 2);
    }

    // A transaction of several changes invalidates the timeline once, and the next access
    // computes it from the tree.
    void a_change_invalidates_once_and_is_computed_on_access() {
        ProjectSession session(threeNotes());
        TrackTimeline timeline(&session);
        QSignalSpy spy(&timeline, &TrackTimeline::invalidated);
        QCOMPARE(timeline.noteCount(), 3);

        {
            auto tx = session.transaction(QStringLiteral("edit"));
            const auto notes = ProjectRef(&session).tracks().at(0).notes();
            notes.at(0).setLength(960);
            notes.at(2).setLyric(QStringLiteral("R"));
            QVERIFY(tx.commit());
        }
        QCOMPARE(spy.count(), 1);
        QCOMPARE(timeline.note(1).start, 960);
        QVERIFY(timeline.note(2).rest);

        session.undo();
        QCOMPARE(spy.count(), 2);
        QCOMPARE(timeline.note(1).start, 480);
    }
};

QTEST_APPLESS_MAIN(test_TrackTimeline)

#include "test_TrackTimeline.moc"
