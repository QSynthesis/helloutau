#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <hellokit/Support/TextCodec.h>

#include <helloutau/Editor/Dialogs/VoiceBankTranscodingDialog.h>

using namespace hello;
using namespace hello::daw;

class test_VoiceBankTranscodingDialog : public QObject {
    Q_OBJECT

private:
    // 偁.wav and 偄.wav: あ.wav and い.wav in Shift_JIS read as GBK
    const QString kA = QString::fromUtf8("偁.wav");
    const QString kI = QString::fromUtf8("偄.wav");

    // The root in Shift_JIS, whose entries name あ.wav and 偄.wav, which the disk holds as kA and
    // kI, and the folder "left", which was not read, with the misdecoded name kA
    QList<VoiceBankTranscodingDialog::Folder> folders() const {
        VoiceBankTranscodingDialog::Folder root;
        root.listing.charset = QStringLiteral("Shift_JIS");
        root.listing.audioFiles = {kA, kI, QStringLiteral("a.wav")};
        root.listing.entries = {QString::fromUtf8("あ.wav"), kI};
        root.name = QStringLiteral("bank");

        VoiceBankTranscodingDialog::Folder left;
        left.listing.directory = "left";
        left.listing.audioFiles = {kA};
        left.name = QStringLiteral("left");
        left.excluded = true;
        return {root, left};
    }

private Q_SLOTS:
    // Each folder is checked as its conversion proposes, and nothing is converted until chosen.
    void the_folders_are_checked_as_proposed() {
        VoiceBankTranscodingDialog dialog;
        dialog.setFolders("bank", folders());
        QCOMPARE(dialog.folderCount(), 2);
        QVERIFY(dialog.folderText(1).contains(QStringLiteral("left")));
        QCOMPARE(dialog.nameConversion(0).original(),
                 kit::TextCodec(QStringLiteral("Shift_JIS")).name());
        QCOMPARE(dialog.checkedNames(0), QStringList{kA});
        QCOMPARE(dialog.method(), VoiceBankTranscodingDialog::KeepNames);
        QVERIFY(dialog.textCharset(0).isEmpty());
        const auto conversion = dialog.conversion();
        QVERIFY(conversion.charsets.isEmpty());
        QVERIFY(conversion.names.isEmpty());
    }

    // The text files of a folder that was read are converted into another encoding.
    void the_text_files_are_converted_if_chosen() {
        VoiceBankTranscodingDialog dialog;
        dialog.setFolders("bank", folders());
        dialog.setTextCharset(0, QStringLiteral("Shift_JIS"));
        QVERIFY(dialog.conversion().charsets.isEmpty());
        dialog.setTextCharset(0, QStringLiteral("UTF-8"));
        dialog.setTextCharset(1, QStringLiteral("UTF-8"));
        QVERIFY(dialog.textCharset(1).isEmpty());
        const auto charsets = dialog.conversion().charsets;
        QCOMPARE(charsets.size(), 1);
        QCOMPARE(charsets.at(0).first, std::filesystem::path());
        QCOMPARE(charsets.at(0).second, QStringLiteral("UTF-8"));
    }

    // Other encodings check the names anew, and the checks of the user apply.
    void the_checked_names_are_converted_by_the_method() {
        VoiceBankTranscodingDialog dialog;
        dialog.setFolders("bank", folders());
        dialog.setChecked(0, kI, true);
        QCOMPARE(dialog.checkedNames(0), (QStringList{kA, kI}));
        dialog.setFileNameEncodings(0, QStringLiteral("Shift_JIS"), QStringLiteral("GBK"));
        QCOMPARE(dialog.checkedNames(0), (QStringList{kA, kI}));
        dialog.setFileNameEncodings(0, QStringLiteral("Big5"), QStringLiteral("GBK"));
        QCOMPARE(dialog.nameConversion(0).original(),
                 kit::TextCodec(QStringLiteral("Big5")).name());
        dialog.setFileNameEncodings(0, QStringLiteral("Shift_JIS"), QStringLiteral("GBK"));
        QCOMPARE(dialog.checkedNames(0), QStringList{kA});

        // A folder that was not read is renamed, and not read as misdecoded.
        dialog.setMethod(VoiceBankTranscodingDialog::ReadAsMisdecoded);
        auto names = dialog.conversion().names;
        QCOMPARE(names.size(), 1);
        QCOMPARE(names.at(0).first.directory(), std::filesystem::path());
        QCOMPARE(names.at(0).second, QStringList{kA});
        dialog.setMethod(VoiceBankTranscodingDialog::Rename);
        names = dialog.conversion().names;
        QCOMPARE(names.size(), 2);
        QCOMPARE(names.at(1).first.directory(), std::filesystem::path("left"));
        QCOMPARE(names.at(1).second, QStringList{kA});
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_VoiceBankTranscodingDialog test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_VoiceBankTranscodingDialog.moc"
