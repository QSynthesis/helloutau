#include <cmath>

#include <QtCore/QJsonObject>
#include <QtCore/QtMath>
#include <QtTest/QTest>

#include <hellokit/Document/PortamentoPoint.h>

using namespace hello::kit;

class test_PortamentoPoint : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void each_curve_type_has_a_name_that_reads_back() {
        for (const auto type : {PortamentoPoint::S, PortamentoPoint::Linear, PortamentoPoint::R,
                                PortamentoPoint::J}) {
            QCOMPARE(PortamentoPoint::typeFromName(PortamentoPoint::typeName(type)), type);
        }
        QCOMPARE(PortamentoPoint::typeName(PortamentoPoint::Linear), QStringLiteral("Linear"));
    }

    // The letters of the PBM entry of UST are not names of .usth.
    void a_letter_of_ust_is_not_a_curve_type_name() {
        QVERIFY(!PortamentoPoint::typeFromName(u"s").has_value());
        QVERIFY(!PortamentoPoint::typeFromName(u"").has_value());
    }

    void an_unknown_curve_type_is_reported_and_read_as_s() {
        const QJsonObject object{
            {QStringLiteral("x"),    10                   },
            {QStringLiteral("y"),    5                    },
            {QStringLiteral("type"), QStringLiteral("zig")},
        };
        DiagnosticList diagnostics;
        const auto point = PortamentoPoint::fromJson(object, diagnostics);
        QCOMPARE(point.x, 10.0);
        QCOMPARE(point.y, 5.0);
        QCOMPARE(point.type, PortamentoPoint::S);
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(diagnostics.at(0).severity, DiagnosticSeverity::Warning);
    }

    // Each segment is drawn in the type of the point that ends it, with the shapes of
    // PitchCurve, and the value is not truncated to whole cents.
    void the_height_follows_the_shape_of_each_segment() {
        const auto heightAt = [](PortamentoPoint::Type type, double x) {
            return PortamentoPoint::heightAt(
                {
                    {0,   0,   PortamentoPoint::S},
                    {100, 100, type              }
            },
                x);
        };
        const auto near = [](double actual, double expected) {
            return std::abs(actual - expected) < 1e-9;
        };
        for (const auto type : {PortamentoPoint::S, PortamentoPoint::Linear, PortamentoPoint::R,
                                PortamentoPoint::J}) {
            QVERIFY(near(heightAt(type, 0), 0));
            QVERIFY(near(heightAt(type, 100), 100));
        }
        QVERIFY(near(heightAt(PortamentoPoint::Linear, 25), 25));
        QVERIFY(near(heightAt(PortamentoPoint::S, 50), 50));
        QVERIFY(near(heightAt(PortamentoPoint::S, 25), 50 - 50 * M_SQRT1_2));
        QVERIFY(near(heightAt(PortamentoPoint::J, 50), 100 - 100 * M_SQRT1_2));
        QVERIFY(near(heightAt(PortamentoPoint::R, 50), 100 * M_SQRT1_2));
    }

    void the_height_outside_the_points_is_that_of_the_nearest_point() {
        const QList<PortamentoPoint> points{
            {-20, -50, PortamentoPoint::S     },
            {30,  10,  PortamentoPoint::Linear}
        };
        QCOMPARE(PortamentoPoint::heightAt(points, -100), -50.0);
        QCOMPARE(PortamentoPoint::heightAt(points, 500), 10.0);
        QCOMPARE(PortamentoPoint::heightAt({}, 0), 0.0);
        // A segment of no length
        QCOMPARE(PortamentoPoint::heightAt(
                     {
                         {0,  0,  PortamentoPoint::S},
                         {0,  40, PortamentoPoint::S},
                         {10, 40, PortamentoPoint::S}
        },
                     0),
                 0.0);
    }
};

QTEST_APPLESS_MAIN(test_PortamentoPoint)

#include "test_PortamentoPoint.moc"
