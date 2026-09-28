// The expected samples are those wavtool.exe wrote for the same calls, as recorded in
// docs/claude/wavtool-concatenation.md.

#include <QtTest/QTest>

#include <hellokit/Synth/WavtoolMixer.h>

using namespace hello::kit;

namespace {

    // A call as SynthPlan writes it, with the file names first
    WavtoolCall call(const QString &text) {
        auto arguments = QStringList{QStringLiteral("out.wav"), QStringLiteral("in.wav")};
        arguments += text.split(QLatin1Char(' '));
        const auto parsed = WavtoolCall::parse(arguments);
        return parsed.value_or(WavtoolCall());
    }

    const QString flat = QStringLiteral("0 0 0 100 100 100 100 0");

    // Mixes the whole track of \a calls, each with its fragment.
    std::vector<qint16> mixed(const QList<WavtoolCall> &calls,
                              const QList<const std::vector<qint16> *> &fragments) {
        const auto segments = WavtoolMixer::layOut(calls);
        std::vector<qint16> out(size_t(WavtoolMixer::lengthOf(segments)));
        WavtoolMixer::mix(
            segments, [&](int index) { return fragments.value(index); }, 0, qint64(out.size()),
            out.data());
        return out;
    }

    const std::vector<qint16> dc(44100, 10000);
    const std::vector<qint16> dc2(44100, 20000);
    const std::vector<qint16> negative(44100, -10000);

    std::vector<qint16> ramp() {
        std::vector<qint16> samples(44100);
        for (int i = 0; i < 44100; ++i) {
            samples[size_t(i)] = qint16(std::min(i, 30000));
        }
        return samples;
    }

}

class test_WavtoolMixer : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void the_arguments_of_stdutau_are_read() {
        const auto parsed = WavtoolCall::parse(
            {QStringLiteral("out.wav"), QStringLiteral("in.wav"), QStringLiteral("12.500000"),
             QStringLiteral("480@120.000000+50.100000"), QStringLiteral("0.000000"),
             QStringLiteral("5.000000"), QStringLiteral("35.000000"), QStringLiteral("0.000000"),
             QStringLiteral("100.000000"), QStringLiteral("100.000000"), QStringLiteral("0.000000"),
             QStringLiteral("15.000000"), QStringLiteral("10.000000"), QStringLiteral("20.000000"),
             QStringLiteral("80.000000")});
        QVERIFY(parsed);
        QCOMPARE(parsed->startPoint, 12.5);
        QCOMPARE(parsed->length, 550.1);
        QVERIFY(parsed->hasEnvelope);
        QCOMPARE(parsed->p2, 5.0);
        QCOMPARE(parsed->overlap, 15.0);
        QCOMPARE(parsed->p4, 10.0);
        QCOMPARE(parsed->p5, std::optional<double>(20));
        QCOMPARE(parsed->v5, std::optional<double>(80));

        // A rest has no envelope; a negative correction shortens the call.
        const auto rest = call(QStringLiteral("0 480@120-41 0 0"));
        QVERIFY(!rest.hasEnvelope);
        QCOMPARE(rest.length, 459.0);

