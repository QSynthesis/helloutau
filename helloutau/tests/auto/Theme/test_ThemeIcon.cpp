#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryDir>
#include <QtGui/QGuiApplication>
#include <QtGui/QPalette>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QPushButton>

#include <helloutau/Theme/ThemeIcon.h>
#include <helloutau/Theme/ThemeStyleSheet.h>

using namespace hello::daw;

namespace {

    // The icons beside the test: square fills the whole icon with currentColor, half its left
    // half.
    QString icons() {
        return QDir(QString::fromUtf8(TEST_RESOURCE_DIRECTORY))
            .absoluteFilePath(QStringLiteral("icons"));
    }

    QString quoted(const QString &path) {
        return u'"' + path + u'"';
    }

    std::optional<ThemeIcon> read(const QString &text, ThemeError *error = nullptr) {
        const auto arguments = ThemeSyntax::parseArguments(text, error);
        return arguments ? ThemeIcon::read(*arguments, error) : std::nullopt;
    }

    QImage imageOf(const QIcon &icon, QIcon::Mode mode = QIcon::Normal,
                   QIcon::State state = QIcon::Off) {
        return icon.pixmap(QSize(16, 16), 1.0, mode, state).toImage();
    }

    // The color in the left half, and whether the right half is empty
    QColor leftOf(const QImage &image) {
        return image.pixelColor(4, 8);
    }

    bool rightIsEmpty(const QImage &image) {
        return image.pixelColor(12, 8).alpha() == 0;
    }

    bool writeFile(const QString &path, const QByteArray &data) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
    }

}

