#include <cmath>

#include <QtCore/QJsonObject>
#include <QtTest/QTest>

#include <hellokit/Document/PortamentoSettings.h>

using namespace hello::kit;

namespace {

    using Points = QList<PortamentoPoint>;

    bool near(double actual, double expected) {
        return std::abs(actual - expected) < 1e-9;
    }

    // Returns whether the points have the positions and the heights of \a expected, and its types.
    bool samePoints(const Points &actual, const Points &expected) {
        if (actual.size() != expected.size()) {
            return false;
        }
        for (qsizetype i = 0; i < actual.size(); ++i) {
            if (!near(actual[i].x, expected[i].x) || !near(actual[i].y, expected[i].y) ||
                actual[i].type != expected[i].type) {
                return false;
            }
        }
        return true;
    }

    PortamentoSettings addPoints(int count, bool evenly) {
        PortamentoSettings settings;
        settings.mode = PortamentoSettings::AddPoints;
        settings.count = count;
        settings.evenlyDistributed = evenly;
        return settings;
    }

}

class test_PortamentoSettings : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void a_preset_lies_around_the_start_of_the_note() {
        PortamentoSettings settings;
        const auto pointsOf = [&](PortamentoSettings::Position position, int length) {
            settings.position = position;
            settings.presetLength = length;
            return settings.pointsFor(
                {
                    {0, -100, PortamentoPoint::Linear}
            },
                480);
        };
        constexpr auto s = PortamentoPoint::S;
        QVERIFY(samePoints(pointsOf(PortamentoSettings::Center, 50), {
                                                                         {-50, 0, s},
                                                                         {50,  0, s}
        }));
        QVERIFY(samePoints(pointsOf(PortamentoSettings::Left, 100), {
                                                                        {-100, 0, s},
                                                                        {0,    0, s}
        }));
        QVERIFY(samePoints(pointsOf(PortamentoSettings::Right, 200), {
                                                                         {0,   0, s},
                                                                         {200, 0, s}
        }));
    }

    // Custom keeps the heights and the types of the two current points, and starts from the
    // height of the note otherwise.
    void custom_points_keep_the_heights_of_two_current_points() {
        PortamentoSettings settings;
        settings.mode = PortamentoSettings::Custom;
        settings.start = -30;
        settings.length = 59;
        const Points two{
            {-10, -20, PortamentoPoint::R},
            {40,  0,   PortamentoPoint::J}
        };
        QVERIFY(samePoints(settings.pointsFor(two, 480),
                           {
                               {-30, -20, PortamentoPoint::R},
                               {29,  0,   PortamentoPoint::J}
        }));
        QVERIFY(samePoints(settings.pointsFor(
                               {
        },
                               480),
                           {{-30, 0, PortamentoPoint::S}, {29, 0, PortamentoPoint::S}}));
    }

    // Spread evenly from the first point to the end of the note, as in UTAU. A new point lies on
    // the current curve and takes the type of its segment.
    void evenly_added_points_lie_on_the_current_curve() {
        const Points current{
            {0,   0,   PortamentoPoint::S},
            {100, 100, PortamentoPoint::R},
            {200, 0,   PortamentoPoint::J}
        };
        const auto points = addPoints(5, true).pointsFor(current, 200);
        QCOMPARE(points.size(), 5);
        const double xs[] = {0, 50, 100, 150, 200};
        for (int i = 0; i < 5; ++i) {
            QVERIFY(near(points[i].x, xs[i]));
            QVERIFY(near(points[i].y, PortamentoPoint::heightAt(current, xs[i])));
        }
        QCOMPARE(points[1].type, PortamentoPoint::R);
        QCOMPARE(points[3].type, PortamentoPoint::J);
        // A point at the position of a current point takes the type of that point.
        QCOMPARE(points[2].type, PortamentoPoint::R);
        QCOMPARE(points[4].type, PortamentoPoint::J);

        // To the end of the note, past the last current point
        const auto longer = addPoints(3, true).pointsFor(current, 400);
        QVERIFY(near(longer.last().x, 400));
    }

    // Without even distribution, more points are appended after the last point at 25 ms, closer
    // if the end of the note is nearer.
    void more_points_are_appended_after_the_last_point() {
        const Points current{
            {0,  -100, PortamentoPoint::S},
            {50, 0,    PortamentoPoint::S}
        };
        constexpr auto s = PortamentoPoint::S;
        QVERIFY(samePoints(addPoints(4, false).pointsFor(current, 200),
                           {
                               {0,   -100, s},
                               {50,  0,    s},
                               {75,  0,    s},
                               {100, 0,    s}
        }));
        QVERIFY(samePoints(addPoints(4, false).pointsFor(current, 70),
                           {
                               {0,  -100, s},
                               {50, 0,    s},
                               {60, 0,    s},
                               {70, 0,    s}
        }));
    }

    // Without room after the last point, the longest segment is split in its middle. The halves
    // of an S curve are a J curve and an R curve, so that the curve does not change.
    void without_room_the_longest_segment_is_split() {
        const Points current{
            {0,  -100, PortamentoPoint::S},
            {20, -100, PortamentoPoint::S},
            {60, 0,    PortamentoPoint::S}
        };
        const auto points = addPoints(4, false).pointsFor(current, 60);
        QVERIFY(samePoints(points, {
                                       {0,  -100, PortamentoPoint::S},
                                       {20, -100, PortamentoPoint::S},
                                       {40, -50,  PortamentoPoint::J},
                                       {60, 0,    PortamentoPoint::R}
        }));
        for (const double x : {25.0, 33.0, 40.0, 47.0, 55.0}) {
            QVERIFY2(
                near(PortamentoPoint::heightAt(points, x), PortamentoPoint::heightAt(current, x)),
                qPrintable(QString::number(x)));
        }
    }

    void fewer_points_are_spread_over_the_current_range() {
        const Points current{
            {0,  0,  PortamentoPoint::S},
            {30, 10, PortamentoPoint::S},
            {60, 20, PortamentoPoint::S},
            {90, 0,  PortamentoPoint::S}
        };
        const auto points = addPoints(2, false).pointsFor(current, 480);
        QCOMPARE(points.size(), 2);
        QVERIFY(near(points[0].x, 0));
        QVERIFY(near(points[1].x, 90));
    }

    void without_current_points_the_points_are_spread_over_the_preset() {
        constexpr auto s = PortamentoPoint::S;
        QVERIFY(samePoints(addPoints(3, false).pointsFor(
                               {
        },
                               480),
                           {{-50, 0, s}, {0, 0, s}, {50, 0, s}}));
    }

    void the_settings_survive_a_round_trip() {
        PortamentoSettings settings;
        settings.mode = PortamentoSettings::AddPoints;
        settings.position = PortamentoSettings::Left;
        settings.presetLength = 200;
        settings.start = -45;
        settings.length = 80;
        settings.count = 5;
        settings.evenlyDistributed = false;
        QCOMPARE(PortamentoSettings::fromJson(settings.toJson()), settings);
        QCOMPARE(PortamentoSettings::fromJson({}), PortamentoSettings());
    }

    void a_field_that_is_not_valid_keeps_its_default() {
        const QJsonObject object{
            {QStringLiteral("mode"),         QStringLiteral("bogus")},
            {QStringLiteral("position"),     7                      },
            {QStringLiteral("presetLength"), 75                     },
            {QStringLiteral("start"),        QStringLiteral("x")    },
            {QStringLiteral("length"),       -5                     },
            {QStringLiteral("count"),        1                      },
        };
        QCOMPARE(PortamentoSettings::fromJson(object), PortamentoSettings());
    }
};

QTEST_APPLESS_MAIN(test_PortamentoSettings)

#include "test_PortamentoSettings.moc"
