#include <QtTest/QTest>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QTreeWidget>

#include <helloutau/Editor/Dialogs/ProcessAudioFilesDialog.h>
#include <helloutau/Widgets/CheckAllBox.h>
#include <helloutau/Widgets/FileCheckList.h>

using namespace hello;
using namespace hello::daw;
namespace fs = std::filesystem;

namespace {

    using Processor = kit::AudioFileProcessor;

    // A file of the given sample rate and channels, 16-bit, without metadata
    ProcessAudioFilesDialog::File fileOf(const char *path, int rate, int channels) {
        ProcessAudioFilesDialog::File file;
        file.path = fs::path("C:/bank") / path;
        file.inspection.sampleRate = rate;
        file.inspection.channels = channels;
        file.inspection.bitsPerSample = 16;
        return file;
    }

    QStringList checkedOf(const FileCheckList *list) {
        QStringList names;
        for (int i = 0; i < list->rowCount(); ++i) {
            if (list->isChecked(i)) {
                names.push_back(list->itemOf(i)->text(0));
            }
        }
        return names;
    }

}

class test_ProcessAudioFilesDialog : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Without a selection, the files that the options change are checked, again whenever an
    // option changes. A box checks or unchecks every file.
    void the_files_that_change_are_checked() {
        ProcessAudioFilesDialog dialog(
            {fileOf("a.wav", 44100, 1), fileOf("b.wav", 48000, 1), fileOf("c.wav", 44100, 2)},
            "C:/bank", "C:/backup");
        const auto list = dialog.fileList();
        QCOMPARE(checkedOf(list), (QStringList{"b.wav", "c.wav"}));
        QCOMPARE(dialog.checkedFiles(),
                 (QList<fs::path>{fs::path("C:/bank/b.wav"), fs::path("C:/bank/c.wav")}));
        QVERIFY(dialog.findChild<CheckAllBox *>());

        auto options = dialog.options();
        options.mono.reset();
        dialog.setOptions(options);
        QCOMPARE(checkedOf(list), QStringList{"b.wav"});
        QVERIFY(!dialog.options().mono);
    }

    // With a selection, the selected files are checked, and stay checked when an option
    // changes.
    void the_selected_files_are_checked() {
        auto files =
            QList{fileOf("a.wav", 44100, 1), fileOf("b.wav", 48000, 1), fileOf("c.wav", 44100, 2)};
        files[0].selected = true;
        ProcessAudioFilesDialog dialog(files, "C:/bank", "C:/backup");
        QCOMPARE(checkedOf(dialog.fileList()), QStringList{"a.wav"});
        auto options = dialog.options();
        options.mono.reset();
        dialog.setOptions(options);
        QCOMPARE(checkedOf(dialog.fileList()), QStringList{"a.wav"});
        dialog.checkFilesButton()->click();
        QCOMPARE(checkedOf(dialog.fileList()), QStringList{"b.wav"});
    }

    // A button checks exactly the files that the options change, after the user has checked
    // others.
    void a_button_checks_the_files_to_process() {
        ProcessAudioFilesDialog dialog(
            {fileOf("a.wav", 44100, 1), fileOf("b.wav", 48000, 1), fileOf("c.wav", 44100, 2)},
            "C:/bank", "C:/backup");
        const auto list = dialog.fileList();
        list->setChecked(0, true);
        list->setChecked(1, false);
        QCOMPARE(checkedOf(list), (QStringList{"a.wav", "c.wav"}));
        dialog.checkFilesButton()->click();
        QCOMPARE(checkedOf(list), (QStringList{"b.wav", "c.wav"}));
    }

    // Processing requires a checked file, and a folder if the backup is checked. The maps are
    // removed and the files backed up by default.
    void the_processing_requires_a_file_and_a_folder() {
        ProcessAudioFilesDialog dialog({fileOf("a.wav", 44100, 1)}, "C:/bank", "C:/backup");
        const auto ok = dialog.buttonBox()->button(QDialogButtonBox::Ok);
        QVERIFY(!ok->isEnabled());
        QVERIFY(dialog.removesFrequencyMaps());
        QCOMPARE(dialog.backupFolder(), std::optional<fs::path>("C:/backup"));

        dialog.fileList()->setChecked(0, true);
        QVERIFY(ok->isEnabled());
        dialog.backupFolderEdit()->clear();
        QVERIFY(!ok->isEnabled());
    }

    // The files and the options are the panes of a splitter, which the user resizes and cannot
    // collapse.
    void the_files_and_the_options_are_split() {
        ProcessAudioFilesDialog dialog({fileOf("a.wav", 44100, 1)}, "C:/bank", "C:/backup");
        const auto splitter = dialog.splitter();
        QCOMPARE(splitter->count(), 2);
        QVERIFY(splitter->widget(0)->isAncestorOf(dialog.fileList()));
        QVERIFY(splitter->widget(1)->isAncestorOf(dialog.backupFolderEdit()));
        QVERIFY(!splitter->childrenCollapsible());
    }

    // The options read back as set.
    void the_options_read_back() {
        ProcessAudioFilesDialog dialog({fileOf("a.wav", 44100, 1)}, "C:/bank", "C:/backup");
        Processor::Options options;
        options.sampleRate = 32000;
        options.mono = Processor::SecondChannel;
        options.sixteenBits = false;
        options.dithered = false;
        options.removingMetadata = false;
        options.plainHeader = false;
        options.removingDcOffset = true;
        options.peakLevel = -3;
        dialog.setOptions(options);
        const auto read = dialog.options();
        QCOMPARE(read.sampleRate, std::optional<int>(32000));
        QCOMPARE(read.mono, std::optional(Processor::SecondChannel));
        QVERIFY(!read.sixteenBits && !read.dithered && !read.removingMetadata && !read.plainHeader);
        QVERIFY(read.removingDcOffset);
        QCOMPARE(read.peakLevel, std::optional<double>(-3));
        QCOMPARE(checkedOf(dialog.fileList()), QStringList{"a.wav"});
    }
};

QTEST_MAIN(test_ProcessAudioFilesDialog)

#include "test_ProcessAudioFilesDialog.moc"
