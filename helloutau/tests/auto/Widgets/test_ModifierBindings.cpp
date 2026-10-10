#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtTest/QTest>

#include <helloutau/Widgets/ModifierBindings.h>

using namespace hello::daw;

namespace {

    // The scheme of the test, independent of the schemes of the editor. Scene 0 drags a note:
    // Move starts without modifiers, with Snap Off and Lock as toggles, Copy starts with Ctrl,
    // with Snap Off as toggle, and Scale starts with Ctrl+Alt.
    enum Role { Move, Copy, SnapOff, Lock, Scale };

    const ModifierScheme &scheme() {
        static const ModifierScheme scheme(
            "test", "test_ModifierBindings", "Test",
            {
                {Move,    "move",    "Move",     Qt::NoModifier                       },
                {Copy,    "copy",    "Copy",     Qt::ControlModifier                  },
                {SnapOff, "snapOff", "Snap Off", Qt::AltModifier                      },
                {Lock,    "lock",    "Lock",     Qt::ShiftModifier                    },
                {Scale,   "scale",   "Scale",    Qt::ControlModifier | Qt::AltModifier},
        },
            {{0, {{Move, {SnapOff, Lock}}, {Copy, {SnapOff}}, {Scale, {}}}}});
        return scheme;
    }

    // A scheme in which Ctrl+Shift splits in two ways: A without modifiers with Y on, and B
    // with Shift and Z on.
    enum OtherRole { A, B, Y, Z };

    const ModifierScheme &ambiguousScheme() {
        static const ModifierScheme scheme(
            "ambiguous", "test_ModifierBindings", "Ambiguous",
            {
                {A, "a", "A", Qt::NoModifier                         },
                {B, "b", "B", Qt::ShiftModifier                      },
                {Y, "y", "Y", Qt::ControlModifier | Qt::ShiftModifier},
                {Z, "z", "Z", Qt::ControlModifier                    },
        },
            {{0, {{A, {Y}}, {B, {Z}}}}});
        return scheme;
    }

    using Activation = ModifierBindings::Activation;
    using Conflict = ModifierBindings::Conflict;

    constexpr auto ctrl = Qt::ControlModifier;
    constexpr auto alt = Qt::AltModifier;
    constexpr auto shift = Qt::ShiftModifier;

}

