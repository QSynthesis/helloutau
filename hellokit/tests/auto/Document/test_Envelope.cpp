#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtTest/QTest>

#include <hellokit/Document/Envelope.h>

using namespace hello::kit;

class test_Envelope : public QObject {
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

    // Of a fragment of 175 ms, anchors at 0, 5, 130 and 165 ms. Without one of them the others
    // stay, and of four the removed one is replaced halfway between its neighbours, the ends of
    // the fragment at 0 included.
    void an_anchor_is_removed_and_the_others_stay() {
        const auto anchorsOf = [](const QList<EnvelopeAnchor> &timeOrder, int index) {
            return Envelope::fromTimeOrder(timeOrder)
                ->withoutAnchor(index, 175)
                .anchorsInTimeOrder();
        };
        const QList<EnvelopeAnchor> four{
            {0,  0  },
            {5,  100},
            {35, 90 },
            {10, 0  }
        };
        QCOMPARE(anchorsOf(four, 1), (QList<EnvelopeAnchor>{
                                         {0,  0 },
                                         {65, 45},
                                         {35, 90},
                                         {10, 0 }
        }));
        QCOMPARE(anchorsOf(four, 0), (QList<EnvelopeAnchor>{
                                         {2.5, 50 },
                                         {2.5, 100},
                                         {35,  90 },
                                         {10,  0  }
        }));
        QCOMPARE(anchorsOf(four, 3), (QList<EnvelopeAnchor>{
                                         {0,    0  },
                                         {5,    100},
                                         {22.5, 90 },
                                         {22.5, 45 }
        }));
        QCOMPARE(anchorsOf(four, 4), four);

        const QList<EnvelopeAnchor> five{
            {0,  0  },
            {5,  100},
            {20, 80 },
            {35, 90 },
            {10, 0  }
        };
        QCOMPARE(anchorsOf(five, 2), four);
        QCOMPARE(anchorsOf(five, 1), (QList<EnvelopeAnchor>{
                                         {0,  0 },
                                         {25, 80},
                                         {35, 90},
                                         {10, 0 }
        }));
        // p3 and p5 remain, as the new p3 and p4.
        QCOMPARE(anchorsOf(five, 4), (QList<EnvelopeAnchor>{
                                         {0,   0  },
                                         {5,   100},
                                         {105, 80 },
                                         {45,  90 }
        }));
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

    void an_envelope_is_written_in_time_order() {
        const auto envelope = Envelope::fromTimeOrder({
            {0,  0  },
            {5,  100},
            {35, 90 },
            {10, 0  }
        });
        const auto anchors = envelope->toJson().value(QStringLiteral("anchors")).toArray();
        QCOMPARE(anchors.size(), 4);
        QCOMPARE(anchors.at(2).toObject().value(QStringLiteral("x")).toDouble(), 35.0);
        QVERIFY(Envelope::fromJson(envelope->toJson()) == envelope);
    }

    void an_envelope_of_another_number_of_anchors_is_refused() {
        const QJsonObject object{
            {QStringLiteral("anchors"),
             QJsonArray{QJsonObject{{QStringLiteral("x"), 0}, {QStringLiteral("y"), 0}},
                        QJsonObject{{QStringLiteral("x"), 5}, {QStringLiteral("y"), 100}},
                        QJsonObject{{QStringLiteral("x"), 0}, {QStringLiteral("y"), 0}}}},
        };
        QVERIFY(!Envelope::fromJson(object).has_value());
    }
};

QTEST_APPLESS_MAIN(test_Envelope)

#include "test_Envelope.moc"
