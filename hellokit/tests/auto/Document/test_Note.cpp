#include <QtCore/QList>
#include <QtTest/QTest>

#include <hellokit/Document/Note.h>

using namespace hello::kit;

class test_Note : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Without the middle anchor, the four anchors in time order occupy indices 0, 1, 3 and 4, so
    // that each index keeps its role.
    void four_anchors_keep_their_roles() {
        const QList<EnvelopeAnchor> timeOrder{
            {0,  0  },
            {5,  100},
            {35, 90 },
            {10, 0  }
        };
        const auto envelope = Envelope::fromTimeOrder(timeOrder);
        QVERIFY(envelope.has_value());
        QVERIFY(!envelope->hasMiddle);
        QVERIFY(envelope->anchors[1] == timeOrder[1]);
        QVERIFY(envelope->anchors[3] == timeOrder[2]);
        QVERIFY(envelope->anchors[4] == timeOrder[3]);
        QVERIFY(envelope->anchorsInTimeOrder() == timeOrder);
    }

    void five_anchors_place_the_middle_one_at_index_two() {
        const QList<EnvelopeAnchor> timeOrder{
            {0,  0  },
            {5,  100},
            {20, 80 },
            {35, 90 },
            {10, 0  }
        };
        const auto envelope = Envelope::fromTimeOrder(timeOrder);
        QVERIFY(envelope.has_value());
        QVERIFY(envelope->hasMiddle);
        QVERIFY(envelope->anchors[2] == timeOrder[2]);
        QVERIFY(envelope->anchors[4] == timeOrder[4]);
        QVERIFY(envelope->anchorsInTimeOrder() == timeOrder);
    }

    void other_numbers_of_anchors_are_refused() {
        QVERIFY(!Envelope::fromTimeOrder({}).has_value());
        QVERIFY(!Envelope::fromTimeOrder({
                                             {0, 0  },
                                             {5, 100},
                                             {0, 0  }
        })
                     .has_value());
        QVERIFY(!Envelope::fromTimeOrder(QList<EnvelopeAnchor>(6)).has_value());
    }

    // The unused middle anchor is not part of the envelope, so it does not affect equality.
    void an_unused_middle_anchor_is_not_compared() {
        Envelope first;
        Envelope second;
        second.anchors[2] = {20, 80};
        QVERIFY(first == second);

        first.hasMiddle = true;
        second.hasMiddle = true;
        QVERIFY(first != second);
    }

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

    // The editing layer stores a vibrato as one value and creates no action if the new value is
    // equal to the old one, so every parameter must take part in the comparison.
    void vibratos_differing_in_any_parameter_are_unequal() {
        const Vibrato base{65, 180, 35, 20, 20, 0, 0, 0};
        QVERIFY(base == Vibrato(base));

        for (int i = 0; i < 8; ++i) {
            auto other = base;
            double *parameters[] = {&other.length, &other.period,   &other.amplitude,
                                    &other.attack, &other.release,  &other.phase,
                                    &other.offset, &other.intensity};
            *parameters[i] += 1;
            QVERIFY2(base != other, qPrintable(QString::number(i)));
        }
    }
};

QTEST_APPLESS_MAIN(test_Note)

#include "test_Note.moc"