        QVERIFY(!WavtoolCall::parse({QStringLiteral("out.wav"), QStringLiteral("in.wav"),
                                     QStringLiteral("0"), QStringLiteral("480")}));
        QVERIFY(!WavtoolCall::parse({QStringLiteral("out.wav")}));
    }

    // The offset skips the start of the fragment, and the length is rounded to whole samples.
    void offset_and_length_cut_the_fragment() {
        const auto samples = ramp();
        auto out = mixed({call(QStringLiteral("10.02 480@120+0 ") + flat)}, {&samples});
        QCOMPARE(out.size(), size_t(22050));
        QCOMPARE(out.front(), qint16(442));
        QCOMPARE(out.back(), qint16(22491));

        QCOMPARE(mixed({call(QStringLiteral("0 480@120+0.02 ") + flat)}, {&samples}).size(),
                 size_t(22051));
        QCOMPARE(mixed({call(QStringLiteral("0 479@120+0 ") + flat)}, {&samples}).size(),
                 size_t(22004));

        // A short fragment is followed by silence.
        const std::vector<qint16> shortFragment(4410, 10000);
        out = mixed({call(QStringLiteral("0 480@120+0 ") + flat)}, {&shortFragment});
        QCOMPARE(out[4409], qint16(10000));
        QCOMPARE(out[4410], qint16(0));
    }

    void the_envelope_as_wavtool_applies_it() {
        auto out = mixed({call(QStringLiteral("0 480@120+0 10 20 30 0 50 100 25 0"))}, {&dc});
        QCOMPARE(out[441], qint16(0));
        QCOMPARE(out[442], qint16(5));
        QCOMPARE(out[1323], qint16(5000));
        QCOMPARE(out[20727], qint16(10000));
        QCOMPARE(out[20728], qint16(9994));
        QCOMPARE(out[22049], qint16(2505));

        // Negative samples are truncated toward zero as well.
        out = mixed({call(QStringLiteral("0 480@120+0 10 20 30 0 50 100 25 0"))}, {&negative});
        QCOMPARE(out[442], qint16(-5));
        QCOMPARE(out[22049], qint16(-2505));

        // The middle point, p4 before the end, and silence after it
        out = mixed({call(QStringLiteral("0 480@120+0 10 20 30 0 50 100 25 0 40 5 80"))}, {&dc});
        QCOMPARE(out[1323], qint16(5000));
        QCOMPARE(out[1544], qint16(8000));
        QCOMPARE(out[18963], qint16(10000));
        QCOMPARE(out[20286], qint16(2500));
        QCOMPARE(out[22049], qint16(1));

        // Before the first point, the volume rises from zero.
        out = mixed({call(QStringLiteral("0 480@120+0 10 20 30 100 50 100 100 0"))}, {&dc});
        QCOMPARE(out[0], qint16(0));
        QCOMPARE(out[441], qint16(10000));
        QCOMPARE(out[1323], qint16(5000));

        // A fractional position rounds to the nearest sample.
        out = mixed({call(QStringLiteral("0 480@120+0 10.02 20 30 0 100 100 100 0"))}, {&dc});
        QCOMPARE(out[442], qint16(0));
        QCOMPARE(out[443], qint16(11));

        // The envelope follows the segment, not the fragment.
        out = mixed({call(QStringLiteral("30 480@120+0 10 20 30 0 50 100 25 0"))}, {&dc});
        QCOMPARE(out[442], qint16(5));
    }

    // The next call starts its overlap before the end, and the two are added, then limited.
    void overlaps_add_up() {
        auto out = mixed({call(QStringLiteral("0 480@120+0 ") + flat),
                          call(QStringLiteral("0 480@120+0 0 0 0 100 100 100 100 100"))},
                         {&dc, &dc2});
        QCOMPARE(out.size(), size_t(39690));
        QCOMPARE(out[17639], qint16(10000));
        QCOMPARE(out[17640], qint16(30000));
        QCOMPARE(out[22050], qint16(20000));

        // The correction lengthens the first call, and the overlap counts from its end.
        out = mixed({call(QStringLiteral("0 480@120+100 ") + flat),
                     call(QStringLiteral("0 480@120+0 0 0 0 100 100 100 100 100"))},
                    {&dc, &dc2});
        QCOMPARE(out.size(), size_t(44100));
        QCOMPARE(out[22050], qint16(30000));
        QCOMPARE(out[26460], qint16(20000));

        out = mixed({call(QStringLiteral("0 480@120+0 ") + flat),
                     call(QStringLiteral("0 480@120+0 0 0 0 100 100 100 100 250"))},
                    {&dc2, &dc2});
        QCOMPARE(out[11025], qint16(32767));

        // The first overlap does nothing, and a fractional one rounds.
        out = mixed({call(QStringLiteral("0 480@120+0 10 20 30 0 50 100 25 15"))}, {&dc});
        QCOMPARE(out.size(), size_t(22050));
        out = mixed({call(QStringLiteral("0 480@120+0 ") + flat),
                     call(QStringLiteral("0 480@120+0 0 0 0 100 100 100 100 10.01"))},
                    {&dc, &dc});
        QCOMPARE(out[21608], qint16(10000));
        QCOMPARE(out[21609], qint16(20000));
    }

    void a_rest_is_silent() {
        const auto out = mixed(
            {call(QStringLiteral("0 480@120+0 ") + flat), call(QStringLiteral("0 480@120+0 0 0"))},
            {&dc, &dc});
        QCOMPARE(out.size(), size_t(44100));
        QCOMPARE(out[22049], qint16(10000));
        QCOMPARE(out[22050], qint16(0));
    }

    // The length and the points are rounded on the fragment, after the offset.
    void the_offset_moves_where_rounding_happens() {
        const auto first = call(QStringLiteral("0 480@120+0 ") + flat);
        auto out = mixed({first, call(QStringLiteral("30.4478 720@134+89.5522 0 5 35 0 100 100 0 "
                                                     "-22.3881"))},
                         {&dc, &dc});
        QCOMPARE(out.size(), size_t(56604));
        // The attack of 5 ms ends at 220 samples after the offset of 30.4478 ms, at 221 after
        // one of 30 ms.
        QCOMPARE(out[23255] < 10000, true);
        QCOMPARE(out[23256], qint16(10000));
        out = mixed({first, call(QStringLiteral("30 720@134+89.5522 0 5 35 0 100 100 0 "
                                                "-22.3881"))},
                    {&dc, &dc});
        QCOMPARE(out.size(), size_t(56605));
        QCOMPARE(out[23256] < 10000, true);
        QCOMPARE(out[23257], qint16(10000));
        out = mixed({first, call(QStringLiteral("30.4478 720@134+89.5522 0 5 35 0 100 100 0 "
                                                "22.3881"))},
                    {&dc, &dc});
        QCOMPARE(out.size(), size_t(54631));
    }

    // A negative overlap leaves a gap one sample shorter than its length.
    void a_negative_overlap_leaves_a_gap() {
        const auto first = call(QStringLiteral("0 480@120+0 ") + flat);
        auto out = mixed({first, call(QStringLiteral("0 480@120+0 10 20 30 0 50 100 25 -30"))},
                         {&dc, &dc});
        QCOMPARE(out.size(), size_t(45422));
        QCOMPARE(out[23813], qint16(0));
        QCOMPARE(out[23814], qint16(5));
        QCOMPARE(out[24695], qint16(5000));
        QCOMPARE(out[45421], qint16(2505));

        out = mixed({first, call(QStringLiteral("0 480@120+0 10 20 30 0 50 100 25 -10.99"))},
                    {&dc, &dc});
        QCOMPARE(out.size(), size_t(44584));
        QCOMPARE(out[22975], qint16(0));
        QCOMPARE(out[22976], qint16(5));

        out = mixed({first, call(QStringLiteral("0 480@120+0 0 0 0 100 100 100 100 -0.01"))},
                    {&dc, &dc});
        QCOMPARE(out.size(), size_t(44100));
    }

    // Each point is converted from its own time, and the last two count back from the exact
    // length.
    void points_are_placed_from_their_times() {
        auto out =
            mixed({call(QStringLiteral("0 480@120+0 10.01 10.01 0 0 100 100 100 0"))}, {&dc});
        QCOMPARE(out[441], qint16(0));
        QCOMPARE(out[442], qint16(22));
        QCOMPARE(out[882], qint16(9977));
        QCOMPARE(out[883], qint16(10000));

        out = mixed({call(QStringLiteral("0 480@120+0 0 0 10.01 100 100 100 0 0 10.01"))}, {&dc});
        QCOMPARE(out[21167], qint16(10000));
        QCOMPARE(out[21168], qint16(9977));
        QCOMPARE(out[21609], qint16(0));

        out = mixed({call(QStringLiteral("0 480@120+0.012 0 0 10 100 100 100 0 0"))}, {&dc});
        QCOMPARE(out.size(), size_t(22051));
        QCOMPARE(out[21610], qint16(10000));
        QCOMPARE(out[21611], qint16(9977));
    }

    // The sum is truncated, not each contribution: a fade out and a fade in of the same level
    // meet without a dip, as in two notes of a real project.
    void a_crossfade_sums_before_truncating() {
        const std::vector<qint16> level(88200, -11000);
        const auto out = mixed({call(QStringLiteral("0 480@134+44 0 5 75 0 100 100 0 44")),
                                call(QStringLiteral("0 300@134+63 0 75 53 0 100 100 0 75"))},
                               {&level, &level});
        QCOMPARE(out.size(), size_t(33499));
        for (int i = 18380; i < 18410; ++i) {
            QCOMPARE(out[size_t(i)], qint16(-10999));
        }
        // A fade in over a sample of the other sign: -4000 + 22.68 is truncated to -3977, where
        // truncating the contribution alone would give -3978.
        const std::vector<qint16> below(44100, -4000);
        const auto signs = mixed({call(QStringLiteral("0 480@120+0 ") + flat),
                                  call(QStringLiteral("0 480@120+0 0 10 0 0 100 100 100 100"))},
                                 {&below, &dc});
        QCOMPARE(signs[17640], qint16(-4000));
        QCOMPARE(signs[17641], qint16(-3977));
        QCOMPARE(signs[17644], qint16(-3909));

        // The release of the second note starts from its exact length of 15119.72 samples
        // less 53 ms, not from the rounded 15120.
        QCOMPARE(out[31181], qint16(-10905));
        QCOMPARE(out[31191], qint16(-10858));
    }

    // A part of the track equals the same part of the whole.
    void any_part_mixes_alone() {
        const QList<WavtoolCall> calls{
            call(QStringLiteral("0 480@120+0 10 20 30 0 50 100 25 0")),
            call(QStringLiteral("0 480@120+0 10 20 30 0 50 100 25 100"))};
        const auto whole = mixed(calls, {&dc, &dc2});
        const auto segments = WavtoolMixer::layOut(calls);
        std::vector<qint16> part(1000);
        WavtoolMixer::mix(
            segments, [&](int index) { return index == 0 ? &dc : &dc2; }, 17000, 1000, part.data());
        QCOMPARE(part, std::vector<qint16>(whole.begin() + 17000, whole.begin() + 18000));
    }
};

QTEST_APPLESS_MAIN(test_WavtoolMixer)

#include "test_WavtoolMixer.moc"
