#include <QtTest/QTest>

#include <helloutau/Theme/ThemeReader.h>
#include <helloutau/Theme/ThemeStates.h>
#include <helloutau/Theme/ThemeStyleSheet.h>
#include <helloutau/Theme/ThemeSyntax.h>

using namespace hello::daw;

namespace {

    ThemeValue parsed(const QString &text) {
        ThemeError error;
        const auto value = ThemeSyntax::parse(text, &error);
        if (!value) {
            qWarning() << "at" << error.position << error.message;
        }
        return value.value_or(ThemeValue());
    }

    std::optional<QColor> readColor(const ThemeValue &value, ThemeError *error) {
        return ThemeReader::color(value, error);
    }

    QString converted(const QString &text, double scale = 1, double fontScale = 1) {
        ThemeStyleSheet::Options options;
        options.directory = QStringLiteral("C:/themes/dark");
        options.scale = scale;
        options.fontScale = fontScale;
        return ThemeStyleSheet::preprocess(text, options);
    }

}

class test_ThemeSyntax : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void a_value_is_read_into_its_parts() {
        const auto pen = parsed(QStringLiteral("qpen((lightgrey, down=white), 1px, solid, "
                                               "dashPattern=(2px, 2px), family=\"A, B\")"));
        QCOMPARE(pen.kind, ThemeValue::Function);
        QCOMPARE(pen.text, QStringLiteral("qpen"));
        QCOMPARE(pen.arguments.size(), size_t(5));
        const auto &states = pen.arguments[0].value;
        QCOMPARE(states.kind, ThemeValue::Group);
        QCOMPARE(states.arguments[1].key, QStringLiteral("down"));
        QCOMPARE(states.arguments[1].value.text, QStringLiteral("white"));
        QCOMPARE(pen.arguments[3].key, QStringLiteral("dashPattern"));
        QCOMPARE(pen.arguments[3].value.kind, ThemeValue::Group);
        QCOMPARE(pen.arguments[4].value.kind, ThemeValue::String);
        QCOMPARE(pen.arguments[4].value.text, QStringLiteral("A, B"));

