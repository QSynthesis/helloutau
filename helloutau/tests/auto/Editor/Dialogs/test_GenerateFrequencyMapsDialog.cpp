#include <QtTest/QTest>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>

#include <helloutau/Editor/Dialogs/GenerateFrequencyMapsDialog.h>
#include <helloutau/Widgets/CheckAllBox.h>
#include <helloutau/Widgets/FileCheckList.h>

using namespace hello::daw;
namespace fs = std::filesystem;

namespace {

    using File = GenerateFrequencyMapsDialog::File;

    // Returns a.wav with a map and its LLSM cache, selected if aSelected, b.wav without a map,
    // and sub/c.wav without a map, selected if cSelected.
    QList<File> filesOf(bool aSelected, bool cSelected) {
        return {
            {fs::path("C:/bank/a.wav"),     true,  {true},  aSelected},
            {fs::path("C:/bank/b.wav"),     false, {false}, false    },
            {fs::path("C:/bank/sub/c.wav"), false, {false}, cSelected},
        };
    }

    const QStringList cacheNames{QStringLiteral("LLSM")};

    constexpr auto mapsAndCaches = FrequencyMapSettings::MapsAndCaches;
    constexpr auto cachesOnly = FrequencyMapSettings::CachesOnly;

}

class test_GenerateFrequencyMapsDialog : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Without a selection, the files without a map are checked.
    void the_missing_maps_are_checked_without_a_selection() {
        GenerateFrequencyMapsDialog dialog(filesOf(false, false), "C:/bank", "mrq", cacheNames,
                                           mapsAndCaches);
        QCOMPARE(dialog.checkedFiles(),
                 (QList<fs::path>{fs::path("C:/bank/b.wav"), fs::path("C:/bank/sub/c.wav")}));
        QCOMPARE(dialog.fileList()->rowCount(), 3);
    }

    // With a selection, the selected files are checked, whether they have a map or not.
    void the_selected_files_are_checked() {
        GenerateFrequencyMapsDialog dialog(filesOf(true, true), "C:/bank", "mrq", cacheNames,
                                           mapsAndCaches);
        QCOMPARE(dialog.checkedFiles(),
                 (QList<fs::path>{fs::path("C:/bank/a.wav"), fs::path("C:/bank/sub/c.wav")}));
    }

    // A button checks exactly the files without a map, and the box checks every file.
    void a_button_checks_the_missing_maps() {
        GenerateFrequencyMapsDialog dialog(filesOf(true, false), "C:/bank", "mrq", cacheNames,
                                           mapsAndCaches);
        QCOMPARE(dialog.checkedFiles(), QList<fs::path>{fs::path("C:/bank/a.wav")});
        dialog.checkMissingButton()->click();
        QCOMPARE(dialog.checkedFiles(),
                 (QList<fs::path>{fs::path("C:/bank/b.wav"), fs::path("C:/bank/sub/c.wav")}));
        dialog.fileList()->checkAllBox()->click();
        QCOMPARE(dialog.checkedFiles().size(), 3);
    }

    // Each analysis cache has a column after the column of the map, named by the cache, which
    // shows the presence of the cache as the column of the map shows the presence of the map.
    void each_cache_has_a_column() {
        GenerateFrequencyMapsDialog dialog(filesOf(false, false), "C:/bank", "dio",
                                           {QStringLiteral("STAR"), QStringLiteral("PLATINUM")},
                                           mapsAndCaches);
        const auto tree = dialog.fileList()->tree();
        QCOMPARE(tree->columnCount(), 4);
        QCOMPARE(tree->headerItem()->text(2), QStringLiteral("STAR"));
        QCOMPARE(tree->headerItem()->text(3), QStringLiteral("PLATINUM"));
        // The columns of the marks are centered, their headers included.
        for (int column = 1; column < 4; ++column) {
            QCOMPARE(tree->headerItem()->textAlignment(column), int(Qt::AlignCenter));
            QCOMPARE(tree->topLevelItem(0)->textAlignment(column), int(Qt::AlignCenter));
        }
        const auto a = tree->topLevelItem(0);
        const auto b = tree->topLevelItem(1);
        // A mark for a file present, nothing for a file missing
        QVERIFY(!a->text(1).isEmpty());
        QVERIFY(b->text(1).isEmpty());
        QCOMPARE(a->text(2), a->text(1));
        QCOMPARE(b->text(2), b->text(1));
        // A cache beyond the presence given is missing.
        QCOMPARE(a->text(3), b->text(1));
    }

    // The generation requires a checked file.
    void the_generation_requires_a_file() {
        GenerateFrequencyMapsDialog dialog(filesOf(false, false), "C:/bank", "mrq", cacheNames,
                                           cachesOnly);
        const auto ok = dialog.buttonBox()->button(QDialogButtonBox::Ok);
        QVERIFY(ok->isEnabled());
        dialog.fileList()->checkWhere([](int) { return false; });
        QVERIFY(!ok->isEnabled());
        dialog.fileList()->setChecked(0, true);
        QVERIFY(ok->isEnabled());
    }

    // The regeneration is initially the given one, and follows the check box.
    void the_box_chooses_the_regeneration() {
        GenerateFrequencyMapsDialog dialog(filesOf(false, false), "C:/bank", "mrq", cacheNames,
                                           cachesOnly);
        const auto box = dialog.removeMapsOnRegenerationBox();
        QVERIFY(!box->isHidden());
        QVERIFY(!box->isChecked());
        QCOMPARE(dialog.regeneration(), cachesOnly);
        box->setChecked(true);
        QCOMPARE(dialog.regeneration(), mapsAndCaches);
    }

    // Without analysis caches, the box is hidden and the maps are removed.
    void the_box_is_hidden_without_caches() {
        GenerateFrequencyMapsDialog dialog(filesOf(false, false), "C:/bank", "frq", {}, cachesOnly);
        QVERIFY(dialog.removeMapsOnRegenerationBox()->isHidden());
        QCOMPARE(dialog.regeneration(), mapsAndCaches);
    }
};

QTEST_MAIN(test_GenerateFrequencyMapsDialog)

#include "test_GenerateFrequencyMapsDialog.moc"
