#include <QtTest/QTest>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>

#include <helloutau/Editor/Dialogs/RemoveFrequencyMapsDialog.h>
#include <helloutau/Widgets/CheckAllBox.h>
#include <helloutau/Widgets/FileCheckList.h>

using namespace hello::daw;
namespace fs = std::filesystem;

namespace {

    using File = RemoveFrequencyMapsDialog::File;

    // Returns a.wav with a map and its LLSM cache, b.wav without a map, selected if bSelected,
    // and sub/c.wav with a map.
    QList<File> filesOf(bool bSelected) {
        return {
            {fs::path("C:/bank/a.wav"),     true,  {true},  false    },
            {fs::path("C:/bank/b.wav"),     false, {false}, bSelected},
            {fs::path("C:/bank/sub/c.wav"), true,  {false}, false    },
        };
    }

    const QStringList cacheNames{QStringLiteral("LLSM")};

}

class test_RemoveFrequencyMapsDialog : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Without a selection no file is checked, and the removal is disabled.
    void no_file_is_checked_without_a_selection() {
        RemoveFrequencyMapsDialog dialog(filesOf(false), "C:/bank", "mrq", cacheNames);
        QVERIFY(dialog.checkedFiles().isEmpty());
        QVERIFY(!dialog.removeButton()->isEnabled());
    }

    // With a selection, the selected files are checked.
    void the_selected_files_are_checked() {
        RemoveFrequencyMapsDialog dialog(filesOf(true), "C:/bank", "mrq", cacheNames);
        QCOMPARE(dialog.checkedFiles(), QList<fs::path>{fs::path("C:/bank/b.wav")});
        QVERIFY(dialog.removeButton()->isEnabled());
    }

    // A button checks exactly the files with a map, and the box checks every file.
    void a_button_checks_the_files_with_maps() {
        RemoveFrequencyMapsDialog dialog(filesOf(true), "C:/bank", "mrq", cacheNames);
        dialog.checkMapsButton()->click();
        QCOMPARE(dialog.checkedFiles(),
                 (QList<fs::path>{fs::path("C:/bank/a.wav"), fs::path("C:/bank/sub/c.wav")}));
        dialog.fileList()->checkAllBox()->click();
        QCOMPARE(dialog.checkedFiles().size(), 3);
    }

    // The map and each cache have a centered column, with a mark for a file present and nothing
    // for a file missing.
    void the_map_and_each_cache_have_a_column() {
        RemoveFrequencyMapsDialog dialog(filesOf(false), "C:/bank", "mrq", cacheNames);
        const auto tree = dialog.fileList()->tree();
        QCOMPARE(tree->columnCount(), 3);
        QCOMPARE(tree->headerItem()->text(2), QStringLiteral("LLSM"));
        QVERIFY(!tree->topLevelItem(0)->text(1).isEmpty());
        QVERIFY(!tree->topLevelItem(0)->text(2).isEmpty());
        QVERIFY(tree->topLevelItem(1)->text(1).isEmpty());
        QVERIFY(tree->topLevelItem(2)->text(2).isEmpty());
        for (int column = 1; column < 3; ++column) {
            QCOMPARE(tree->headerItem()->textAlignment(column), int(Qt::AlignCenter));
        }
    }
};

QTEST_MAIN(test_RemoveFrequencyMapsDialog)

#include "test_RemoveFrequencyMapsDialog.moc"
