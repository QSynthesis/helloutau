#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtTest/QTest>

#include <hellokit/Document/Note.h>

using namespace hello::kit;

class test_Note : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void whitespace_lyrics_are_rests() {
        QVERIFY(Note::isRestLyric(QStringLiteral("")));
        QVERIFY(Note::isRestLyric(QStringLiteral("   \t\n")));
        QVERIFY(Note::isRestLyric(QStringLiteral("R")));
        QVERIFY(Note::isRestLyric(QStringLiteral("  r  ")));
        QVERIFY(!Note::isRestLyric(QStringLiteral("Ra")));
        QVERIFY(!Note::isRestLyric(QStringLiteral(" a ")));
    }

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

    void every_field_of_a_note_survives_a_json_round_trip() {
        Note note;
        note.lyric = QStringLiteral("la");
        note.length = 480;
        note.noteNum = 60;
        note.intensity = 80;
        note.modulation = 0;
        note.velocity = 120;
        note.preUtterance = 30;
        note.voiceOverlap = 10;
        note.startPoint = 5;
        note.tempo = 128.5;
        note.flags = QStringLiteral("g-5");
        note.envelope = Envelope::fromTimeOrder({
            {0,  0  },
            {5,  100},
            {20, 80 },
            {35, 100},
            {0,  0  }
        });
        note.vibrato = Vibrato{65, 180, 35, 20, 20, 0, 0, 1};
        note.portamento = {
            {-40, 0,  PortamentoPoint::S     },
            {50,  10, PortamentoPoint::R     },
            {20,  0,  PortamentoPoint::J     },
            {10,  -5, PortamentoPoint::Linear}
        };
        note.pitchBend = PitchBend{
            -20.0, {0, 10.5, -20}
        };
        note.label = QStringLiteral("verse");
        note.direct = QStringLiteral("direct.wav");
        note.patch = QStringLiteral("resampler.exe");
        note.regions = {QStringLiteral("A"), QStringLiteral("B")};
        note.regionEnds = {QStringLiteral("B")};
        note.userData.insert(QStringLiteral("$whatever"), QStringLiteral("kept"));

        DiagnosticList diagnostics;
        const auto back = Note::fromJson(note.toJson(), diagnostics);
        QVERIFY(back.has_value());
        QVERIFY(diagnostics.isEmpty());
        QCOMPARE(back->lyric, note.lyric);
        QCOMPARE(back->length, note.length);
        QCOMPARE(back->noteNum, note.noteNum);
        QCOMPARE(back->intensity, note.intensity);
        QCOMPARE(back->modulation, note.modulation);
        QCOMPARE(back->velocity, note.velocity);
        QCOMPARE(back->preUtterance, note.preUtterance);
        QCOMPARE(back->voiceOverlap, note.voiceOverlap);
        QCOMPARE(back->startPoint, note.startPoint);
        QCOMPARE(back->tempo, note.tempo);
        QCOMPARE(back->flags, note.flags);
        QVERIFY(back->envelope == note.envelope);
        QVERIFY(back->vibrato == note.vibrato);
        QCOMPARE(back->portamento.size(), note.portamento.size());
        for (int i = 0; i < note.portamento.size(); ++i) {
            QCOMPARE(back->portamento.at(i).x, note.portamento.at(i).x);
            QCOMPARE(back->portamento.at(i).y, note.portamento.at(i).y);
            QCOMPARE(back->portamento.at(i).type, note.portamento.at(i).type);
        }
        QVERIFY(back->pitchBend == note.pitchBend);
        QCOMPARE(back->label, note.label);
        QCOMPARE(back->direct, note.direct);
        QCOMPARE(back->patch, note.patch);
        QCOMPARE(back->regions, note.regions);
        QCOMPARE(back->regionEnds, note.regionEnds);
        QCOMPARE(back->userData, note.userData);
    }

    // An empty optional field is omitted rather than written as null or zero.
    void empty_fields_of_a_note_are_omitted() {
        Note note;
        note.lyric = QStringLiteral("la");
        note.length = 480;
        note.noteNum = 60;
        QCOMPARE(note.toJson().keys(),
                 QStringList({QStringLiteral("length"), QStringLiteral("lyric"),
                              QStringLiteral("noteNum")}));
    }

    void a_note_without_its_lyric_length_or_pitch_is_refused() {
        const QJsonObject complete{
            {QStringLiteral("lyric"),   QStringLiteral("a")},
            {QStringLiteral("length"),  480                },
            {QStringLiteral("noteNum"), 60                 },
        };
        DiagnosticList diagnostics;
        QVERIFY(Note::fromJson(complete, diagnostics).has_value());
        QVERIFY(diagnostics.isEmpty());

        for (const auto &key : complete.keys()) {
            auto incomplete = complete;
            incomplete.remove(key);
            diagnostics.clear();
            QVERIFY2(!Note::fromJson(incomplete, diagnostics).has_value(), qPrintable(key));
            QVERIFY(hasError(diagnostics));
        }
    }

    // A malformed optional field is reported and read as absent, so that the rest of the note is
    // still read.
    void a_malformed_optional_field_is_reported_and_read_as_absent() {
        const QJsonObject object{
            {QStringLiteral("lyric"),     QStringLiteral("a")   },
            {QStringLiteral("length"),    480                   },
            {QStringLiteral("noteNum"),   60                    },
            {QStringLiteral("intensity"), QStringLiteral("loud")},
        };
        DiagnosticList diagnostics;
        const auto note = Note::fromJson(object, diagnostics);
        QVERIFY(note.has_value());
        QVERIFY(!note->intensity.has_value());
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(diagnostics.at(0).severity, DiagnosticSeverity::Warning);
    }

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

    // An absent start differs from a start of zero.
    void a_pitch_curve_without_a_start_reads_back_without_one() {
        const PitchBend bend{
            std::nullopt, {1, 2}
        };
        QVERIFY(!bend.toJson().contains(QStringLiteral("start")));
        QVERIFY(PitchBend::fromJson(bend.toJson()) == bend);
    }
};

QTEST_APPLESS_MAIN(test_Note)

#include "test_Note.moc"
