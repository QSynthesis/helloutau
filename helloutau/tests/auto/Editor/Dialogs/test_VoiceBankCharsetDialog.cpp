#include <QtCore/QTimer>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <helloutau/Editor/Dialogs/VoiceBankCharsetDialog.h>

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

    // A subfolder "cn" whose oto.ini has an alias in GBK that Shift_JIS cannot read
    static kit::VoiceBankDirectorySource gbkFolder() {
        kit::VoiceBankDirectorySource source;
        source.path = "cn";
        source.contents[kit::VoiceBankDirectorySource::Oto] =
            "a.wav=\xd6\xd0\xce\xc4\xd2\xf4\xd4\xb4,0,0,0,0,0\r\n";
        return source;
    }

private Q_SLOTS:
    // The folders are asked about at once. The encoding that reads them all best is selected
    // for each, but for a folder that it cannot read, which takes its own; an encoding chosen
    // applies to the folders selected, and an unchecked folder is left out.
    void the_folders_are_asked_about_at_once() {
        const auto jp = shiftJisFolder();
        const auto cn = gbkFolder();
        VoiceBankCharsetDialog dialog;
        dialog.setRoot("bank");
        dialog.setDirectories({&jp, &cn});
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Choose Encoding - bank"));
        QCOMPARE(dialog.charsetOf(0), QStringLiteral("Shift_JIS"));
        QCOMPARE(dialog.charsetOf(1), QStringLiteral("GBK"));
        QCOMPARE(dialog.currentFolder(), 0);
        QVERIFY(dialog.isGrey(QStringLiteral("GBK")));
        QVERIFY(!dialog.isGrey(QStringLiteral("Shift_JIS")));

        // For every folder selected, all at first
        dialog.setSelectedCharset(QStringLiteral("UTF-8"));
        QCOMPARE(dialog.charsetOf(1), QStringLiteral("UTF-8"));
        QVERIFY(dialog.folderText(1).startsWith(QStringLiteral("cn  -  UTF-8  (")));
        QVERIFY(dialog.folderText(1).endsWith(QStringLiteral(" invalid)")));

        dialog.selectFolders({1});
        dialog.setCurrentFolder(1);
        dialog.setSelectedCharset(QStringLiteral("GBK"));
        QCOMPARE(dialog.charsetOf(0), QStringLiteral("UTF-8"));
        QCOMPARE(dialog.charsetOf(1), QStringLiteral("GBK"));
        QVERIFY(dialog.previewText(0).contains(QString::fromUtf8("中文音源")));

        // The answers, one per folder, none for one unchecked
        kit::DiagnosticList diagnostics;
        QTimer::singleShot(0, &dialog, [&dialog] {
            dialog.setIncluded(0, false);
            dialog.accept();
        });
        QCOMPARE(dialog.selectCharsets({&jp, &cn}, diagnostics),
                 (QList<std::optional<QString>>{std::nullopt, QStringLiteral("GBK")}));

        QTimer::singleShot(0, &dialog, &QDialog::reject);
        QCOMPARE(dialog.selectCharsets({&jp, &cn}, diagnostics),
                 (QList<std::optional<QString>>{std::nullopt, std::nullopt}));
    }

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
