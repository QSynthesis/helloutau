#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Edit/ProjectSession.h>
#include <hellokit/Edit/TrackTimeline.h>

#include <helloutau/Widgets/SceneView.h>
#include <helloutau/Widgets/TimelineRuler.h>

#include <helloutau/Editor/PianoRoll.h>

using namespace hello;
using namespace hello::daw;

class test_PianoRoll : public QObject {
    Q_OBJECT

private:
    // A quarter note la at C4, a half note rest that sets 60, a quarter note li at E4
    static kit::Project threeNotes() {
        kit::Note la;
        la.lyric = QStringLiteral("la");
        la.length = 480;
        la.noteNum = 60;
        kit::Note rest;
        rest.lyric = QStringLiteral("R");
        rest.length = 960;
        rest.noteNum = 60;
        rest.tempo = 60;
        kit::Note li;
        li.lyric = QStringLiteral("li");
        li.length = 480;
        li.noteNum = 64;

        kit::Track track;
        track.notes = {la, rest, li};
        kit::Project project;
        project.settings.tempo = 120;
        project.tracks.push_back(track);
        return project;
    }

    static kit::edit::NodeId idOf(kit::ProjectSession &session, int index) {
        return kit::ProjectRef(&session).tracks().at(0).notes().at(index).id();
    }

    // The middle of the bar of a note from \a tick to \a tick + \a length at \a key
    static QPointF middleOf(const PianoRoll &roll, double tick, double length, int key) {
        return {roll.view()->timeAxis().toX(tick + length / 2),
                roll.view()->keyAxis().toY(key + 0.5)};
    }

private Q_SLOTS:
    void a_note_is_hit_where_it_is_drawn() {
        kit::ProjectSession session(threeNotes());
        PianoRoll roll(&session);
        roll.resize(800, 600);
        roll.show();
        roll.scrollToNotes();

        const auto hit = roll.view()->hitAt(middleOf(roll, 0, 480, 60));
        QVERIFY(hit);
        QCOMPARE(hit->node, quint64(idOf(session, 0)));
        QCOMPARE(hit->part, int(PianoRoll::NoteBody));

        QCOMPARE(roll.view()->hitAt(middleOf(roll, 480, 960, 60))->node, quint64(idOf(session, 1)));
        QCOMPARE(roll.view()->hitAt(middleOf(roll, 1440, 480, 64))->node,
                 quint64(idOf(session, 2)));

        // The row of another key, or past the last note, is empty.
        QVERIFY(!roll.view()->hitAt(middleOf(roll, 0, 480, 61)));
        QVERIFY(!roll.view()->hitAt(middleOf(roll, 1920, 480, 64)));
    }

    void an_edit_moves_what_is_drawn() {
        kit::ProjectSession session(threeNotes());
        PianoRoll roll(&session);
        roll.resize(800, 600);
        roll.show();
        roll.scrollToNotes();

        {
            auto tx = session.transaction(QStringLiteral("lengthen"));
            kit::ProjectRef(&session).tracks().at(0).notes().at(0).setLength(960);
            QVERIFY(tx.commit());
        }
        QCOMPARE(roll.view()->hitAt(middleOf(roll, 480, 480, 60))->node, quint64(idOf(session, 0)));
        QCOMPARE(roll.view()->hitAt(middleOf(roll, 1920, 480, 64))->node,
                 quint64(idOf(session, 2)));
    }

    void the_ruler_shows_the_tempo_where_it_is_set() {
        kit::ProjectSession session(threeNotes());
        PianoRoll roll(&session);
        const auto marks = roll.ruler()->marks();
        QCOMPARE(marks.size(), 2);
        QCOMPARE(marks[0].tick, 0.0);
        QCOMPARE(marks[0].text, QStringLiteral("120"));
        QCOMPARE(marks[1].tick, 480.0);
        QCOMPARE(marks[1].text, QStringLiteral("60"));

        // Updated once the changes are over
        {
            auto tx = session.transaction(QStringLiteral("tempo"));
            kit::ProjectRef(&session).tracks().at(0).notes().at(2).setTempo(150.5);
            QVERIFY(tx.commit());
        }
        QTRY_COMPARE(roll.ruler()->marks().size(), 3);
        QCOMPARE(roll.ruler()->marks()[2].text, QStringLiteral("150.5"));
    }

    // The keys the notes use are in view after opening, and the track scrolls to its start.
    void the_notes_are_in_view_after_opening() {
        kit::ProjectSession session(threeNotes());
        PianoRoll roll(&session);
        roll.resize(800, 600);
        roll.show();
        roll.scrollToNotes();

        const auto &keys = roll.view()->keyAxis();
        const int height = roll.view()->viewport()->height();
        QVERIFY(keys.toY(65) >= 0 && keys.toY(65) <= height);
        QVERIFY(keys.toY(60) >= 0 && keys.toY(60) <= height);
        QCOMPARE(roll.view()->timeAxis().left, 0.0);
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_PianoRoll test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_PianoRoll.moc"
