#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <helloutau/Theme/ThemeTypes.h>

using namespace hello::daw;

namespace {

    // A widget with a property of each type, as a control declares them
    class Probe : public QWidget {
        Q_OBJECT
        Q_PROPERTY(hello::daw::ThemePen linePen MEMBER linePen)
        Q_PROPERTY(hello::daw::ThemeFont labelFont MEMBER labelFont)
        Q_PROPERTY(hello::daw::ThemeRect keyCap MEMBER keyCap)
        Q_PROPERTY(hello::daw::ThemeShadow shadow MEMBER shadow)
    public:
        ThemePen linePen;
        ThemeFont labelFont;
        ThemeRect keyCap;
        ThemeShadow shadow;
    };

    // The value of a property of \a probe after applying \a sheet
    void apply(Probe &probe, const QString &sheet) {
        probe.setStyleSheet(QStringLiteral("Probe { %1 }").arg(sheet));
        probe.ensurePolished();
    }

}

class test_ThemeTypes : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() {
        ThemeTypes::registerConversions();
    }

    // The mechanism the theme system rests on: a style sheet assigns a custom type through the
    // conversions registered for it. See the section on assigning custom types in
    // docs/Theme.md.
    void a_style_sheet_assigns_the_types() {
        Probe probe;
        apply(probe, QStringLiteral(
                         "qproperty-linePen: qpen((lightgrey, down=white), 2px, dash, flat, "
                         "round, dashPattern=(4px, 2px), cosmetic=true);"
                         "qproperty-labelFont: qfont(#FFFFFF, 15px, bold, true, (\"A\", \"B\"));"
                         "qproperty-keyCap: qrect(#303030, (1px, 4px), 3px);"
                         "qproperty-shadow: qshadow(#40000000, 16px, 0 4px);"));

        const auto &pen = probe.linePen;
        QCOMPARE(pen.color.value(ThemeButtonState::Up), QColor::fromString("lightgrey"));
        QCOMPARE(pen.color.value(ThemeButtonState::Down), QColor(Qt::white));
        QCOMPARE(pen.width, 2.0);
        const auto qpen = pen.pen(ThemeButtonState::Over);
        QCOMPARE(qpen.color(), QColor::fromString("lightgrey"));
        QCOMPARE(qpen.capStyle(), Qt::FlatCap);
        QCOMPARE(qpen.joinStyle(), Qt::RoundJoin);
        QCOMPARE(qpen.dashPattern(), (QList<qreal>{2, 1}));
        QVERIFY(qpen.isCosmetic());

        const auto &font = probe.labelFont;
        QCOMPARE(font.color.value(ThemeButtonState::CheckedDisabled), QColor(Qt::white));
        const auto qfont = font.font();
        QCOMPARE(qfont.pixelSize(), 15);
        QCOMPARE(qfont.weight(), QFont::Bold);
        QVERIFY(qfont.italic());
        QCOMPARE(qfont.families(), (QStringList{QStringLiteral("A"), QStringLiteral("B")}));

        QCOMPARE(probe.keyCap.margins, QMargins(4, 1, 4, 1));
        QCOMPARE(probe.keyCap.radius, 3);
        QCOMPARE(probe.shadow.color, QColor(0, 0, 0, 0x40));
        QCOMPARE(probe.shadow.blur, 16);
        QCOMPARE(probe.shadow.offset, QPointF(0, 4));
        QVERIFY(probe.shadow.isVisible());
    }

    // A plain value is the first argument; the function form may also come as a word.
    void a_word_is_the_first_argument() {
        Probe probe;
        apply(probe, QStringLiteral("qproperty-linePen: red;"));
        QCOMPARE(probe.linePen.pen().color(), QColor(Qt::red));
        QCOMPARE(probe.linePen.width, 1.0);
    }

    // A value that cannot be read leaves the property as it was, with a warning.
    void a_malformed_value_changes_nothing() {
        Probe probe;
        probe.linePen.width = 5;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("qpen.*keyword")));
        apply(probe, QStringLiteral("qproperty-linePen: qpen(width=2px, red);"));
        QCOMPARE(probe.linePen.width, 5.0);

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("qpen.*One of")));
        apply(probe, QStringLiteral("qproperty-linePen: qpen(red, 1px, wavy);"));
        QCOMPARE(probe.linePen.width, 5.0);

        // Another type's function is not taken for this one.
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("qpen was expected")));
        apply(probe, QStringLiteral("qproperty-linePen: qfont(red);"));
        QCOMPARE(probe.linePen.width, 5.0);
    }

    void the_readers_check_their_parameters() {
        ThemeError error;
        const auto read = [&error](const QString &text) {
            const auto arguments = ThemeSyntax::parseArguments(text, &error);
            return arguments ? ThemeRect::read(*arguments, &error) : std::nullopt;
        };
        QVERIFY(read(QStringLiteral("red, (1px, 2px, 3px, 4px)")));
        QCOMPARE(read(QStringLiteral("red, (1px, 2px, 3px, 4px)"))->margins, QMargins(1, 2, 3, 4));
        QVERIFY(!read(QStringLiteral("red, (1px, 2px, 3px)")));
        QVERIFY(!read(QStringLiteral("red, radius=1px, radius=2px")));
        QVERIFY(!read(QStringLiteral("red, 1px, 2px, 3px")));
        QVERIFY(!read(QStringLiteral("red, corner=2px")));
        QCOMPARE(error.position, 12);

        const auto arguments = ThemeSyntax::parseArguments(QStringLiteral("red, 12pt, 350"));
        const auto font = ThemeFont::read(*arguments, nullptr);
        QVERIFY(font);
        QCOMPARE(font->pointSize, 12.0);
        QCOMPARE(font->weight, 350);
        // What is not written stays as the base font has it.
        QFont base;
        base.setPixelSize(20);
        const auto colorOnly = ThemeSyntax::parseArguments(QStringLiteral("red"));
        QCOMPARE(ThemeFont::read(*colorOnly, nullptr)->font(base).pixelSize(), 20);
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_ThemeTypes test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_ThemeTypes.moc"
