#include <filesystem>
#include <fstream>

#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <helloutau/Editor/ExportUstDialog.h>
#include <helloutau/Editor/UstCharsetDialog.h>

using namespace hello;
using namespace hello::daw;
namespace fs = std::filesystem;

class test_UstCharsetDialog : public QObject {
    Q_OBJECT

private:
    // The lyric ｱ in Shift_JIS, a single byte that GBK reads as a lead byte without its trail
    static std::optional<kit::UstDocument> shiftJisUst(const QTemporaryDir &dir) {
        const auto path = fs::path(dir.path().toStdU16String()) / "song.ust";
        {
            std::ofstream out(path, std::ios::binary);
            out << "[#VERSION]\r\nUST Version1.2\r\n[#SETTING]\r\nTempo=120.00\r\nTracks=1\r\n"
                   "ProjectName=song\r\nMode2=True\r\n[#0000]\r\nLength=480\r\nLyric=\xb1\r\n"
                   "NoteNum=60\r\n[#TRACKEND]\r\n";
        }
        kit::DiagnosticList diagnostics;
        return kit::UstDocument::open(path, diagnostics);
    }

private Q_SLOTS:
    void the_preview_follows_the_selected_encoding() {
        QTemporaryDir dir;
        const auto ust = shiftJisUst(dir);
        QVERIFY(ust);

        UstCharsetDialog dialog;
        dialog.setDocument(*ust, "song.ust");
        dialog.setSelectedCharset(QStringLiteral("Shift_JIS"));
        QCOMPARE(dialog.selectedCharset(), QStringLiteral("Shift_JIS"));
        QVERIFY(dialog.previewText().contains(QString::fromUtf8("ｱ")));

        // Text that is not valid in the encoding is marked rather than shown garbled.
        dialog.setSelectedCharset(QStringLiteral("GBK"));
        QVERIFY(!dialog.previewText().contains(QString::fromUtf8("ｱ")));
        QVERIFY(dialog.previewText().contains(QStringLiteral("(not valid in this encoding)")));
    }

    // The encoding that reads the file best is selected at first, and those that cannot read
    // it are grey.
    void the_encoding_that_reads_best_is_selected_first() {
        QTemporaryDir dir;
        const auto ust = shiftJisUst(dir);
        QVERIFY(ust);

        UstCharsetDialog dialog;
        dialog.setDocument(*ust, "song.ust");
        QCOMPARE(dialog.selectedCharset(), QStringLiteral("Shift_JIS"));
        QVERIFY(!dialog.isGrey(QStringLiteral("Shift_JIS")));
        QVERIFY(dialog.isGrey(QStringLiteral("GBK")));
        QVERIFY(dialog.isGrey(QStringLiteral("UTF-8")));
    }

    void the_export_encodings_start_with_utf8() {
        auto charsets = ExportUstDialog::charsets();
        QCOMPARE(charsets.first(), QStringLiteral("UTF-8"));
        QVERIFY(charsets.contains(QStringLiteral("Shift_JIS")));
        QCOMPARE(charsets.removeDuplicates(), 0);

        ExportUstDialog dialog("song.ust", QStringLiteral("Shift_JIS"));
        QCOMPARE(dialog.charset(), QStringLiteral("Shift_JIS"));
        QCOMPARE(dialog.path(), fs::path("song.ust"));
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_UstCharsetDialog test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_UstCharsetDialog.moc"
