#include <QtTest/QTest>

#include <hellokit/Document/PitchBend.h>

using namespace hello::kit;

class test_PitchBend : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The editing layer replaces a pitch curve only if the new curve differs from the current
    // one, so both the start and every value take part in the comparison.
    void pitch_curves_differing_in_the_start_or_a_value_are_unequal() {
        PitchBend base;
        base.start = -20.0;
        base.values = {0, 10.5, -20};
        QVERIFY(base == PitchBend(base));

        auto withoutStart = base;
        withoutStart.start = std::nullopt;
        QVERIFY(base != withoutStart);

        auto otherValue = base;
        otherValue.values[2] = -21;
        QVERIFY(base != otherValue);

        auto shorter = base;
        shorter.values.removeLast();
        QVERIFY(base != shorter);
    }

    // An absent start differs from a start of zero.
    void a_pitch_curve_without_a_start_reads_back_without_one() {
        const PitchBend bend{
            std::nullopt, {1, 2}
        };
        QVERIFY(!bend.toJson().contains(QStringLiteral("start")));
        QVERIFY(PitchBend::fromJson(bend.toJson()) == bend);
    }
};

QTEST_APPLESS_MAIN(test_PitchBend)

#include "test_PitchBend.moc"