class test_ThemeIcon : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Missing states fall back as for every field with button states; auto and a missing color
    // follow the text.
    void an_icon_is_read_with_its_states() {
        const auto icon = read(
            QStringLiteral("(\"a.svg\", up2=\"b.svg\"), (#FF0000, over=#00FF00, disabled=auto)"));
        QVERIFY(icon);
        QCOMPARE(icon->files.value(ThemeButtonState::Down), QStringLiteral("a.svg"));
        QCOMPARE(icon->files.value(ThemeButtonState::CheckedDown), QStringLiteral("b.svg"));
        QCOMPARE(icon->colors.value(ThemeButtonState::Down), QColor(0, 255, 0));
        QVERIFY(!icon->colors.value(ThemeButtonState::Disabled).isValid());
        QCOMPARE(icon->colors.value(ThemeButtonState::CheckedOver), QColor(255, 0, 0));

        const auto plain = read(QStringLiteral("a.svg"));
        QVERIFY(plain);
        QVERIFY(!plain->colors.value(ThemeButtonState::Up).isValid());

        ThemeError error;
        QVERIFY(!read(QStringLiteral("(\"a.svg\", pressed=\"b.svg\")"), &error));
        QVERIFY(error.message.contains(QStringLiteral("pressed")));
        QVERIFY(!read(QString(), &error));
        QVERIFY(!read(QStringLiteral("a.svg, notacolor"), &error));
    }

    // The name has no separator of folders and no quote, whatever the paths hold.
    void the_file_name_describes_the_icon() {
        ThemeIcon icon;
        const auto path =
            QStringLiteral("C:/a b/\"q\"\\x/") + QChar(0x56FE) + QStringLiteral(".svg");
        icon.files = ThemeStates<QString>(path);
        icon.files.setValue(ThemeButtonState::CheckedUp, QStringLiteral("b.svg"));
        icon.colors.setValue(ThemeButtonState::Up, QColor(1, 2, 3, 4));

        const auto name = icon.fileName();
        QVERIFY(name.endsWith(QStringLiteral(".svgx")));
        for (const QChar c : {u'/', u'\\', u'"', u'\''}) {
            QVERIFY(!name.contains(c));
        }
        QVERIFY(ThemeIcon::fromFileName(name) == icon);
        QVERIFY(!ThemeIcon::fromFileName(u"icon.png"));
    }

    // QIcon passes its mode and state: active and selected as over, disabled, on as checked.
    void qicon_chooses_the_engine_by_the_suffix() {
        const auto icon = read(quoted(icons() + QStringLiteral("/square.svg")) +
                               QStringLiteral(", (#FF0000, over=#00FF00, disabled=#800000FF)"));
        QVERIFY(icon);
        auto withChecked = *icon;
        withChecked.files.setValue(ThemeButtonState::CheckedUp,
                                   icons() + QStringLiteral("/half.svg"));

        const QIcon qicon(withChecked.fileName());
        QVERIFY(!qicon.isNull());
        QVERIFY(ThemeIcon::of(qicon) == withChecked);

        QCOMPARE(leftOf(imageOf(qicon)), QColor(255, 0, 0));
        QCOMPARE(leftOf(imageOf(qicon, QIcon::Active)), QColor(0, 255, 0));
        QCOMPARE(leftOf(imageOf(qicon, QIcon::Selected)), QColor(0, 255, 0));
        // The alpha of the color applies to the whole icon.
        const auto disabled = leftOf(imageOf(qicon, QIcon::Disabled));
        QVERIFY(qAbs(disabled.alpha() - 128) <= 2);
        QVERIFY(disabled.blue() > 240 && disabled.red() < 16);

        const auto checked = imageOf(qicon, QIcon::Normal, QIcon::On);
        QCOMPARE(leftOf(checked), QColor(255, 0, 0));
        QVERIFY(rightIsEmpty(checked));
        QVERIFY(!rightIsEmpty(imageOf(qicon)));
    }

    // A control passes the state that QIcon cannot, such as pressed, and the color of its text.
    void for_state_fixes_the_state_and_the_text() {
        const auto icon = read(quoted(icons() + QStringLiteral("/square.svg")) +
                               QStringLiteral(", (auto, down=#00FF00)"));
        QVERIFY(icon);
        const auto qicon = icon->icon();

        const auto down = ThemeIcon::forState(qicon, ThemeButtonState::Down);
        QCOMPARE(leftOf(imageOf(down)), QColor(0, 255, 0));
        QCOMPARE(leftOf(imageOf(down, QIcon::Disabled)), QColor(0, 255, 0));
        QVERIFY(ThemeIcon::of(down) == *icon);

        QCOMPARE(leftOf(imageOf(ThemeIcon::forState(qicon, ThemeButtonState::Up, Qt::blue))),
                 QColor(Qt::blue));
        QCOMPARE(leftOf(imageOf(qicon)),
                 QGuiApplication::palette().color(QPalette::Active, QPalette::WindowText));

        const QIcon other(icons() + QStringLiteral("/square.svg"));
        QCOMPARE(ThemeIcon::forState(other, ThemeButtonState::Down).cacheKey(), other.cacheKey());
        QVERIFY(!ThemeIcon::of(other));
    }

    // svg(...) becomes a url, its files @/... in the folder of the style sheet; comments and
    // strings stay as written.
    void a_style_sheet_gives_an_icon() {
        ThemeStyleSheet::Options options;
        options.directory = icons();
        const auto sheet = ThemeStyleSheet::preprocess(
            u"QPushButton { qproperty-icon: svg(\"@/square.svg\", #0000FF); } /* svg(x) */",
            options);
        QVERIFY(sheet.contains(QStringLiteral("qproperty-icon: url(\"")));
        QVERIFY(sheet.contains(QStringLiteral(".svgx\"); }")));
        QVERIFY(sheet.endsWith(QStringLiteral("/* svg(x) */")));

        QPushButton button;
        button.setStyleSheet(sheet);
        button.ensurePolished();
        const auto icon = ThemeIcon::of(button.icon());
        QVERIFY(icon);
        QCOMPARE(icon->files.value(ThemeButtonState::Up), icons() + QStringLiteral("/square.svg"));
        QCOMPARE(leftOf(imageOf(button.icon())), QColor(0, 0, 255));

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("svg\\(nofile")));
        const auto bad = u"A { qproperty-icon: svg(nofile=1); }";
        QCOMPARE(ThemeStyleSheet::preprocess(bad, options), QString::fromUtf16(bad));
    }

    // The files are read once, until the cache is cleared.
    void clearing_the_cache_reads_the_files_again() {
        QTemporaryDir dir;
        const auto path = dir.filePath(QStringLiteral("icon.svg"));
        const QByteArray head = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"16\" "
                                "height=\"16\"><rect width=\"16\" height=\"16\" fill=\"";
        QVERIFY(writeFile(path, head + "currentColor\"/></svg>"));
        const auto icon = read(quoted(path) + QStringLiteral(", #FF0000"));
        QVERIFY(icon);
        QCOMPARE(leftOf(imageOf(icon->icon())), QColor(255, 0, 0));

        QVERIFY(writeFile(path, head + "#00FF00\"/></svg>"));
        QCOMPARE(leftOf(imageOf(icon->icon())), QColor(255, 0, 0));
        ThemeIcon::clearCache();
        QCOMPARE(leftOf(imageOf(icon->icon())), QColor(0, 255, 0));
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_ThemeIcon test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_ThemeIcon.moc"
