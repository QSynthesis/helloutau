#include <QtCore/QTimer>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QToolButton>

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
private:
    // A drag of the pen on the background of threeNotes (la 0-480, a rest 480-1440 that sets
    // the tempo 60, li 1440-1920) from tick from to tick to in the row of key 70, or at key; the
    // lyrics and lengths after it, the drawn note being selected, in one step
    struct Drawn {
        QString lyrics;
        QList<int> lengths;
        QList<std::optional<double>> tempos;
        QList<int> selected;
        int steps = 0;
    };

    static Drawn penDrag(double from, double to, Qt::KeyboardModifiers modifiers = {},
                         int key = 70) {
        kit::ProjectSession session(threeNotes());
        PianoRoll roll(&session);
        show(roll);
        roll.setTool(PianoRoll::PenTool);
        drag(roll, {from, key}, {to, key}, modifiers);
        Drawn drawn;
        drawn.lyrics = lyricsOf(session);
        const auto notes = session.snapshot().tracks[0].notes;
        for (const auto &note : notes) {
            drawn.lengths.push_back(note.length);
            drawn.tempos.push_back(note.tempo);
        }
        drawn.selected = roll.selectedIndices();
        drawn.steps = session.currentStep();
        return drawn;
    }

private Q_SLOTS:
    // The pen draws from the end of the previous note, before the note at the pointer, which
    // moves later; after the last note, from its end. A note that sets a tempo passes it on.
    void the_pen_draws_from_the_end_of_the_previous_note() {
        auto drawn = penDrag(2400, 2880, {}, 62);
        QCOMPARE(drawn.lyrics, QStringLiteral("la R li la"));
        QCOMPARE(drawn.lengths, (QList<int>{480, 960, 480, 960}));
        QCOMPARE(drawn.selected, QList<int>{3});
        QCOMPARE(drawn.steps, 1);

        drawn = penDrag(1700, 1800);
        QCOMPARE(drawn.lyrics, QStringLiteral("la R la li"));
        QCOMPARE(drawn.lengths, (QList<int>{480, 960, 360, 480}));
        QCOMPARE(drawn.selected, QList<int>{2});

        drawn = penDrag(900, 960);
        QCOMPARE(drawn.lyrics, QStringLiteral("la la R li"));
        QCOMPARE(drawn.lengths, (QList<int>{480, 480, 960, 480}));
        QCOMPARE(drawn.tempos[1], std::optional<double>(60));
        QCOMPARE(drawn.tempos[2], std::optional<double>(60));
        QCOMPARE(drawn.selected, QList<int>{1});
    }

    // With Shift, a rest fills the gap up to the pointer; within a rest, the note takes the place
    // of part of it, and the notes after it move only as far as the note passes its end.
    void the_pen_with_shift_draws_at_the_pointer() {
        auto drawn = penDrag(2400, 2880, Qt::ShiftModifier, 62);
        QCOMPARE(drawn.lyrics, QStringLiteral("la R li R la"));
        QCOMPARE(drawn.lengths, (QList<int>{480, 960, 480, 480, 480}));
        QCOMPARE(drawn.selected, QList<int>{4});
        QCOMPARE(drawn.steps, 1);

        drawn = penDrag(1700, 1800, Qt::ShiftModifier);
        QCOMPARE(drawn.lyrics, QStringLiteral("la R R la li"));
        QCOMPARE(drawn.lengths, (QList<int>{480, 960, 240, 120, 480}));
        QCOMPARE(drawn.selected, QList<int>{3});

        // Within the rest, which keeps what lies before and after the note
        drawn = penDrag(720, 960, Qt::ShiftModifier);
        QCOMPARE(drawn.lyrics, QStringLiteral("la R la R li"));
        QCOMPARE(drawn.lengths, (QList<int>{480, 240, 240, 480, 480}));
        QCOMPARE(drawn.tempos[1], std::optional<double>(60));
        QCOMPARE(drawn.selected, QList<int>{2});

        // Past its end, which moves li later
        drawn = penDrag(1200, 1680, Qt::ShiftModifier);
        QCOMPARE(drawn.lyrics, QStringLiteral("la R la li"));
        QCOMPARE(drawn.lengths, (QList<int>{480, 720, 480, 480}));

        // At its start, where the note takes its tempo
        drawn = penDrag(480, 720, Qt::ShiftModifier);
        QCOMPARE(drawn.lyrics, QStringLiteral("la la R li"));
        QCOMPARE(drawn.lengths, (QList<int>{480, 240, 720, 480}));
        QCOMPARE(drawn.tempos[1], std::optional<double>(60));
        QCOMPARE(drawn.selected, QList<int>{1});

        // Over the whole of it, which it replaces
        drawn = penDrag(480, 1440, Qt::ShiftModifier);
        QCOMPARE(drawn.lyrics, QStringLiteral("la la li"));
        QCOMPARE(drawn.lengths, (QList<int>{480, 960, 480}));
        QCOMPARE(drawn.tempos[1], std::optional<double>(60));
    }

    // On a note the pen selects, as the select tool does.
    void the_pen_selects_on_a_note() {
        kit::ProjectSession session(threeNotes());
        PianoRoll roll(&session);
        show(roll);
        roll.setTool(PianoRoll::PenTool);
        click(roll, 1700, 64);
        QCOMPARE(roll.selectedIndices(), QList<int>{2});
        QCOMPARE(session.currentStep(), 0);
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

    // A stroke with button from \a from to \a to, through the middle
    static void stroke(PianoRoll &roll, Qt::MouseButton button, QPoint from, QPoint to) {
        const auto viewport = roll.view()->viewport();
        QTest::mousePress(viewport, button, {}, from);
        QTest::mouseMove(viewport, (from + to) / 2);
        QTest::mouseMove(viewport, to);
        QTest::mouseRelease(viewport, button, {}, to);
    }

    // With Mode2 off, the points are hidden; the pitch tool draws the Mode1 values of the notes
    // along a stroke, a note without values starting them at its first reading, and a stroke
    // with the right button returns them to 0, each in one step.
    void mode1_values_are_drawn_and_erased() {
        auto project = bentNotes();
        project.settings.mode2 = false;
        kit::ProjectSession session(project);
        PianoRoll roll(&session);
        showExactly(roll);
        const auto hit = roll.view()->hitAt(pointOfLi(roll, 0, 100));
        QVERIFY(!hit || hit->part != PianoRoll::PitchPoint);
        roll.setTool(PianoRoll::PitchTool);

        // Across la, 100 cents above it, from about 100 ticks to about 300. Without a voice
        // bank the first reading is at the start of the note.
        const auto &time = roll.view()->timeAxis();
        const auto from = QPointF(time.toX(100), roll.view()->keyAxis().toY(61.5)).toPoint();
        const auto to = QPointF(time.toX(300), roll.view()->keyAxis().toY(61.5)).toPoint();
        stroke(roll, Qt::LeftButton, from, to);
        const auto first = int(std::ceil(time.toTick(from.x()) / 5));
        const auto last = int(std::floor(time.toTick(to.x()) / 5));
        QList<double> values(first, 0);
        values.append(QList<double>(last - first + 1, 100));
        auto bend = session.snapshot().tracks[0].notes[0].pitchBend;
        QVERIFY(bend);
        QCOMPARE(bend->start, std::optional<double>(0));
        QCOMPARE(bend->values, values);
        QVERIFY(!session.snapshot().tracks[0].notes[1].pitchBend);
        QCOMPARE(session.currentStep(), 1);
        QCOMPARE(session.undoMessage(), PianoRoll::tr("Draw Pitch"));

        // The right button erases with any tool, only where the values are.
        roll.setTool(PianoRoll::SelectTool);
        const auto eraseFrom = QPointF(time.toX(200), roll.view()->keyAxis().toY(65)).toPoint();
        const auto eraseTo = QPointF(time.toX(700), roll.view()->keyAxis().toY(65)).toPoint();
        stroke(roll, Qt::RightButton, eraseFrom, eraseTo);
        for (int k = int(std::ceil(time.toTick(eraseFrom.x()) / 5)); k <= last; ++k) {
            values[k] = 0;
        }
        bend = session.snapshot().tracks[0].notes[0].pitchBend;
        QCOMPARE(bend->values, values);
        QVERIFY(!session.snapshot().tracks[0].notes[1].pitchBend);
        QCOMPARE(session.undoMessage(), PianoRoll::tr("Reset Pitch"));

        // With Mode2 on, the points are back, and the pitch tool selects.
        kit::DiagnosticList diagnostics;
        QVERIFY(
            kit::ProjectEdits::setMode2(kit::ProjectRef(&session).settings(), true, diagnostics));
        const auto point = roll.view()->hitAt(pointOfLi(roll, 0, 100));
        QVERIFY(point && point->part == PianoRoll::PitchPoint);
        roll.setTool(PianoRoll::PitchTool);
        const int step = session.currentStep();
        stroke(roll, Qt::RightButton, eraseFrom, eraseTo);
        QCOMPARE(session.currentStep(), step);
        click(roll, 240, 60);
        QCOMPARE(roll.selectedIndices(), QList<int>{0});
    }

    // A stroke across two notes: the values that the second note gains before its own, beyond
    // the stroke, take the curve of the first note as it was before the stroke, which there
    // still leans towards its last drawn value.
    void a_stroke_fills_from_the_curve_as_it_was() {
        // At 125 bpm a tick is a millisecond. la has values of 0 over its length; li reads
        // its curve from 100 ms before it, and has one value 2 ms after its start.
        kit::Note la;
        la.lyric = QStringLiteral("la");
        la.length = 480;
        la.noteNum = 60;
        la.pitchBend = kit::PitchBend{0.0, QList<double>(97, 0)};
        kit::Note li = la;
        li.lyric = QStringLiteral("li");
        li.noteNum = 62;
        li.preUtterance = 100;
        li.pitchBend = kit::PitchBend{2.0, {0}};
        kit::Project project;
        project.settings.tempo = 125;
        project.settings.mode2 = false;
        project.tracks.push_back({});
        project.tracks[0].notes = {la, li};
        kit::ProjectSession session(project);
        PianoRoll roll(&session);
        show(roll);
        auto time = roll.view()->timeAxis();
        time.left = 0;
        time.pixelsPerTick = 1;
        roll.view()->setTimeAxis(time);
        auto keys = roll.view()->keyAxis();
        keys.top = 70;
        keys.pixelsPerKey = 24;
        roll.view()->setKeyAxis(keys);
        roll.setTool(PianoRoll::PitchTool);

        // From 400 to 421 ticks at 61.5 keys: la takes 100 cents at 400 to 420, li -100 cents
        // at 402 to 417 (-78 to -63 ms from its start).
        stroke(roll, Qt::LeftButton, QPoint(400, 204), QPoint(421, 204));
        const auto notes = session.snapshot().tracks[0].notes;
        auto laValues = la.pitchBend->values;
        for (int k = 80; k <= 84; ++k) {
            laValues[k] = 100;
        }
        QCOMPARE(notes[0].pitchBend->values, laValues);
        // At 422 ticks, la was 0 before the stroke; afterwards it is 60, on its way from 100.
        QList<double> liValues(4, -100);
        liValues.append(QList<double>(13, 0));
        QCOMPARE(notes[1].pitchBend, std::optional(kit::PitchBend{-78.0, liValues}));
        QCOMPARE(session.currentStep(), 1);
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

        // The last point passes the second, at 50 ms, keeping its height, and stays selected;
        // the second, now last, ends at the pitch of the note.
        dragPoint(roll, pointOfLi(roll, 60, 0), pointOfLi(roll, 0, 100));
        points = pointsOfLi(session);
        QCOMPARE(points[1].x, 0.0);
        QCOMPARE(points[1].y, 0.0);
        QCOMPARE(points[2].x, 50.0);
        QCOMPARE(points[2].y, 0.0);
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

    // An edit of the points of a note ends them at its pitch, whatever the file gave the last.
    void an_edit_ends_the_points_at_the_pitch_of_the_note() {
        auto project = bentNotes();
        project.tracks[0].notes[1].portamento[2].y = -20;
        kit::ProjectSession session(project);
        PianoRoll roll(&session);
        showExactly(roll);

        dragPoint(roll, pointOfLi(roll, 0, 100), pointOfLi(roll, 10, 100));
        const auto points = pointsOfLi(session);
        QCOMPARE(points[1].x, 10.0);
        QCOMPARE(points[2].y, 0.0);
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
        QCOMPARE(points[2].y, 0.0); // now the last point
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

    // A rest of 500 ms, then la of 125 ms with 50 ms of pre-utterance and the envelope of UTAU:
    // its fragment starts at 450 ms and lasts 175 ms, its anchors lie at 450, 455, 590 and
    // 625 ms, with a pixel to a millisecond.
    static kit::Project envelopedNote() {
        kit::Note rest;
        rest.lyric = QStringLiteral("R");
        rest.length = 480;
        rest.noteNum = 60;
        kit::Note la;
        la.lyric = QStringLiteral("la");
        la.length = 120;
        la.noteNum = 60;
        la.preUtterance = 50;
        kit::Project project;
        project.settings.tempo = 120;
        project.tracks.push_back({});
        project.tracks[0].notes = {rest, la};
        return project;
    }

    // Where the parameter area draws volume at milliseconds into the track
    static QPoint envelopePoint(const PianoRoll &roll, double milliseconds, double volume) {
        const auto view = roll.parameterView();
        return QPointF(view->timeAxis().toX(milliseconds * 0.96), view->keyAxis().toY(volume))
            .toPoint();
    }

    static kit::Envelope envelopeOfLa(const kit::ProjectSession &session) {
        return session.snapshot().tracks[0].notes[1].envelope.value_or(kit::Envelope());
    }

    // An anchor moves in time between its neighbours, the others staying where they are, and in
    // volume; the right button sets its volume to 100%.
    void an_envelope_is_edited_in_the_parameter_area() {
        const auto edited = [this](const std::function<void(PianoRoll &)> &edit) {
            kit::ProjectSession session(envelopedNote());
            PianoRoll roll(&session);
            showExactly(roll);
            [&] { QVERIFY(roll.parameterView()->timeAxis() == roll.view()->timeAxis()); }();
            edit(roll);
            if (session.canUndo()) {
                [&] {
                    QCOMPARE(session.undoMessage(), kit::ProjectEdits::tr("Change Envelope"));
                }();
            }
            return envelopeOfLa(session).anchorsInTimeOrder();
        };
        const auto drag = [](PianoRoll &roll, QPoint from, QPoint to) {
            const auto viewport = roll.parameterView()->viewport();
            QTest::mousePress(viewport, Qt::LeftButton, {}, from);
            QTest::mouseMove(viewport, (from + to) / 2);
            QTest::mouseMove(viewport, to);
            QTest::mouseRelease(viewport, Qt::LeftButton, {}, to);
        };

        // The end of the attack, 20 ms later and 20% louder; p1 and p3 stay.
        auto anchors = edited([&](PianoRoll &roll) {
            drag(roll, envelopePoint(roll, 455, 100), envelopePoint(roll, 475, 120));
        });
        QCOMPARE(anchors[0].x, 0.0);
        QCOMPARE(anchors[1].x, 25.0);
        QVERIFY(qAbs(anchors[1].y - 120) <= 2);
        QCOMPARE(anchors[2].x, 35.0);

        // The start, 3 ms later: p1 grows and p2 shrinks, the end of the attack staying.
        anchors = edited([&](PianoRoll &roll) {
            drag(roll, envelopePoint(roll, 450, 0), envelopePoint(roll, 453, 0));
        });
        QCOMPARE(anchors[0].x, 3.0);
        QCOMPARE(anchors[1].x, 2.0);

        // Not past the start of the release, at 590 ms
        anchors = edited([&](PianoRoll &roll) {
            drag(roll, envelopePoint(roll, 455, 100), envelopePoint(roll, 700, 100));
        });
        QCOMPARE(anchors[1].x, 140.0);

        // Nor the start of the release before the end of the attack, at 455 ms
        anchors = edited([&](PianoRoll &roll) {
            drag(roll, envelopePoint(roll, 590, 100), envelopePoint(roll, 300, 100));
        });
        QCOMPARE(anchors[2].x, 170.0);

        // The end, 25 ms earlier: p4 grows and p3 shrinks, the start of the release staying.
        anchors = edited([&](PianoRoll &roll) {
            drag(roll, envelopePoint(roll, 625, 0), envelopePoint(roll, 600, 0));
        });
        QCOMPARE(anchors[3].x, 25.0);
        QCOMPARE(anchors[2].x, 10.0);
        QCOMPARE(anchors[3].y, 0.0);

        anchors = edited([&](PianoRoll &roll) {
            QTest::mouseClick(roll.parameterView()->viewport(), Qt::RightButton, {},
                              envelopePoint(roll, 450, 0));
        });
        QCOMPARE(anchors[0].y, 100.0);
    }

    // A double click on the envelope between the attack and the release inserts the middle
    // anchor, and one on the middle anchor removes it.
    void the_middle_anchor_is_inserted_and_removed() {
        kit::ProjectSession session(envelopedNote());
        PianoRoll roll(&session);
        showExactly(roll);
        const auto viewport = roll.parameterView()->viewport();

        QTest::mouseDClick(viewport, Qt::LeftButton, {}, envelopePoint(roll, 520, 100));
        auto envelope = envelopeOfLa(session);
        QVERIFY(envelope.hasMiddle);
        QCOMPARE(envelope.anchors[2].x, 65.0);
        QCOMPARE(envelope.anchors[2].y, 100.0);
        QCOMPARE(envelope.anchors[3].x, 35.0);

        QTest::mouseDClick(viewport, Qt::LeftButton, {}, envelopePoint(roll, 520, 100));
        envelope = envelopeOfLa(session);
        QVERIFY(!envelope.hasMiddle);
        QCOMPARE(envelope.anchors[1].x, 5.0);
    }

    // la, li and lu overlapping by 20, 30 and 10 ms, and li with a middle anchor
    static kit::Project overlappingNotes() {
        kit::Project project;
        project.settings.tempo = 120;
        project.tracks.push_back({});
        const std::tuple<const char *, int, double, double> notes[] = {
            {"la", 60, 50, 20},
            {"li", 62, 60, 30},
            {"lu", 64, 40, 10},
        };
        for (const auto &[lyric, key, preUtterance, overlap] : notes) {
            kit::Note note;
            note.lyric = QString::fromLatin1(lyric);
            note.length = 480;
            note.noteNum = key;
            note.preUtterance = preUtterance;
            note.voiceOverlap = overlap;
            project.tracks[0].notes.push_back(note);
        }
        project.tracks[0].notes[1].envelope = kit::Envelope::fromTimeOrder({
            {0,  0  },
            {5,  100},
            {10, 80 },
            {35, 90 },
            {0,  0  }
        });
        return project;
    }

    // Each envelope fades over the overlaps with sung neighbours and loses its middle anchor;
    // the first note has no overlap before it.
    void envelopes_are_crossfaded_over_the_overlaps() {
        const auto crossfaded = [this](PianoRoll::Crossfade crossfade) {
            kit::ProjectSession session(overlappingNotes());
            PianoRoll roll(&session);
            show(roll);
            roll.setSelectedIndices({0, 1});
            kit::DiagnosticList diagnostics;
            [&] { QVERIFY(roll.crossfadeEnvelopes(crossfade, diagnostics)); }();
            [&] { QCOMPARE(session.undoMessage(), PianoRoll::tr("Crossfade Envelopes")); }();
            const auto notes = session.snapshot().tracks[0].notes;
            return std::pair{notes[0].envelope->anchorsInTimeOrder(),
                             notes[1].envelope->anchorsInTimeOrder()};
        };
        using Anchors = QList<kit::EnvelopeAnchor>;

        const auto [la, li] = crossfaded(PianoRoll::CrossfadeP2P3);
        QCOMPARE(la, (Anchors{
                         {0,  0  },
                         {5,  100},
                         {30, 100},
                         {0,  0  }
        }));
        QCOMPARE(li, (Anchors{
                         {0,  0  },
                         {30, 100},
                         {10, 90 },
                         {0,  0  }
        }));

        const auto [la4, li4] = crossfaded(PianoRoll::CrossfadeP1P4);
        QCOMPARE(la4, (Anchors{
                          {0,  0  },
                          {5,  100},
                          {5,  100},
                          {30, 100}
        }));
        QCOMPARE(li4, (Anchors{
                          {30, 100},
                          {5,  100},
                          {5,  90 },
                          {10, 90 }
        }));
    }

    // la with points, a vibrato and an envelope, then li and lu with none of them, and a rest
    static kit::Project parameterSource() {
        auto project = overlappingNotes();
        auto &notes = project.tracks[0].notes;
        kit::PortamentoPoint first;
        first.x = -40;
        kit::PortamentoPoint last;
        last.x = 20;
        notes[0].portamento = {first, last};
        notes[0].vibrato = VibratoDialog::defaultVibrato();
        notes[0].envelope = kit::Envelope::fromTimeOrder({
            {0,  0  },
            {10, 100},
            {20, 100},
            {5,  0  }
        });
        notes[1].envelope.reset();
        notes[2].envelope = notes[0].envelope;
        kit::Note rest;
        rest.lyric = QStringLiteral("R");
        rest.length = 480;
        rest.noteNum = 60;
        notes.push_back(rest);
        return project;
    }

    // The parameters chosen of one copied note go to every selected note, of several to the
    // selected notes in order; rests take none.
    void parameters_are_copied_and_pasted() {
        kit::ProjectSession session(parameterSource());
        PianoRoll roll(&session);
        show(roll);
        const auto original = session.snapshot().tracks[0].notes;
        const auto source = original[0];

        roll.setSelectedIndices({0});
        QVERIFY(roll.copySelected());
        QCOMPARE(PianoRoll::copiedNotes().size(), 1);
        QCOMPARE(PianoRoll::copiedNotes().first().portamento, source.portamento);

        roll.setSelectedIndices({1, 2, 3});
        kit::DiagnosticList diagnostics;
        QVERIFY(roll.pasteParameters(PianoRoll::PortamentoParameter | PianoRoll::VibratoParameter,
                                     diagnostics));
        auto notes = session.snapshot().tracks[0].notes;
        for (const int i : {1, 2}) {
            QCOMPARE(notes[i].portamento, source.portamento);
            QCOMPARE(notes[i].vibrato, source.vibrato);
            // The envelope was not chosen and stays as it was.
            QCOMPARE(notes[i].envelope, original[i].envelope);
        }
        QVERIFY(notes[3].portamento.isEmpty());
        QCOMPARE(session.undoMessage(), PianoRoll::tr("Paste Parameters"));

        // Two copied notes, la and li, to lu alone: those of la
        session.undo();
        roll.setSelectedIndices({0, 1});
        QVERIFY(roll.copySelected());
        roll.setSelectedIndices({2});
        QVERIFY(roll.pasteParameters(PianoRoll::EnvelopeParameter, diagnostics));
        notes = session.snapshot().tracks[0].notes;
        QCOMPARE(notes[2].envelope, source.envelope);
        QVERIFY(notes[2].portamento.isEmpty());
    }

    // Without a selection a reset applies to every note, with one to the selected notes.
    void parameters_are_reset() {
        kit::ProjectSession session(parameterSource());
        PianoRoll roll(&session);
        show(roll);
        kit::DiagnosticList diagnostics;

        roll.setSelectedIndices({0});
        QVERIFY(roll.resetParameters(PianoRoll::VibratoParameter, diagnostics));
        auto notes = session.snapshot().tracks[0].notes;
        QVERIFY(!notes[0].vibrato);
        QVERIFY(notes[0].envelope);
        QCOMPARE(session.undoMessage(), PianoRoll::tr("Reset Parameters"));
        session.undo();

        roll.setSelectedIndices({});
        QVERIFY(roll.resetParameters(PianoRoll::AllParameters, diagnostics));
        notes = session.snapshot().tracks[0].notes;
        for (const auto &note : notes) {
            QVERIFY(note.portamento.isEmpty());
            QVERIFY(!note.vibrato);
            QVERIFY(!note.envelope);
        }
        session.undo();

        // With points selected, their notes
        roll.setSelectedPoints({
            {0, 1}
        });
        QVERIFY(roll.resetParameters(PianoRoll::EnvelopeParameter, diagnostics));
        notes = session.snapshot().tracks[0].notes;
        QVERIFY(!notes[0].envelope);
        QVERIFY(notes[0].vibrato);
        QCOMPARE(notes[0].portamento.size(), 2);
        QVERIFY(notes[2].envelope);
    }

    // Copied notes are pasted as they are before the first selected note, or after the last
    // note without a selection, in one step, and become the selection.
    void notes_are_pasted_before_the_selection() {
        kit::ProjectSession session(parameterSource());
        PianoRoll roll(&session);
        show(roll);
        const auto source = session.snapshot().tracks[0].notes[0];
        const auto lyrics = [&session] {
            QStringList result;
            const auto project = session.snapshot();
            for (const auto &note : project.tracks[0].notes) {
                result.push_back(note.lyric);
            }
            return result;
        };

        roll.setSelectedIndices({0, 1});
        QVERIFY(roll.copySelected());
        roll.setSelectedIndices({2, 3});
        kit::DiagnosticList diagnostics;
        QVERIFY(roll.pasteNotes(diagnostics));
        QCOMPARE(lyrics(),
                 (QStringList{QStringLiteral("la"), QStringLiteral("li"), QStringLiteral("la"),
                              QStringLiteral("li"), QStringLiteral("lu"), QStringLiteral("R")}));
        QCOMPARE(roll.selectedIndices(), (QList<int>{2, 3}));
        QCOMPARE(session.snapshot().tracks[0].notes[2].portamento, source.portamento);
        QCOMPARE(session.snapshot().tracks[0].notes[2].vibrato, source.vibrato);
        QCOMPARE(session.currentStep(), 1);
        QCOMPARE(session.undoMessage(), PianoRoll::tr("Paste"));

        roll.setSelectedIndices({});
        QVERIFY(roll.pasteNotes(diagnostics));
        QCOMPARE(lyrics().mid(6), (QStringList{QStringLiteral("la"), QStringLiteral("li")}));
        QCOMPARE(roll.selectedIndices(), (QList<int>{6, 7}));
    }

    // Where the parameter area draws value on the line of note index, a little after its start
    static QPoint valuePoint(const PianoRoll &roll, int index, double value) {
        const auto view = roll.parameterView();
        return QPointF(view->timeAxis().toX(double(roll.timeline()->note(index).start)) + 10,
                       view->keyAxis().toY(value))
            .toPoint();
    }

    static QToolButton *laneButton(const PianoRoll &roll, const QString &text) {
        for (const auto button : roll.findChildren<QToolButton *>()) {
            if (button->text() == text) {
                return button;
            }
        }
        return nullptr;
    }

    // A drag of the handle of a note sets the value of the lane shown on it, or on every
    // selected note if it is selected; the right button removes the value.
    void values_are_edited_in_the_parameter_area() {
        kit::ProjectSession session(parameterSource());
        PianoRoll roll(&session);
        show(roll);
        const auto viewport = roll.parameterView()->viewport();
        const auto drag = [viewport](QPoint from, QPoint to) {
            QTest::mousePress(viewport, Qt::LeftButton, {}, from);
            QTest::mouseMove(viewport, (from + to) / 2);
            QTest::mouseMove(viewport, to);
            QTest::mouseRelease(viewport, Qt::LeftButton, {}, to);
        };
        const auto notes = [&session] { return session.snapshot().tracks[0].notes; };
        QCOMPARE(roll.lane(), PianoRoll::EnvelopeLane);

        roll.setLane(PianoRoll::IntensityLane);
        QVERIFY(laneButton(roll, QStringLiteral("Int"))->isChecked());
        // la alone, from the default of 100
        drag(valuePoint(roll, 0, 100), valuePoint(roll, 0, 150));
        QVERIFY(qAbs(*notes()[0].intensity - 150) <= 2);
        QCOMPARE(*notes()[0].intensity, std::round(*notes()[0].intensity));
        QVERIFY(!notes()[1].intensity);
        QCOMPARE(session.undoMessage(), kit::ProjectEdits::tr("Change Intensity"));

        // li and lu, selected, both to the value li is dragged to
        roll.setSelectedIndices({1, 2});
        drag(valuePoint(roll, 1, 100), valuePoint(roll, 1, 50));
        QVERIFY(qAbs(*notes()[1].intensity - 50) <= 2);
        QCOMPARE(notes()[2].intensity, notes()[1].intensity);
        QVERIFY(qAbs(*notes()[0].intensity - 150) <= 2);
        const int step = session.currentStep();

        // The right button on lu removes the values of both.
        const auto lu = valuePoint(roll, 2, *notes()[2].intensity);
        QTest::mousePress(viewport, Qt::RightButton, {}, lu);
        QTest::mouseRelease(viewport, Qt::RightButton, {}, lu);
        QVERIFY(!notes()[1].intensity);
        QVERIFY(!notes()[2].intensity);
        QCOMPARE(session.currentStep(), step + 1);

        // The velocity, chosen with its button, and the modulation, kept within -200
        laneButton(roll, QStringLiteral("Vel"))->click();
        QCOMPARE(roll.lane(), PianoRoll::VelocityLane);
        roll.setSelectedIndices({});
        // Above 100 each key is a value; below it, down to -100, each key is two (QSynthesis).
        // valuePoint() takes the key here.
        drag(valuePoint(roll, 0, 100), valuePoint(roll, 0, 150));
        QVERIFY(qAbs(*notes()[0].velocity - 150) <= 2);
        drag(valuePoint(roll, 0, 150), valuePoint(roll, 0, 40));
        QVERIFY(qAbs(*notes()[0].velocity - -20) <= 4);
        drag(valuePoint(roll, 0, 40), valuePoint(roll, 0, -100));
        QCOMPARE(notes()[0].velocity, std::optional<double>(-100));
        QVERIFY(qAbs(*notes()[0].intensity - 150) <= 2);
        roll.setLane(PianoRoll::ModulationLane);
        // Every modulation is in view.
        const auto &keys = roll.parameterView()->keyAxis();
        QVERIFY(keys.toY(200) >= 0);
        QVERIFY(keys.toY(-200) <= viewport->height());
        QVERIFY(keys.toY(-200) - keys.toY(200) >= viewport->height() * 0.8);
        drag(valuePoint(roll, 0, 100), valuePoint(roll, 0, -500));
        QCOMPARE(notes()[0].modulation, std::optional<double>(-200));
    }

    // The depth of the vibrato of the selected notes is scaled, and the other notes keep theirs.
    void the_pitch_of_the_selection_is_scaled() {
        auto project = parameterSource();
        project.tracks[0].notes[1].vibrato = VibratoDialog::defaultVibrato();
        kit::ProjectSession session(project);
        PianoRoll roll(&session);
        show(roll);

        roll.setSelectedIndices({0, 3});
        kit::DiagnosticList diagnostics;
        QVERIFY(roll.scalePitch(1, 2, diagnostics));
        const auto notes = session.snapshot().tracks[0].notes;
        const double depth = VibratoDialog::defaultVibrato().amplitude;
        QCOMPARE(notes[0].vibrato->amplitude, depth * 2);
        QCOMPARE(notes[1].vibrato->amplitude, depth);
        QCOMPARE(session.undoMessage(), kit::ProjectEdits::tr("Scale Pitch"));
    }

    // The parameter area and the roll scroll together.
    void the_parameter_area_follows_the_roll() {
        kit::ProjectSession session(envelopedNote());
        PianoRoll roll(&session);
        show(roll);
        auto time = roll.view()->timeAxis();
        time.left = 240;
        roll.view()->setTimeAxis(time);
        QCOMPARE(roll.parameterView()->timeAxis().left, 240.0);
        time.left = 120;
        roll.parameterView()->setTimeAxis(time);
        QCOMPARE(roll.view()->timeAxis().left, 120.0);
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
