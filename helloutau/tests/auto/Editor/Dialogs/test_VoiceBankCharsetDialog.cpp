#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTimer>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <hellokit/Support/TextCodec.h>

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

    // 偁.wav and 偄.wav: あ.wav and い.wav in Shift_JIS read as GBK
    const QString kA = QString::fromUtf8("偁.wav");
    const QString kI = QString::fromUtf8("偄.wav");

    // A folder in dir of a Japanese voice bank extracted on a Chinese system, with entries of
    // あ.wav and い.wav in Shift_JIS whose files are named kA and kI, as read
    kit::VoiceBankDirectorySource misdecodedFolder(const QTemporaryDir &dir) const {
        const auto write = [&dir](const QString &name, const QByteArray &bytes) {
            QFile file(dir.path() + QLatin1Char('/') + name);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(bytes);
            }
        };
        write(QStringLiteral("oto.ini"),
              kit::TextCodec(QStringLiteral("Shift_JIS"))
                  .encode(QString::fromUtf8("あ.wav=あ,1,2,3,4,5\r\nい.wav=い,1,2,3,4,5\r\n")));
        write(kA, "RIFF");
        write(kI, "RIFF");
        kit::DiagnosticList diagnostics;
        return kit::VoiceBankSource::readDirectory(
                   std::filesystem::path(dir.path().toStdU16String()), {}, diagnostics)
            .value_or(kit::VoiceBankDirectorySource());
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

    // For reading the folders again, the initial encoding of each folder is the encoding it was
    // read in, and the folders whose encoding is changed are reported separately.
    void reading_again_starts_with_the_current_encodings() {
        const auto jp = shiftJisFolder();
        const auto cn = gbkFolder();
        VoiceBankCharsetDialog dialog;
        dialog.setRoot("bank");
        dialog.setDirectories({&jp, &cn});
        dialog.setCurrentCharsets({QStringLiteral("EUC-KR"), QStringLiteral("GBK")});
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Read Folders Again in Encodings - bank"));
        QCOMPARE(dialog.charsetOf(0), QStringLiteral("EUC-KR"));
        QCOMPARE(dialog.charsetOf(1), QStringLiteral("GBK"));
        QVERIFY(dialog.changedFolders().isEmpty());
        // An encoding outside the candidates is listed for the folder read in it.
        QVERIFY(dialog.isGrey(QStringLiteral("EUC-KR")));

        dialog.selectFolders({0});
        dialog.setCurrentFolder(0);
        dialog.setSelectedCharset(QStringLiteral("Shift_JIS"));
        QCOMPARE(dialog.changedFolders(), QList<int>{0});
        QCOMPARE(dialog.folderText(0), QStringLiteral("jp  -  Shift_JIS  (currently EUC-KR)"));
        QCOMPARE(dialog.folderText(1), QStringLiteral("cn  -  GBK"));
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

    // A folder whose encoding is settled is asked about its file names only, and its encoding
    // cannot be changed.
    void a_settled_folder_is_asked_about_its_file_names_only() {
        QTemporaryDir dir;
        const auto source = misdecodedFolder(dir);
        VoiceBankCharsetDialog dialog;
        dialog.setRoot("bank");
        dialog.setQuestions({
            {&source, QStringLiteral("Shift_JIS")}
        });
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Determine File Names - bank"));
        QVERIFY(dialog.misdecodedNameQuestion(0).has_value());
        QCOMPARE(dialog.misdecodedNames(0), (QStringList{kA, kI}));
        QVERIFY(dialog.previewTitles().contains(QStringLiteral("File Names (2)")));
        QVERIFY(dialog.folderText(0).contains(QStringLiteral("to determine)")));

        dialog.setSelectedCharset(QStringLiteral("UTF-8"));
        QCOMPARE(dialog.charsetOf(0), QStringLiteral("Shift_JIS"));

        const auto answer = dialog.answers().value(0);
        QCOMPARE(answer.charset, std::optional(QStringLiteral("Shift_JIS")));
        QVERIFY(answer.fileNameEncoding.has_value());
        // Both names misdecoded, which the record without lists records.
        QCOMPARE(answer.fileNameEncoding->misdecodedAs, QStringLiteral("GBK"));
        QVERIFY(!answer.fileNameEncoding->files.has_value());
    }

    // The question about the file names follows the encoding chosen for the folder, because the
    // inference reads the entries in it.
    void the_file_names_follow_the_encoding_of_the_folder() {
        QTemporaryDir dir;
        const auto source = misdecodedFolder(dir);
        VoiceBankCharsetDialog dialog;
        dialog.setQuestions({
            {&source, std::nullopt}
        });
        dialog.setSelectedCharset(QStringLiteral("UTF-8"));
        QVERIFY(!dialog.misdecodedNameQuestion(0).has_value());
        QVERIFY(!dialog.previewTitles().contains(QStringLiteral("File Names (2)")));
        dialog.setSelectedCharset(QStringLiteral("Shift_JIS"));
        QVERIFY(dialog.misdecodedNameQuestion(0).has_value());
        QCOMPARE(dialog.answers().value(0).charset, std::optional(QStringLiteral("Shift_JIS")));
    }

    void the_choices_make_the_record() {
        QTemporaryDir dir;
        const auto source = misdecodedFolder(dir);
        VoiceBankCharsetDialog dialog;
        dialog.setQuestions({
            {&source, QStringLiteral("Shift_JIS")}
        });
        dialog.setMisdecoded(0, kI, false);
        auto record = dialog.answers().value(0).fileNameEncoding;
        QVERIFY(record.has_value());
        QVERIFY(record->files.has_value());
        QCOMPARE(record->files->misdecoded, QStringList{kA});
        QCOMPARE(record->files->correct, QStringList{kI});

        // Other encodings, under which the names restore otherwise
        dialog.setFileNameEncodings(0, QStringLiteral("Big5"), QStringLiteral("GBK"));
        QCOMPARE(dialog.misdecodedNameQuestion(0)->original(), QStringLiteral("Big5"));
        record = dialog.answers().value(0).fileNameEncoding;
        QVERIFY(record.has_value());
        QCOMPARE(record->original, QStringLiteral("Big5"));

        // Using the names as they are is a decision as well.
        dialog.setNamesAsTheyAre(0, true);
        QCOMPARE(dialog.answers().value(0).fileNameEncoding,
                 std::optional(kit::VoiceBankConfig::FileNameEncoding()));
    }

    // A folder left out, and every folder of a declined dialog, records no file names.
    void a_folder_not_read_records_no_file_names() {
        QTemporaryDir dir;
        const auto source = misdecodedFolder(dir);
        VoiceBankCharsetDialog dialog;
        dialog.setQuestions({
            {&source, std::nullopt}
        });
        dialog.setSelectedCharset(QStringLiteral("Shift_JIS"));
        dialog.setIncluded(0, false);
        const auto answer = dialog.answers().value(0);
        QVERIFY(!answer.charset.has_value());
        QVERIFY(!answer.fileNameEncoding.has_value());

        kit::DiagnosticList diagnostics;
        QTimer::singleShot(0, &dialog, &QDialog::reject);
        const auto declined = dialog.selectEncodings(
            {
                {&source, QStringLiteral("Shift_JIS")}
        },
            diagnostics);
        QCOMPARE(declined.value(0).charset, std::optional(QStringLiteral("Shift_JIS")));
        QVERIFY(!declined.value(0).fileNameEncoding.has_value());
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
