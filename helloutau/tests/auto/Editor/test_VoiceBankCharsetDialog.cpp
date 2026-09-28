#include <QtCore/QTimer>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <helloutau/Editor/VoiceBankCharsetDialog.h>

using namespace hello;
using namespace hello::daw;

class test_VoiceBankCharsetDialog : public QObject {
    Q_OBJECT

private:
    // A subfolder "jp" whose oto.ini has the alias ｱ in Shift_JIS, a single byte that GBK reads
    // as a lead byte without its trail, and whose readme.txt is ASCII
    static kit::VoiceBankDirectorySource shiftJisFolder() {
        kit::VoiceBankDirectorySource source;
        source.path = "jp";
        source.contents[kit::VoiceBankDirectorySource::Oto] = "a.wav=\xb1,0,0,0,0,0\r\n";
        source.contents[kit::VoiceBankDirectorySource::Readme] = "readme";
        source.files[kit::VoiceBankDirectorySource::Oto].name = "OTO.INI";
        return source;
    }

private Q_SLOTS:
    // Each text file has a tab, under the name it has on disk, and counts what the selected
    // encoding cannot read.
    void each_file_is_previewed_in_the_selected_encoding() {
        const auto source = shiftJisFolder();
        VoiceBankCharsetDialog dialog;
        dialog.setRoot("bank");
        dialog.setDirectory(source);
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Choose Encoding - jp"));

        dialog.setSelectedCharset(QStringLiteral("Shift_JIS"));
        QCOMPARE(dialog.previewTitles(),
                 (QStringList{QStringLiteral("OTO.INI"), QStringLiteral("readme.txt")}));
        QVERIFY(dialog.previewText(0).contains(QString::fromUtf8("ｱ")));
        QCOMPARE(dialog.previewText(1), QStringLiteral("readme"));

        dialog.setSelectedCharset(QStringLiteral("GBK"));
        QCOMPARE(dialog.previewTitles(), (QStringList{QStringLiteral("OTO.INI (1 invalid)"),
                                                      QStringLiteral("readme.txt")}));
        QVERIFY(dialog.previewText(0).contains(QChar(0xfffd)));
    }

    void the_answer_is_the_selected_encoding_or_none() {
        const auto source = shiftJisFolder();
        VoiceBankCharsetDialog dialog;
        kit::DiagnosticList diagnostics;

        dialog.setSelectedCharset(QStringLiteral("Shift_JIS"));
        QTimer::singleShot(0, &dialog, &QDialog::accept);
        QCOMPARE(dialog.selectCharset(source, diagnostics), QStringLiteral("Shift_JIS"));

        QTimer::singleShot(0, &dialog, &QDialog::reject);
        QCOMPARE(dialog.selectCharset(source, diagnostics), std::nullopt);
        QVERIFY(diagnostics.empty());
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_VoiceBankCharsetDialog test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_VoiceBankCharsetDialog.moc"
