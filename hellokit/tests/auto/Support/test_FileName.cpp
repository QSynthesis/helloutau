#include <filesystem>
#include <fstream>

#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Support/FileName.h>

using namespace hello::kit;

namespace fs = std::filesystem;

namespace {

    // The name ga.wav with the kana ga, U+304C, in Normalization Form C
    QString composed() {
        return QString(QChar(0x304C)) + QStringLiteral(".wav");
    }

    // The same name in Normalization Form D, the kana ka, U+304B, and the combining voiced sound
    // mark, U+3099
    QString decomposed() {
        return QString(QChar(0x304B)) + QChar(0x3099) + QStringLiteral(".wav");
    }

    fs::path pathOf(const QString &text) {
        return fs::path(text.toStdU16String());
    }

}

class test_FileName : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // A decomposed name is composed on macOS and kept elsewhere. A name without combining marks
    // is kept everywhere.
    void a_listed_name_is_composed_on_macos() {
#ifdef Q_OS_MACOS
        QCOMPARE(FileName::listed(pathOf(decomposed())), pathOf(composed()));
#else
        QCOMPARE(FileName::listed(pathOf(decomposed())), pathOf(decomposed()));
#endif
        QCOMPARE(FileName::listed(pathOf(composed())), pathOf(composed()));
        QCOMPARE(FileName::listed(fs::path("a.wav")), fs::path("a.wav"));
    }

    // The listed name of a file written with a decomposed name finds the file.
    void the_listed_name_finds_the_file() {
        QTemporaryDir dir;
        const auto folder = pathOf(dir.path());
        std::ofstream(folder / pathOf(decomposed())) << "RIFF";
        std::error_code error;
        int count = 0;
        for (const auto &entry : fs::directory_iterator(folder, error)) {
            const auto listed = FileName::listed(entry.path().filename());
            QVERIFY(fs::is_regular_file(folder / listed, error));
            ++count;
        }
        QVERIFY(!error);
        QCOMPARE(count, 1);
    }
};

QTEST_MAIN(test_FileName)
#include "test_FileName.moc"
