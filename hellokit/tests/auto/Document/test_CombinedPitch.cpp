#include <QtTest/QTest>

#include <hellokit/Document/CombinedPitch.h>
#include <hellokit/Document/Note.h>

using namespace hello::kit;

namespace {

    // At 120 BPM, 480 ticks last 500 milliseconds.
    constexpr double tempo = 120;

    Note noteOf(int noteNum, const QList<PortamentoPoint> &points = {}) {
        Note note;
        note.lyric = QStringLiteral("a");
        note.length = 480;
        note.noteNum = noteNum;
        note.portamento = points;
        return note;
    }

    PortamentoPoint pointOf(double x, double y, PortamentoPoint::Type type = PortamentoPoint::S) {
        PortamentoPoint point;
        point.x = x;
        point.y = y;
        point.type = type;
        return point;
    }

    bool ascending(const QList<PortamentoPoint> &points) {
        for (qsizetype i = 1; i < points.size(); ++i) {
            if (points[i].x < points[i - 1].x) {
                return false;
            }
        }
        return true;
    }

}

class test_CombinedPitch : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The points of the second note follow those of the first, moved by its start and its
    // pitch. Its first point starts at the pitch of the first note, and the curve stays at its
    // pitch to the end of the combined note.
    void the_points_of_later_notes_follow() {
        const auto pitch = CombinedPitch::of({noteOf(60, {pointOf(-25, -100), pointOf(25, 0)}),
                                              noteOf(62, {pointOf(-25, 50), pointOf(25, 0)})},
                                             tempo);
        const QList<PortamentoPoint> expected{pointOf(-25, -100), pointOf(25, 0), pointOf(475, 0),
                                              pointOf(525, 200), pointOf(1000, 200)};
        QCOMPARE(pitch.portamento, expected);
    }

    // A curve that reaches past the first point of the next note ends there, at the height it
    // has, and no interval is negative. A curve that ends above its note returns to its pitch
    // after its last point.
    void the_curves_end_where_the_next_note_takes_over() {
        const auto cut = CombinedPitch::of(
            {noteOf(60, {pointOf(0, 0), pointOf(600, 120, PortamentoPoint::Linear)}),
             noteOf(60, {pointOf(-25, 0), pointOf(25, 0)})},
            tempo);
        QVERIFY(ascending(cut.portamento));
        QCOMPARE(cut.portamento.size(), 4);
        QCOMPARE(cut.portamento[1].x, 475.0);
        QCOMPARE(cut.portamento[1].y, 95.0);
        QCOMPARE(cut.portamento[1].type, PortamentoPoint::Linear);
        QCOMPARE(cut.portamento[2], pointOf(475, 0));
        QCOMPARE(cut.portamento[3], pointOf(525, 0));

        const auto held = CombinedPitch::of({noteOf(60, {pointOf(0, 0), pointOf(100, 50)}),
                                             noteOf(60, {pointOf(-25, 0), pointOf(25, 30)})},
                                            tempo);
        const QList<PortamentoPoint> expected{pointOf(0, 0),   pointOf(100, 50), pointOf(100, 0),
                                              pointOf(475, 0), pointOf(525, 30), pointOf(525, 0)};
        QCOMPARE(held.portamento, expected);

        // Negative intervals in the file become empty.
        const auto backwards =
            CombinedPitch::of({noteOf(60, {pointOf(0, 0), pointOf(-10, 20)}), noteOf(60)}, tempo);
        QVERIFY(ascending(backwards.portamento));
    }

    // The curve of a note without points bends at its start, and notes of one pitch without
    // points combine into a note without points. A point between two of the same height is
    // dropped.
    void notes_without_points_bend_at_their_starts() {
        QVERIFY(
            CombinedPitch::of({noteOf(60), noteOf(60), noteOf(60)}, tempo).portamento.isEmpty());
        const auto pitch = CombinedPitch::of({noteOf(60), noteOf(62)}, tempo);
        const QList<PortamentoPoint> expected{pointOf(0, 0), pointOf(500, 0), pointOf(500, 200),
                                              pointOf(1000, 200)};
        QCOMPARE(pitch.portamento, expected);
    }

    // After a rest, the first point of a note has its own height.
    void a_note_after_a_rest_keeps_its_first_point() {
        auto rest = noteOf(60, {pointOf(0, 0), pointOf(10, 0)});
        rest.lyric = QStringLiteral("R");
        const auto pitch =
            CombinedPitch::of({rest, noteOf(62, {pointOf(-25, -50), pointOf(25, 0)})}, tempo);
        QVERIFY(pitch.portamento.contains(pointOf(475, 150)));
    }

    // The Mode1 values are those of each note, every five ticks from the start of the first,
    // with the pitch of the later notes added. Notes of one pitch without values combine into a
    // note without values.
    void the_mode1_values_follow_the_notes() {
        QVERIFY(!CombinedPitch::of({noteOf(60), noteOf(60)}, tempo).pitchBend);

        auto first = noteOf(60);
        first.pitchBend = PitchBend{
            0.0, {0, 10, 20}
        };
        auto second = noteOf(62);
        second.pitchBend = PitchBend{0.0, {30}};
        const auto pitch = CombinedPitch::of({first, second}, tempo);
        QVERIFY(pitch.pitchBend);
        QCOMPARE(pitch.pitchBend->start, std::optional<double>(0));
        const auto &values = pitch.pitchBend->values;
        QCOMPARE(values.size(), 193);
        QCOMPARE(values.mid(0, 3), (QList<double>{0, 10, 20}));
        QCOMPARE(values[50], 0.0);
        QCOMPARE(values[96], 230.0);
        QCOMPARE(values[150], 200.0);

        // A difference of pitch alone results in values.
        const auto stepped = CombinedPitch::of({noteOf(60), noteOf(59)}, tempo);
        QVERIFY(stepped.pitchBend);
        QCOMPARE(stepped.pitchBend->values.value(0), 0.0);
        QCOMPARE(stepped.pitchBend->values.value(100), -100.0);
    }
};

QTEST_GUILESS_MAIN(test_CombinedPitch)

#include "test_CombinedPitch.moc"
