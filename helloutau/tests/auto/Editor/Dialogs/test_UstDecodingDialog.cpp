#include <filesystem>
#include <fstream>

#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <hellokit/Support/TextCodec.h>

#include <helloutau/Editor/Dialogs/ExportUstDialog.h>
#include <helloutau/Editor/Dialogs/UstDecodingDialog.h>

using namespace hello;
using namespace hello::daw;
namespace fs = std::filesystem;

class test_UstDecodingDialog : public QObject {
    Q_OBJECT

private:
    // The lyric あいｱ in Shift_JIS. GBK reads the last byte as a lead byte without its trail.
    // Without the kana, windows-1252 would read the byte as ± and rank first.
    static std::optional<kit::UstDocument> shiftJisUst(const QTemporaryDir &dir) {
        const auto path = fs::path(dir.path().toStdU16String()) / "song.ust";
        {
            std::ofstream out(path, std::ios::binary);
            out << "[#VERSION]\r\nUST Version1.2\r\n[#SETTING]\r\nTempo=120.00\r\nTracks=1\r\n"
                   "ProjectName=song\r\nMode2=True\r\n[#0000]\r\nLength=480\r\n"
                   "Lyric=\x82\xa0\x82\xa2\xb1\r\nNoteNum=60\r\n[#TRACKEND]\r\n";
        }
        kit::DiagnosticList diagnostics;
        return kit::UstDocument::open(path, diagnostics);
    }

private Q_SLOTS:
    void cleanup() {
        kit::TextCodec::setSystemName({});
    }

    void the_preview_follows_the_selected_encoding() {
        QTemporaryDir dir;
        const auto ust = shiftJisUst(dir);
        QVERIFY(ust);

        UstDecodingDialog dialog;
        dialog.setDocument(*ust, "song.ust");
        dialog.setSelectedCharset(QStringLiteral("Shift_JIS"));
        QCOMPARE(dialog.selectedCharset(), QStringLiteral("Shift_JIS"));
        QVERIFY(dialog.previewText().contains(QString::fromUtf8("ｱ")));

        // Text that is not valid in the encoding is marked rather than shown garbled.
        dialog.setSelectedCharset(QStringLiteral("GBK"));
        QVERIFY(!dialog.previewText().contains(QString::fromUtf8("ｱ")));
        QVERIFY(dialog.previewText().contains(QStringLiteral("(not valid in this encoding)")));
    }

    // The system encodings of several regions, which the candidates include
    void the_encoding_that_reads_best_is_selected_first_data() {
        QTest::addColumn<QString>("system");
        for (const auto name : {"GBK", "Shift_JIS", "Big5", "EUC-KR", "windows-1252", "UTF-8"}) {
            QTest::newRow(name) << QString::fromLatin1(name);
        }
    }

    // The encoding that reads the file best is selected at first, and those that cannot read
    // it are gray.
    void the_encoding_that_reads_best_is_selected_first() {
        QFETCH(QString, system);
        kit::TextCodec::setSystemName(system);
        QTemporaryDir dir;
        const auto ust = shiftJisUst(dir);
        QVERIFY(ust);

        UstDecodingDialog dialog;
        dialog.setDocument(*ust, "song.ust");
        QCOMPARE(dialog.selectedCharset(), QStringLiteral("Shift_JIS"));
        QVERIFY(!dialog.isGray(QStringLiteral("Shift_JIS")));
        QVERIFY(dialog.isGray(QStringLiteral("GBK")));
        QVERIFY(dialog.isGray(QStringLiteral("UTF-8")));
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
    test_UstDecodingDialog test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_UstDecodingDialog.moc"
