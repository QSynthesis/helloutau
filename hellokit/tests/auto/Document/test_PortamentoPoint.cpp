#include <QtCore/QJsonObject>
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
};

QTEST_APPLESS_MAIN(test_PortamentoPoint)

#include "test_PortamentoPoint.moc"