        // Blanks separate the items of a sequence, and toString() restores the original text.
        const auto size = parsed(QStringLiteral("  2px   3px "));
        QCOMPARE(size.kind, ThemeValue::Sequence);
        QCOMPARE(size.items.size(), size_t(2));
        QCOMPARE(size.items[1].position, 8);
        QCOMPARE(pen.toString(), QStringLiteral("qpen((lightgrey, down=white), 1px, solid, "
                                                "dashPattern=(2px, 2px), family=\"A, B\")"));
        QCOMPARE(parsed(QStringLiteral("'it\\'s' /* note */")).text, QStringLiteral("it's"));
    }

    void malformed_values_are_reported_where_they_fail() {
        const auto errorOf = [](const QString &text) {
            ThemeError error;
            const auto value = ThemeSyntax::parse(text, &error);
            return value ? -1 : error.position;
        };
        QCOMPARE(errorOf(QStringLiteral("f(a=1, b)")), 7);
        QCOMPARE(errorOf(QStringLiteral("f(a b=1)")), 2);
        QCOMPARE(errorOf(QStringLiteral("f(1, 2")), 6);
        QCOMPARE(errorOf(QStringLiteral("\"open")), 0);
        QCOMPARE(errorOf(QStringLiteral("a) b")), 1);
        QCOMPARE(errorOf(QStringLiteral("f(,)")), 2);

        // Arguments without the enclosing parentheses, as passed from a style sheet
        const auto arguments = ThemeSyntax::parseArguments(QStringLiteral("white, 1px"));
        QVERIFY(arguments);
        QCOMPARE(arguments->size(), size_t(2));
    }

    // Omitted states fall back: over to up, down to over, disabled to up, and the checked states
    // in the same way among themselves, with up2 falling back to up.
    void button_states_fall_back() {
        using S = ThemeButtonState;
        auto states = ThemeStates<QColor>::read(
            parsed(QStringLiteral("(red, down=blue, up2=lime)")), readColor, nullptr);
        QVERIFY(states);
        QCOMPARE(states->value(S::Up), QColor(Qt::red));
        QCOMPARE(states->value(S::Over), QColor(Qt::red));
        QCOMPARE(states->value(S::Down), QColor(Qt::blue));
        QCOMPARE(states->value(S::Disabled), QColor(Qt::red));
        QCOMPARE(states->value(S::CheckedUp), QColor(Qt::green));
        QCOMPARE(states->value(S::CheckedOver), QColor(Qt::green));
        QCOMPARE(states->value(S::CheckedDown), QColor(Qt::green));
        QCOMPARE(states->value(S::CheckedDisabled), QColor(Qt::green));

        // Positional states follow the order of the keys. A single value applies to all states.
        states = ThemeStates<QColor>::read(parsed(QStringLiteral("(red, lime, blue)")), readColor,
                                           nullptr);
        QCOMPARE(states->value(S::Over), QColor(Qt::green));
        QCOMPARE(states->value(S::CheckedDown), QColor(Qt::red));
        // Down falls back to over, not to up.
        states = ThemeStates<QColor>::read(parsed(QStringLiteral("(red, over=lime)")), readColor,
                                           nullptr);
        QCOMPARE(states->value(S::Down), QColor(Qt::green));
        QCOMPARE(states->value(S::CheckedDown), QColor(Qt::red));
        states = ThemeStates<QColor>::read(parsed(QStringLiteral("#123456")), readColor, nullptr);
        QCOMPARE(states->value(S::CheckedDisabled), QColor(0x12, 0x34, 0x56));

        ThemeError error;
        QVERIFY(!ThemeStates<QColor>::read(parsed(QStringLiteral("(red, pressed=blue)")), readColor,
                                           &error));
        QCOMPARE(error.position, 14);
        QVERIFY(
            !ThemeStates<QColor>::read(parsed(QStringLiteral("(down=blue)")), readColor, &error));
        QVERIFY(
            !ThemeStates<QColor>::read(parsed(QStringLiteral("(red, 1px)")), readColor, &error));
    }

    void basic_values_are_read() {
        QCOMPARE(ThemeReader::color(parsed(QStringLiteral("#80FF0000"))), QColor(255, 0, 0, 128));
        QCOMPARE(ThemeReader::color(parsed(QStringLiteral("transparent")))->alpha(), 0);
        QCOMPARE(ThemeReader::color(parsed(QStringLiteral("rgba(255, 0, 0, 50%)"))),
                 QColor(255, 0, 0, 128));
        QCOMPARE(ThemeReader::color(parsed(QStringLiteral("rgba(0, 0, 255, 0.5)"))),
                 QColor(0, 0, 255, 128));
        QCOMPARE(ThemeReader::color(parsed(QStringLiteral("rgb(100%, 0, 0)"))), QColor(Qt::red));
        QCOMPARE(ThemeReader::color(parsed(QStringLiteral("hsv(120, 255, 255)"))),
                 QColor::fromHsv(120, 255, 255));
        QVERIFY(!ThemeReader::color(parsed(QStringLiteral("rgb(1, 2)"))));
        QVERIFY(!ThemeReader::color(parsed(QStringLiteral("notacolor"))));

        QCOMPARE(ThemeReader::pixels(parsed(QStringLiteral("12px"))), 12);
        QCOMPARE(ThemeReader::pixels(parsed(QStringLiteral("-2px"))), -2);
        QCOMPARE(ThemeReader::pixels(parsed(QStringLiteral("0"))), 0);
        QVERIFY(!ThemeReader::pixels(parsed(QStringLiteral("12"))));
        QCOMPARE(ThemeReader::size(parsed(QStringLiteral("2px 3px"))), QSize(2, 3));
        QCOMPARE(ThemeReader::size(parsed(QStringLiteral("4px"))), QSize(4, 4));
        QCOMPARE(ThemeReader::boolean(parsed(QStringLiteral("false"))), false);
        QVERIFY(!ThemeReader::boolean(parsed(QStringLiteral("no"))));
        QCOMPARE(ThemeReader::text(parsed(QStringLiteral("\"a b\""))), QStringLiteral("a b"));
        QCOMPARE(ThemeReader::integer(parsed(QStringLiteral("600"))), 600);
        QCOMPARE(ThemeReader::number(parsed(QStringLiteral("1.5"))), 1.5);
    }

    void the_extended_syntax_becomes_that_of_qt() {
        QCOMPARE(converted(QStringLiteral("A { --lineColor: red; ---width: 2px }")),
                 QStringLiteral("A { qproperty-lineColor: red; width: 2px }"));
        QCOMPARE(converted(QStringLiteral("QPushButton:not(:checked):hover { color: red }")),
                 QStringLiteral("QPushButton:!checked:hover { color: red }"));
        QCOMPARE(converted(QStringLiteral("A { image: url(@/icons/a.png) }")),
                 QStringLiteral("A { image: url(C:/themes/dark/icons/a.png) }"));

        // Lengths are scaled, and font sizes are scaled by their own factor. Strings, comments
        // and names remain unchanged.
        QCOMPARE(converted(QStringLiteral("A { width: 10px; font-size: 12px; --pen: qpen(red, "
                                          "1.5px); icon: url(\"12px.png\") /* 4px */; b2px: 1 }"),
                           2, 1.5),
                 QStringLiteral("A { width: 20px; font-size: 18px; qproperty-pen: qpen(red, "
                                "3px); icon: url(\"12px.png\") /* 4px */; b2px: 1 }"));
        QCOMPARE(converted(QStringLiteral("A { margin: -3px 4px; x: a2px }"), 2),
                 QStringLiteral("A { margin: -6px 8px; x: a2px }"));
        // A scale factor of one leaves lengths unchanged.
        QCOMPARE(converted(QStringLiteral("A { width: 1.5px }")),
                 QStringLiteral("A { width: 1.5px }"));
    }
};

QTEST_APPLESS_MAIN(test_ThemeSyntax)

#include "test_ThemeSyntax.moc"