class test_ModifierBindings : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void the_defaults_come_from_the_roles() {
        const ModifierBindings bindings(scheme());
        QCOMPARE(bindings.modifiers(Move), std::optional<Qt::KeyboardModifiers>(Qt::NoModifier));
        QCOMPARE(bindings.modifiers(Scale), std::optional<Qt::KeyboardModifiers>(ctrl | alt));
        QVERIFY(bindings.isValid());
        QVERIFY(bindings.toJson().isEmpty());
    }

    // An operation that starts with exactly the modifiers held is selected first, otherwise the
    // only split into the start of an operation and some of its toggles.
    void an_operation_is_selected_by_the_modifiers() {
        const ModifierBindings bindings(scheme());
        QCOMPARE(bindings.activate(0, Qt::NoModifier), (Activation{0, Move, {}}));
        QCOMPARE(bindings.activate(0, ctrl), (Activation{0, Copy, {}}));
        // Exactly Scale, although Copy with Snap Off splits Ctrl+Alt as well
        QCOMPARE(bindings.activate(0, ctrl | alt), (Activation{0, Scale, {}}));
        QCOMPARE(bindings.activate(0, alt), (Activation{0, Move, {SnapOff}}));
        QCOMPARE(bindings.activate(0, alt | shift), (Activation{
                                                        0, Move, {SnapOff, Lock}
        }));
        QCOMPARE(bindings.activate(0, ctrl | shift), std::nullopt);
        // Only Ctrl, Alt, Shift and Meta count.
        QCOMPARE(bindings.activate(0, ctrl | Qt::KeypadModifier), (Activation{0, Copy, {}}));
    }

    void a_role_that_is_off_takes_no_part() {
        ModifierBindings bindings(scheme());
        bindings.setModifiers(Copy, std::nullopt);
        QCOMPARE(bindings.modifiers(Copy), std::nullopt);
        QCOMPARE(bindings.activate(0, ctrl), std::nullopt);
        bindings.setModifiers(SnapOff, std::nullopt);
        QCOMPARE(bindings.activate(0, alt), std::nullopt);
    }

    void ambiguous_modifiers_select_nothing() {
        const ModifierBindings bindings(ambiguousScheme());
        QCOMPARE(bindings.activate(0, ctrl | shift), std::nullopt);
        const auto ambiguities = bindings.ambiguities();
        QCOMPARE(ambiguities.size(), 1);
        QCOMPARE(ambiguities[0].modifiers, ctrl | shift);
        QCOMPARE(ambiguities[0].activations.size(), 2);
        QVERIFY(ambiguities[0].activations.contains(Activation{0, A, {Y}}));
        QVERIFY(ambiguities[0].activations.contains(Activation{0, B, {Z}}));
        // An ambiguity leaves the bindings valid.
        QVERIFY(bindings.isValid());
        QVERIFY(ModifierBindings(scheme()).ambiguities().isEmpty());
    }

    // While an operation runs, the rest of the modifiers held, without those that started it,
    // turns on the toggles that add up to exactly that rest.
    void the_toggles_follow_the_modifiers_held() {
        const ModifierBindings bindings(scheme());
        auto move = *bindings.activate(0, alt);
        bindings.updateToggles(move, alt | shift);
        QCOMPARE(move.toggles, (QList<int>{SnapOff, Lock}));
        QVERIFY(move.isOn(Lock));
        bindings.updateToggles(move, shift);
        QCOMPARE(move.toggles, (QList<int>{Lock}));
        // No toggles add up to Ctrl, so the toggles stay.
        bindings.updateToggles(move, ctrl);
        QCOMPARE(move.toggles, (QList<int>{Lock}));
        bindings.updateToggles(move, Qt::NoModifier);
        QVERIFY(move.toggles.isEmpty());

        auto copy = *bindings.activate(0, ctrl);
        bindings.updateToggles(copy, ctrl | alt);
        QCOMPARE(copy.toggles, (QList<int>{SnapOff}));
        bindings.updateToggles(copy, alt);
        QCOMPARE(copy.toggles, (QList<int>{SnapOff}));
    }

    void a_role_is_held_only_with_exactly_its_modifiers() {
        ModifierBindings bindings(scheme());
        QVERIFY(bindings.isHeld(Lock, shift));
        QVERIFY(!bindings.isHeld(Lock, shift | ctrl));
        // A role without modifiers is never held.
        QVERIFY(!bindings.isHeld(Move, Qt::NoModifier));
        bindings.setModifiers(Lock, std::nullopt);
        QVERIFY(!bindings.isHeld(Lock, shift));
    }

    void conflicts_are_reported_once() {
        ModifierBindings bindings(scheme());
        // Two operations that start alike
        bindings.setModifiers(Copy, Qt::NoModifier);
        QCOMPARE(bindings.conflicts(), (QList<Conflict>{
                                           {0, Move, Copy}
        }));
        QVERIFY(!bindings.isValid());

        // A toggle without modifiers, in two operations, reported once
        bindings = ModifierBindings(scheme());
        bindings.setModifiers(SnapOff, Qt::NoModifier);
        QCOMPARE(bindings.conflicts(), (QList<Conflict>{
                                           {0, SnapOff, std::nullopt}
        }));

        // A toggle that shares a key with the start of its operation
        bindings = ModifierBindings(scheme());
        bindings.setModifiers(SnapOff, ctrl);
        QCOMPARE(bindings.conflicts(), (QList<Conflict>{
                                           {0, Copy, SnapOff}
        }));

        // Toggles of one operation that share a key
        bindings = ModifierBindings(scheme());
        bindings.setModifiers(Lock, alt);
        QCOMPARE(bindings.conflicts(), (QList<Conflict>{
                                           {0, SnapOff, Lock}
        }));
    }

    void only_the_roles_that_start_an_operation_are_starts() {
        QVERIFY(scheme().isStart(Move));
        QVERIFY(scheme().isStart(Copy));
        QVERIFY(scheme().isStart(Scale));
        QVERIFY(!scheme().isStart(SnapOff));
        QVERIFY(!scheme().isStart(Lock));
    }

    // Only the roles that differ from their defaults are written, an off role as null.
    void the_changed_roles_survive_a_round_trip() {
        ModifierBindings bindings(scheme());
        bindings.setModifiers(Copy, Qt::MetaModifier | shift);
        bindings.setModifiers(Lock, std::nullopt);
        const auto json = bindings.toJson();
        QCOMPARE(json.keys(), (QStringList{QStringLiteral("copy"), QStringLiteral("lock")}));
        QVERIFY(json.value(QStringLiteral("lock")).isNull());

        ModifierBindings back(scheme());
        QVERIFY(back.readJson(json));
        QVERIFY(back == bindings);
        QVERIFY(back != ModifierBindings(scheme()));
    }

    // Bindings of two schemes differ even if the schemes declare the same roles.
    void bindings_of_another_scheme_are_unequal() {
        const ModifierScheme copy("test", "test_ModifierBindings", "Test", scheme().roles(),
                                  scheme().scenes());
        QVERIFY(ModifierBindings(copy) != ModifierBindings(scheme()));
        QVERIFY(ModifierBindings(scheme()) == ModifierBindings(scheme()));
    }

    // Unknown keys and modifier names are ignored, and readJson() reports them.
    void unknown_content_is_ignored_and_reported() {
        ModifierBindings bindings(scheme());
        const QJsonObject unknownName{
            {QStringLiteral("copy"), QJsonArray{QStringLiteral("Alt"), QStringLiteral("Hyper")}},
        };
        QVERIFY(!bindings.readJson(unknownName));
        QCOMPARE(bindings.modifiers(Copy), std::optional<Qt::KeyboardModifiers>(alt));

        const QJsonObject unknownKey{
            {QStringLiteral("lock"),    QJsonArray{QStringLiteral("Meta")}},
            {QStringLiteral("unknown"), QJsonArray{QStringLiteral("Ctrl")}},
        };
        QVERIFY(!bindings.readJson(unknownKey));
        QCOMPARE(bindings.modifiers(Lock), std::optional<Qt::KeyboardModifiers>(Qt::MetaModifier));
    }
};

QTEST_APPLESS_MAIN(test_ModifierBindings)

#include "test_ModifierBindings.moc"
