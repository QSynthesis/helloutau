#include <QtTest/QTest>

#include <hellokit/Document/Vibrato.h>

using namespace hello::kit;

class test_Vibrato : public QObject {
    Q_OBJECT

private Q_SLOTS:
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

    void the_default_is_that_of_utau() {
        const auto vibrato = Vibrato::utauDefault();
        QCOMPARE(vibrato.length, 65.0);
        QCOMPARE(vibrato.period, 180.0);
        QCOMPARE(vibrato.amplitude, 35.0);
        QCOMPARE(vibrato.attack, 20.0);
        QCOMPARE(vibrato.release, 20.0);
        QCOMPARE(vibrato.phase, 0.0);
        QCOMPARE(vibrato.offset, 0.0);
    }
};

QTEST_APPLESS_MAIN(test_Vibrato)

#include "test_Vibrato.moc"
