#include <QtCore/QTimer>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>

#include <hellokit/Edit/ProjectEdits.h>
#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Edit/ProjectSession.h>
#include <hellokit/Edit/TrackTimeline.h>
#include <hellokit/Synth/PitchCurve.h>
#include <hellokit/VoiceBank/VoiceBank.h>

#include <helloutau/Widgets/SceneView.h>
#include <helloutau/Widgets/TimelineRuler.h>

#include <helloutau/Editor/PianoRoll.h>
#include <helloutau/Editor/VibratoDialog.h>

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

    static void show(PianoRoll &roll) {
        roll.resize(800, 600);
        roll.show();
        roll.scrollToNotes();
    }

    // The point at \a tick in the middle of the row of \a key
    static QPoint pointOf(const PianoRoll &roll, double tick, int key) {
        return QPointF(roll.view()->timeAxis().toX(tick), roll.view()->keyAxis().toY(key + 0.5))
            .toPoint();
    }

    static void click(PianoRoll &roll, double tick, int key, Qt::KeyboardModifiers modifiers = {}) {
        QTest::mouseClick(roll.view()->viewport(), Qt::LeftButton, modifiers,
                          pointOf(roll, tick, key));
    }

    // Drags from (tick, key) \a from to \a to, through a point on the way so that the drag
    // passes the start distance.
    static void drag(PianoRoll &roll, std::pair<double, int> from, std::pair<double, int> to,
                     Qt::KeyboardModifiers modifiers = {}) {
        const auto viewport = roll.view()->viewport();
        const auto start = pointOf(roll, from.first, from.second);
        const auto end = pointOf(roll, to.first, to.second);
        QTest::mousePress(viewport, Qt::LeftButton, modifiers, start);
        QTest::mouseMove(viewport, (start + end) / 2);
        QTest::mouseMove(viewport, end);
        QTest::mouseRelease(viewport, Qt::LeftButton, modifiers, end);
    }

    static QString lyricsOf(const kit::ProjectSession &session) {
        const auto project = session.snapshot();
        QStringList lyrics;
        for (const auto &note : project.tracks[0].notes) {
            lyrics.push_back(note.lyric);
        }
        return lyrics.join(u' ');
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

        // The row of another key, or past the last note, is background.
        QCOMPARE(roll.view()->hitAt(middleOf(roll, 0, 480, 61))->part, int(PianoRoll::Background));
        QCOMPARE(roll.view()->hitAt(middleOf(roll, 1920, 480, 64))->part,
                 int(PianoRoll::Background));

        // The right edge of a note changes its length.
        const auto edge = roll.view()->hitAt(middleOf(roll, 0, 950, 60));
        QCOMPARE(edge->node, quint64(idOf(session, 0)));
        QCOMPARE(edge->part, int(PianoRoll::NoteEnd));
        QCOMPARE(edge->cursor, Qt::SizeHorCursor);
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

    // A sung note is looked up as synthesis looks it up; a rest is never missing a sample.
    void a_note_without_a_sample_is_marked() {
        kit::ProjectSession session(threeNotes());
        PianoRoll roll(&session);

        // Without a voice bank nothing is known of the samples.
        QVERIFY(!roll.lacksSample(0));
        QVERIFY(!roll.lacksSample(2));

        kit::VoiceSample la;
        la.path = "la.wav";
        la.fileName = QStringLiteral("la.wav");
        la.alias = QStringLiteral("la");
        la.hasEntry = true;
        auto bank = std::make_shared<const kit::VoiceBank>(
            "bank", QList<kit::VoiceBankDirectory>{kit::VoiceBankDirectory()},
            QList<kit::VoiceSample>{la});
        roll.setVoiceBank(bank);
        QCOMPARE(roll.voiceBank(), bank);
        QVERIFY(!roll.lacksSample(0));
        QVERIFY(!roll.lacksSample(1));
        QVERIFY(roll.lacksSample(2));

        // The lookup follows the tree.
        {
            auto tx = session.transaction(QStringLiteral("rename"));
            kit::ProjectRef(&session).tracks().at(0).notes().at(2).setLyric(QStringLiteral("la"));
            QVERIFY(tx.commit());
        }
        QVERIFY(!roll.lacksSample(2));
    }

    void a_click_selects_and_modifiers_extend_the_selection() {
        kit::ProjectSession session(threeNotes());
        PianoRoll roll(&session);
        show(roll);

        click(roll, 240, 60);
        QCOMPARE(roll.selectedIndices(), QList<int>{0});
        click(roll, 1680, 64, Qt::ControlModifier);
        QCOMPARE(roll.selectedIndices(), (QList<int>{0, 2}));
        click(roll, 240, 60, Qt::ControlModifier);
        QCOMPARE(roll.selectedIndices(), QList<int>{2});
        click(roll, 1680, 64, Qt::ShiftModifier);
        QCOMPARE(roll.selectedIndices(), (QList<int>{0, 1, 2}));

        // A click on one of several selected notes selects it alone, and one beside the notes
        // selects nothing.
        click(roll, 960, 60);
        QCOMPARE(roll.selectedIndices(), QList<int>{1});
        click(roll, 240, 70);
        QVERIFY(roll.selectedIndices().isEmpty());
        QCOMPARE(session.currentStep(), 0);
    }

    void a_rectangle_selects_the_notes_it_touches() {
        kit::ProjectSession session(threeNotes());
        PianoRoll roll(&session);
        show(roll);

        drag(roll, {0, 70}, {2000, 62});
        QCOMPARE(roll.selectedIndices(), QList<int>{2});
        drag(roll, {10, 61}, {100, 60}, Qt::ControlModifier);
        QCOMPARE(roll.selectedIndices(), (QList<int>{0, 2}));
        drag(roll, {10, 61}, {100, 60});
        QCOMPARE(roll.selectedIndices(), QList<int>{0});
    }

    // A drag moves the note in the sequence and transposes it in one step, and changes no
    // length.
    void a_drag_moves_and_transposes_in_one_step() {
        const auto project = threeNotes();
        kit::ProjectSession session(project);
        PianoRoll roll(&session);
        show(roll);

        drag(roll, {1680, 64}, {240, 66});
        QCOMPARE(lyricsOf(session), QStringLiteral("li la R"));
        const auto notes = session.snapshot().tracks[0].notes;
        QCOMPARE(notes[0].noteNum, 66);
        QCOMPARE(notes[1].noteNum, 60);
        QCOMPARE(notes[0].length, 480);
        QCOMPARE(notes[2].length, 960);
        QCOMPARE(session.currentStep(), 1);
        QCOMPARE(session.undoMessage(), PianoRoll::tr("Move Notes"));
        QCOMPARE(roll.selectedIndices(), QList<int>{0});

        session.undo();
        QCOMPARE(session.snapshot().toJson(), project.toJson());

        // Dragged 1000 ticks later, la lands on the nearest boundary, after the rest.
        drag(roll, {240, 60}, {1240, 60});
        QCOMPARE(lyricsOf(session), QStringLiteral("R la li"));
        session.undo();

        // A vertical drag only transposes.
        drag(roll, {240, 60}, {240, 59});
        QCOMPARE(lyricsOf(session), QStringLiteral("la R li"));
        QCOMPARE(session.snapshot().tracks[0].notes[0].noteNum, 59);
        QCOMPARE(session.undoMessage(), PianoRoll::tr("Transpose"));
    }

    void escape_abandons_a_drag() {
        kit::ProjectSession session(threeNotes());
        PianoRoll roll(&session);
        show(roll);
        click(roll, 1680, 64);

        const auto viewport = roll.view()->viewport();
        QTest::mousePress(viewport, Qt::LeftButton, {}, pointOf(roll, 240, 60));
        QTest::mouseMove(viewport, pointOf(roll, 1680, 66));
        QTest::keyClick(roll.view(), Qt::Key_Escape);
        QTest::mouseRelease(viewport, Qt::LeftButton, {}, pointOf(roll, 1680, 66));
        QCOMPARE(session.currentStep(), 0);
        QVERIFY(!session.canUndo());
        QCOMPARE(roll.selectedIndices(), QList<int>{0});

        // A drag extends a selection with gaps to the run it spans; Escape restores it.
        click(roll, 1680, 64, Qt::ControlModifier);
        QTest::mousePress(viewport, Qt::LeftButton, {}, pointOf(roll, 240, 60));
        QTest::mouseMove(viewport, pointOf(roll, 240, 63));
        QCOMPARE(roll.selectedIndices(), (QList<int>{0, 1, 2}));
        QTest::keyClick(roll.view(), Qt::Key_Escape);
        QTest::mouseRelease(viewport, Qt::LeftButton, {}, pointOf(roll, 240, 63));
        QCOMPARE(roll.selectedIndices(), (QList<int>{0, 2}));
        QVERIFY(!session.canUndo());
    }

    // The right edge changes the length, snapped to the quantization.
    void the_right_edge_changes_the_length() {
        kit::ProjectSession session(threeNotes());
        PianoRoll roll(&session);
        show(roll);
        QCOMPARE(roll.quantization(), 120);

        drag(roll, {470, 60}, {700, 60});
        QCOMPARE(session.snapshot().tracks[0].notes[0].length, 720);
        QCOMPARE(session.undoMessage(), kit::ProjectEdits::tr("Change Length"));

        // Without quantization the length follows the pointer.
        roll.setQuantization(0);
        drag(roll, {710, 60}, {800, 60});
        QCOMPARE(session.snapshot().tracks[0].notes[0].length, 800);
    }

    // The pen draws a note after the last one, and fills the gap before it with a rest.
    void the_pen_draws_after_the_last_note() {
        kit::ProjectSession session(threeNotes());
        PianoRoll roll(&session);
        show(roll);
        roll.setTool(PianoRoll::PenTool);

        drag(roll, {2400, 62}, {2900, 62});
        QCOMPARE(lyricsOf(session), QStringLiteral("la R li R la"));
        const auto notes = session.snapshot().tracks[0].notes;
        QCOMPARE(notes[3].length, 480);
        QCOMPARE(notes[4].length, 480);
        QCOMPARE(notes[4].noteNum, 62);
        QCOMPARE(session.currentStep(), 1);
        QCOMPARE(roll.selectedIndices(), QList<int>{4});

        // Before the end the pen selects, as the select tool does.
        drag(roll, {0, 70}, {1900, 62});
        QCOMPARE(roll.selectedIndices(), QList<int>{2});
        QCOMPARE(session.currentStep(), 1);
    }

    void the_selection_is_deleted_transposed_and_inserted_before() {
        kit::ProjectSession session(threeNotes());
        PianoRoll roll(&session);
        kit::DiagnosticList diagnostics;

        roll.setSelectedIndices({0, 2});
        QVERIFY(roll.transposeSelected(12, diagnostics));
        QCOMPARE(session.snapshot().tracks[0].notes[2].noteNum, 76);

        QVERIFY(roll.removeSelected(diagnostics));
        QCOMPARE(lyricsOf(session), QStringLiteral("R"));
        QVERIFY(roll.selectedIndices().isEmpty());

        // With nothing selected, a note is appended with the key of the last note.
        QVERIFY(roll.insertNote(diagnostics));
        QCOMPARE(lyricsOf(session), QStringLiteral("R la"));
        QCOMPARE(roll.selectedIndices(), QList<int>{1});
        QVERIFY(roll.insertNote(diagnostics));
        QCOMPARE(lyricsOf(session), QStringLiteral("R la la"));
        QCOMPARE(roll.selectedIndices(), QList<int>{1});
        QCOMPARE(session.snapshot().tracks[0].notes[1].length, roll.quantization());
        QVERIFY(diagnostics.empty());
    }

    // The view stays still while the playhead is in it, and follows once it leaves.
    void the_view_follows_the_playhead() {
        kit::ProjectSession session(threeNotes());
        PianoRoll roll(&session);
        show(roll);
        const auto &time = roll.view()->timeAxis();
        const double visible = roll.view()->viewport()->width() / time.pixelsPerTick;

        roll.setPlayheadPosition(visible / 2);
        QCOMPARE(roll.playheadPosition(), std::optional<double>(visible / 2));
        QCOMPARE(time.left, 0.0);

        roll.setPlayheadPosition(visible * 1.5);
        QCOMPARE(time.left, visible * 1.4);
        roll.setPlayheadPosition(std::nullopt);
        QVERIFY(!roll.playheadPosition());
        QCOMPARE(time.left, visible * 1.4);
    }

    // The portamento runs through the rows as the resampler receives it, and the vibrato apart
    // around the middle of the row; both only while the pitch is shown.
    void the_pitch_is_drawn_as_the_resampler_receives_it() {
        kit::Note la;
        la.lyric = QStringLiteral("la");
        la.length = 960;
        la.noteNum = 60;
        kit::Vibrato vibrato;
        vibrato.length = 50;
        vibrato.period = 200;
        vibrato.amplitude = 80;
        la.vibrato = vibrato;
        kit::Note li;
        li.lyric = QStringLiteral("li");
        li.length = 480;
        li.noteNum = 64;
        kit::PortamentoPoint first;
        first.x = -100;
        kit::PortamentoPoint second;
        second.x = 100;
        li.portamento = {first, second};
        kit::Project project;
        project.settings.tempo = 120;
        project.tracks.push_back({});
        project.tracks[0].notes = {la, li};

        kit::ProjectSession session(project);
        PianoRoll roll(&session);
        roll.setPitchColor(QColor(255, 0, 255));
        roll.setVibratoColor(QColor(0, 255, 255));
        show(roll);

        // Whether a pixel of \a color lies within two pixels of (tick, cents from key)
        const auto drawnNear = [&roll](double tick, int key, double cents, QColor color) {
            const auto image = roll.view()->viewport()->grab().toImage();
            const QPointF center(roll.view()->timeAxis().toX(tick),
                                 roll.view()->keyAxis().toY(key + 0.5 + cents / 100));
            for (int dx = -2; dx <= 2; ++dx) {
                for (int dy = -2; dy <= 2; ++dy) {
                    const auto pixel = image.pixelColor(center.toPoint() + QPoint(dx, dy));
                    if (qAbs(pixel.red() - color.red()) < 60 &&
                        qAbs(pixel.green() - color.green()) < 60 &&
                        qAbs(pixel.blue() - color.blue()) < 60) {
                        return true;
                    }
                }
            }
            return false;
        };

        const QList<kit::Note> notes = {la, li};
        const kit::PitchCurve secondCurve(notes, 1, 120);
        // From 100 ms before li to 100 ms after it, li bends up from la; at its start halfway
        const double bend = secondCurve.portamentoAt(0);
        QVERIFY(bend < -100 && bend > -300);
        QVERIFY(drawnNear(960, 64, bend, QColor(255, 0, 255)));
        QVERIFY(drawnNear(960 + 300, 64, 0, QColor(255, 0, 255)));

        const kit::PitchCurve firstCurve(notes, 0, 120);
        const double tick = 960 * 0.5 + 120;
        const double v = firstCurve.vibratoAt(tick);
        QVERIFY(qAbs(v) > 20);
        QVERIFY(drawnNear(tick, 60, v, QColor(0, 255, 255)));

        roll.setPitchVisible(false);
        QVERIFY(!drawnNear(960 + 300, 64, 0, QColor(255, 0, 255)));
        QVERIFY(!drawnNear(tick, 60, v, QColor(0, 255, 255)));
    }

    // la at C4, then li at D4 with points 60 ms before its start, at its start 100 cents up,
    // and 60 ms after, at 120 bpm, where a millisecond is 0.96 ticks
    static kit::Project bentNotes() {
        kit::Note la;
        la.lyric = QStringLiteral("la");
        la.length = 480;
        la.noteNum = 60;
        kit::Note li;
        li.lyric = QStringLiteral("li");
        li.length = 480;
        li.noteNum = 62;
        for (const auto &[x, y] : {
                 std::pair{-60.0, 0.0  },
                 {0.0,   100.0},
                 {60.0,  0.0  }
        }) {
            kit::PortamentoPoint point;
            point.x = x;
            point.y = y;
            li.portamento.push_back(point);
        }
        kit::Project project;
        project.settings.tempo = 120;
        project.tracks.push_back({});
        project.tracks[0].notes = {la, li};
        return project;
    }

    // Where point x, cents of li is drawn
    static QPoint pointOfLi(const PianoRoll &roll, double x, double cents) {
        return QPointF(roll.view()->timeAxis().toX(480 + x * 0.96),
                       roll.view()->keyAxis().toY(62.5 + cents / 100))
            .toPoint();
    }

    // Shows the roll with a pixel for each millisecond at 120 bpm and whole pixels for whole
    // rows, so that a drag between points gives whole milliseconds
    static void showExactly(PianoRoll &roll) {
        show(roll);
        auto time = roll.view()->timeAxis();
        time.left = 0;
        time.pixelsPerTick = 1 / 0.96;
        roll.view()->setTimeAxis(time);
        auto keys = roll.view()->keyAxis();
        keys.top = 70;
        keys.pixelsPerKey = 24;
        roll.view()->setKeyAxis(keys);
    }

    // Presses without modifiers, since Ctrl on the press selects instead, and releases with
    // modifiers, which snap
    static void dragPoint(PianoRoll &roll, QPoint from, QPoint to,
                          Qt::KeyboardModifiers modifiers = {}) {
        const auto viewport = roll.view()->viewport();
        QTest::mousePress(viewport, Qt::LeftButton, {}, from);
        QTest::mouseMove(viewport, (from + to) / 2);
        QTest::mouseMove(viewport, to);
        QTest::mouseRelease(viewport, Qt::LeftButton, modifiers, to);
    }

    static QList<kit::PortamentoPoint> pointsOfLi(const kit::ProjectSession &session) {
        return session.snapshot().tracks[0].notes[1].portamento;
    }

    // A point moves in time and height in one step; the first point after a sung note and the
    // last one move only in time, and a point passes its neighbours into its place in time.
    void a_point_is_dragged_past_its_neighbours() {
        kit::ProjectSession session(bentNotes());
        PianoRoll roll(&session);
        showExactly(roll);

        // 48 ticks later is 50 ms; a row up is 100 cents.
        dragPoint(roll, pointOfLi(roll, 0, 100), pointOfLi(roll, 50, 200));
        auto points = pointsOfLi(session);
        QCOMPARE(points[1].x, 50.0);
        QCOMPARE(points[1].y, 200.0);
        QCOMPARE(session.undoMessage(), PianoRoll::tr("Move Pitch Points"));
        QCOMPARE(roll.selectedPoints(), (QList<std::pair<int, int>>{
                                            {1, 1}
        }));
        QVERIFY(roll.selectedIndices().isEmpty());

        // The first point is drawn at the pitch of la, 200 cents below li.
        dragPoint(roll, pointOfLi(roll, -60, -200), pointOfLi(roll, -30, 0));
        points = pointsOfLi(session);
        QCOMPARE(points[0].x, -30.0);
        QCOMPARE(points[0].y, 0.0);

        // The last point passes the second, at 50 ms, keeping its height, and stays selected.
        dragPoint(roll, pointOfLi(roll, 60, 0), pointOfLi(roll, 0, 100));
        points = pointsOfLi(session);
        QCOMPARE(points[1].x, 0.0);
        QCOMPARE(points[1].y, 0.0);
        QCOMPARE(points[2].x, 50.0);
        QCOMPARE(points[2].y, 200.0);
        QCOMPARE(roll.selectedPoints(), (QList<std::pair<int, int>>{
                                            {1, 1}
        }));
    }

    // The points of the note whose portamento is under the pointer are drawn plainly, all others
    // faintly: a plain point is filled, a faint one is only a thin ring through which the
    // portamento shows.
    void the_points_of_the_note_under_the_pointer_stand_out() {
        kit::ProjectSession session(bentNotes());
        PianoRoll roll(&session);
        roll.setPitchColor(QColor(255, 0, 255));
        roll.setWhiteRowColor(QColor(0, 255, 0));
        showExactly(roll);
        const auto viewport = roll.view()->viewport();
        // Sent directly, since a synthesized move does not always reach the viewport
        const auto hoverAt = [viewport](QPoint position) {
            QMouseEvent move(QEvent::MouseMove, position, viewport->mapToGlobal(position),
                             Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(viewport, &move);
        };
        const auto centerIs = [&](QColor color) {
            const auto pixel = viewport->grab().toImage().pixelColor(pointOfLi(roll, 0, 100));
            return qAbs(pixel.red() - color.red()) < 80 &&
                   qAbs(pixel.green() - color.green()) < 80 &&
                   qAbs(pixel.blue() - color.blue()) < 80;
        };

        // With nothing under the pointer every point is faint.
        QVERIFY(centerIs(QColor(255, 0, 255)));

        // On the portamento of la, whose curve runs along the middle of its row
        hoverAt(pointOfLi(roll, -300, -200));
        QVERIFY(centerIs(QColor(255, 0, 255)));

        // On the portamento of li
        hoverAt(pointOfLi(roll, 250, 0));
        QVERIFY(centerIs(QColor(0, 255, 0)));

        // On li, but away from its portamento
        hoverAt(pointOfLi(roll, 250, 60));
        QVERIFY(centerIs(QColor(255, 0, 255)));

        // A selection does not make points plain.
        roll.setSelectedIndices({1});
        QVERIFY(centerIs(QColor(255, 0, 255)));

        // A drag keeps the note it began on, wherever the pointer goes.
        hoverAt(pointOfLi(roll, 250, 0));
        QTest::mousePress(viewport, Qt::LeftButton, {}, pointOfLi(roll, -60, -200));
        const auto elsewhere = pointOfLi(roll, -300, 300);
        QMouseEvent drag(QEvent::MouseMove, elsewhere, viewport->mapToGlobal(elsewhere),
                         Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(viewport, &drag);
        QVERIFY(centerIs(QColor(0, 255, 0)));
        QTest::keyClick(roll.view(), Qt::Key_Escape);
        QTest::mouseRelease(viewport, Qt::LeftButton, {}, elsewhere);
    }

    // Any point may lie before the start of its note, not only the first.
    void a_later_point_moves_before_its_note() {
        kit::ProjectSession session(bentNotes());
        PianoRoll roll(&session);
        showExactly(roll);

        dragPoint(roll, pointOfLi(roll, 0, 100), pointOfLi(roll, -30, 100));
        QCOMPARE(pointsOfLi(session)[1].x, -30.0);
        QCOMPARE(session.undoMessage(), PianoRoll::tr("Move Pitch Points"));
    }

    // Selected points move together by the same time, passing the others, and stay selected.
    void selected_points_move_together() {
        kit::ProjectSession session(bentNotes());
        PianoRoll roll(&session);
        showExactly(roll);

        roll.setSelectedPoints({
            {1, 0},
            {1, 1}
        });
        dragPoint(roll, pointOfLi(roll, 0, 100), pointOfLi(roll, 100, 100));
        const auto points = pointsOfLi(session);
        QCOMPARE(points[0].x, 40.0);
        QCOMPARE(points[1].x, 60.0);
        QCOMPARE(points[2].x, 100.0);
        QCOMPARE(points[2].y, 100.0);
        QCOMPARE(roll.selectedPoints(), (QList<std::pair<int, int>>{
                                            {1, 0},
                                            {1, 2}
        }));
    }

    // Shift snaps the time to another point of the note, Ctrl the height to 50 cents; Escape
    // leaves the points as they were.
    void a_point_snaps_with_modifiers() {
        kit::ProjectSession session(bentNotes());
        PianoRoll roll(&session);
        showExactly(roll);

        dragPoint(roll, pointOfLi(roll, 0, 100), pointOfLi(roll, 20, 170), Qt::ControlModifier);
        auto points = pointsOfLi(session);
        QCOMPARE(points[1].y, 150.0);

        dragPoint(roll, pointOfLi(roll, 20, 150), pointOfLi(roll, 50, 150), Qt::ShiftModifier);
        points = pointsOfLi(session);
        QCOMPARE(points[1].x, 60.0);

        const int step = session.currentStep();
        const auto viewport = roll.view()->viewport();
        QTest::mousePress(viewport, Qt::LeftButton, {}, pointOfLi(roll, 60, 150));
        QTest::mouseMove(viewport, pointOfLi(roll, 10, 300));
        QTest::keyClick(roll.view(), Qt::Key_Escape);
        QTest::mouseRelease(viewport, Qt::LeftButton, {}, pointOfLi(roll, 10, 300));
        QCOMPARE(session.currentStep(), step);
    }

    // A double click on the portamento inserts a point there; Delete removes the selected
    // points, keeping two.
    void points_are_inserted_and_deleted() {
        kit::ProjectSession session(bentNotes());
        PianoRoll roll(&session);
        showExactly(roll);

        // After its last point the portamento of li is at its own pitch.
        QTest::mouseDClick(roll.view()->viewport(), Qt::LeftButton, {}, pointOfLi(roll, 250, 0));
        auto points = pointsOfLi(session);
        QCOMPARE(points.size(), 4);
        QCOMPARE(points[3].x, 250.0);
        QCOMPARE(points[3].y, 0.0);
        QCOMPARE(session.undoMessage(), PianoRoll::tr("Insert Pitch Point"));
        QCOMPARE(roll.selectedPoints(), (QList<std::pair<int, int>>{
                                            {1, 3}
        }));
        QVERIFY(!roll.lyricEditor()->isVisible());

        roll.setSelectedPoints({
            {1, 0},
            {1, 1},
            {1, 2},
            {1, 3}
        });
        kit::DiagnosticList diagnostics;
        QVERIFY(roll.removeSelected(diagnostics));
        points = pointsOfLi(session);
        QCOMPARE(points.size(), 2);
        QCOMPARE(points[0].x, -60.0);
        QCOMPARE(points[1].x, 250.0);
        QCOMPARE(session.undoMessage(), PianoRoll::tr("Delete Pitch Points"));
        QCOMPARE(session.snapshot().tracks[0].notes.size(), 2);

        // Selecting a note clears the selected points.
        roll.setSelectedPoints({
            {1, 0}
        });
        roll.setSelectedIndices({0});
        QVERIFY(roll.selectedPoints().isEmpty());

        // Hidden, the pitch has no points to hit.
        roll.setPitchVisible(false);
        QTest::mouseDClick(roll.view()->viewport(), Qt::LeftButton, {}, pointOfLi(roll, 250, 0));
        QCOMPARE(pointsOfLi(session).size(), 2);
    }

    // With the pitch shown, a rectangle selects the points in it if there are any, and the
    // notes otherwise; Ctrl adds to the selection.
    void a_rectangle_selects_points_before_notes() {
        kit::ProjectSession session(bentNotes());
        PianoRoll roll(&session);
        showExactly(roll);
        const auto band = [&roll](QPoint from, QPoint to, Qt::KeyboardModifiers modifiers = {}) {
            const auto viewport = roll.view()->viewport();
            QTest::mousePress(viewport, Qt::LeftButton, modifiers, from);
            QTest::mouseMove(viewport, (from + to) / 2);
            QTest::mouseMove(viewport, to);
            QTest::mouseRelease(viewport, Qt::LeftButton, modifiers, to);
        };

        // Over the first two points of li, which also touches la
        band(pointOfLi(roll, -80, 300), pointOfLi(roll, 20, -300));
        QCOMPARE(roll.selectedPoints(), (QList<std::pair<int, int>>{
                                            {1, 0},
                                            {1, 1}
        }));
        QVERIFY(roll.selectedIndices().isEmpty());

        band(pointOfLi(roll, 40, 170), pointOfLi(roll, 80, -50), Qt::ControlModifier);
        QCOMPARE(roll.selectedPoints(), (QList<std::pair<int, int>>{
                                            {1, 0},
                                            {1, 1},
                                            {1, 2}
        }));

        // Over la alone, which has no points
        band(pointOfLi(roll, -400, -120), pointOfLi(roll, -300, -280));
        QVERIFY(roll.selectedPoints().isEmpty());
        QCOMPARE(roll.selectedIndices(), QList<int>{0});
    }

    // The default two points go to the selected notes without points; once all have points,
    // the command removes them.
    void portamento_is_added_and_removed() {
        kit::ProjectSession session(bentNotes());
        PianoRoll roll(&session);
        show(roll);
        roll.setSelectedIndices({0, 1});

        kit::DiagnosticList diagnostics;
        QVERIFY(roll.togglePortamento(diagnostics));
        auto notes = session.snapshot().tracks[0].notes;
        QCOMPARE(notes[0].portamento.size(), 2);
        QCOMPARE(notes[0].portamento[0].x, -15.0);
        QCOMPARE(notes[0].portamento[1].x, 15.0);
        QCOMPARE(notes[1].portamento.size(), 3);
        QCOMPARE(session.undoMessage(), PianoRoll::tr("Add Portamento"));

        QVERIFY(roll.togglePortamento(diagnostics));
        notes = session.snapshot().tracks[0].notes;
        QVERIFY(notes[0].portamento.isEmpty());
        QVERIFY(notes[1].portamento.isEmpty());
        QCOMPARE(session.undoMessage(), PianoRoll::tr("Remove Portamento"));
    }

    // The default vibrato goes to the selected sung notes without one; once all have one, the
    // command removes them. Rests are left alone.
    void vibrato_is_added_and_removed() {
        kit::ProjectSession session(threeNotes());
        PianoRoll roll(&session);
        show(roll);
        roll.setSelectedIndices({0, 1, 2});

        kit::DiagnosticList diagnostics;
        QVERIFY(roll.toggleVibrato(diagnostics));
        auto notes = session.snapshot().tracks[0].notes;
        QCOMPARE(notes[0].vibrato, std::optional<kit::Vibrato>(VibratoDialog::defaultVibrato()));
        QVERIFY(!notes[1].vibrato);
        QCOMPARE(notes[2].vibrato, std::optional<kit::Vibrato>(VibratoDialog::defaultVibrato()));
        QCOMPARE(session.undoMessage(), PianoRoll::tr("Add Vibrato"));

        QVERIFY(roll.toggleVibrato(diagnostics));
        notes = session.snapshot().tracks[0].notes;
        QVERIFY(!notes[0].vibrato);
        QVERIFY(!notes[2].vibrato);
        QCOMPARE(session.undoMessage(), PianoRoll::tr("Remove Vibrato"));
    }

    // Each handle of the vibrato changes one value, rounded as UTAU shows it. With a pixel to a
    // millisecond, the default vibrato of la spans 325 ms from 175 px to 500 px; its trapezoid
    // stands on y 300 and is 35 / 50 rows high, its fades end at 240 px and begin at 435 px, and
    // its period box spans 175 px to 355 px below it, down to y 312.
    void the_handles_of_a_vibrato_change_its_values() {
        const auto dragged = [this](QPoint from, QPoint to, double phase = 0) {
            kit::Note la;
            la.lyric = QStringLiteral("la");
            la.length = 480;
            la.noteNum = 60;
            la.vibrato = VibratoDialog::defaultVibrato();
            la.vibrato->phase = phase;
            kit::Project project;
            project.settings.tempo = 120;
            project.tracks.push_back({});
            project.tracks[0].notes = {la};
            kit::ProjectSession session(project);
            PianoRoll roll(&session);
            showExactly(roll);
            dragPoint(roll, from, to);
            if (session.canUndo()) {
                [&] { QCOMPARE(session.undoMessage(), kit::ProjectEdits::tr("Change Vibrato")); }();
            }
            return *session.snapshot().tracks[0].notes[0].vibrato;
        };

        QCOMPARE(dragged({175, 300}, {225, 300}).length, 55.0);
        QCOMPARE(dragged({240, 283}, {305, 283}).attack, 40.0);
        QCOMPARE(dragged({435, 283}, {403, 283}).release, 30.0);
        QCOMPARE(dragged({337, 283}, {337, 259}).amplitude, 85.0);
        QCOMPARE(dragged({355, 306}, {375, 306}).period, 200.0);
        QCOMPARE(dragged({265, 306}, {301, 306}).phase, 20.0);
        // At a phase of half a period the box spans 265 px to 445 px, and grows with the period.
        QCOMPARE(dragged({445, 306}, {475, 306}, 50).period, 200.0);
        // Nothing else changes.
        auto expected = VibratoDialog::defaultVibrato();
        expected.phase = 20;
        QCOMPARE(dragged({265, 306}, {301, 306}), expected);
    }

    // The distance from the portamento within which a double click inserts a point is a
    // property that a style sheet sets; beyond it the double click edits the lyric.
    void the_distance_to_the_portamento_is_a_property() {
        kit::ProjectSession session(bentNotes());
        PianoRoll roll(&session);
        roll.setStyleSheet(QStringLiteral("hello--daw--PianoRoll { qproperty-curveGrip: 2; }"));
        showExactly(roll);
        QCOMPARE(roll.curveGrip(), 2.0);

        const auto below = pointOfLi(roll, 250, 0) + QPoint(0, 4);
        QTest::mouseDClick(roll.view()->viewport(), Qt::LeftButton, {}, below);
        QCOMPARE(pointsOfLi(session).size(), 3);
        QVERIFY(roll.lyricEditor()->isVisible());
        QTest::keyClick(roll.lyricEditor(), Qt::Key_Escape);

        roll.setCurveGrip(5);
        QTest::mouseDClick(roll.view()->viewport(), Qt::LeftButton, {}, below);
        QCOMPARE(pointsOfLi(session).size(), 4);
    }

    // The context menu of a point changes the shape of the segment that ends at it, and removes
    // it while more than two remain.
    void the_context_menu_changes_a_point() {
        kit::ProjectSession session(bentNotes());
        PianoRoll roll(&session);
        showExactly(roll);

        // Chooses the item named text from the menu once it is open.
        const auto choose = [](const QString &text) {
            QTimer::singleShot(0, [text] {
                const auto menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
                QVERIFY(menu);
                for (const auto action : menu->actions()) {
                    if (action->text() == text) {
                        QVERIFY(action->isEnabled());
                        action->trigger();
                    }
                }
                menu->close();
            });
        };
        const auto viewport = roll.view()->viewport();
        choose(PianoRoll::tr("R-Curve"));
        QTest::mouseClick(viewport, Qt::RightButton, {}, pointOfLi(roll, 0, 100));
        QCOMPARE(pointsOfLi(session)[1].type, kit::PortamentoPoint::R);
        QCOMPARE(session.undoMessage(), PianoRoll::tr("Change Pitch Point"));

        choose(PianoRoll::tr("Delete Point"));
        QTest::mouseClick(viewport, Qt::RightButton, {}, pointOfLi(roll, 0, 100));
        QCOMPARE(pointsOfLi(session).size(), 2);
    }

    // Tab commits the lyric and edits the next note; Escape leaves the lyric as it was.
    void a_lyric_is_edited_in_place() {
        kit::ProjectSession session(threeNotes());
        PianoRoll roll(&session);
        show(roll);
        const auto editor = roll.lyricEditor();

        roll.editLyric(0);
        QVERIFY(editor->isVisible());
        QCOMPARE(editor->text(), QStringLiteral("la"));
        editor->setText(QStringLiteral("ka"));
        QTest::keyClick(editor, Qt::Key_Tab);
        QCOMPARE(lyricsOf(session), QStringLiteral("ka R li"));
        QCOMPARE(editor->text(), QStringLiteral("R"));
        QCOMPARE(roll.selectedIndices(), QList<int>{1});

        editor->setText(QStringLiteral("x"));
        QTest::keyClick(editor, Qt::Key_Escape);
        QVERIFY(!editor->isVisible());
        QCOMPARE(lyricsOf(session), QStringLiteral("ka R li"));

        // A double click on the note away from its portamento edits it, and Return commits it.
        const QPointF upper(roll.view()->timeAxis().toX(1680), roll.view()->keyAxis().toY(64.85));
        QTest::mouseDClick(roll.view()->viewport(), Qt::LeftButton, {}, upper.toPoint());
        QVERIFY(editor->isVisible());
        QCOMPARE(editor->text(), QStringLiteral("li"));
        editor->setText(QStringLiteral("ki"));
        QTest::keyClick(editor, Qt::Key_Return);
        QVERIFY(!editor->isVisible());
        QCOMPARE(lyricsOf(session), QStringLiteral("ka R ki"));
        QCOMPARE(session.undoMessage(), PianoRoll::tr("Change Lyric"));
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
