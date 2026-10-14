#include <algorithm>

#include <QtTest/QTest>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSplitter>

#include <helloutau/Editor/Dialogs/RepairFrequencyMapsDialog.h>
#include <helloutau/Widgets/CheckAllBox.h>

using namespace hello;
using namespace hello::daw;
namespace fs = std::filesystem;

namespace {

    // A format whose maps of a folder are in desc.map, with the cache a.wav.cache
    class Format : public kit::FrequencyFormat {
    public:
        QString id() const override {
            return QStringLiteral("map");
        }
        QString name() const override {
            return QStringLiteral("map");
        }
        QStringList resamplerPatterns() const override {
            return {};
        }
        QList<bool> mapsExist(const fs::path &, const QList<fs::path> &wavs) const override {
            return QList<bool>(wavs.size(), true);
        }
        std::optional<kit::FrequencyMap> read(const fs::path &, int,
                                              kit::DiagnosticList &) const override {
            return std::nullopt;
        }
        QList<AnalysisCache> analysisCaches() const override {
            return {
                {QStringLiteral("CACHE"), AnalysisCache::AppendedSuffix, QStringLiteral(".cache")},
            };
        }
    };

    bool checkable(const QListWidgetItem *item) {
        return item->flags().testFlag(Qt::ItemIsUserCheckable);
    }

}

class test_RepairFrequencyMapsDialog : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // A damaged file is checked. An undamaged file that holds the maps of several audio files
    // cannot be checked, nor can a missing file, and an undamaged file of a single audio file is
    // checkable but not checked. The checked files are returned by folder, and the repair
    // requires a checked file.
    void the_damaged_files_are_checked() {
        const fs::path root("C:/bank");
        RepairFrequencyMapsDialog::Files files;
        files[root] = {
            {"desc.map",
             -1,
             {"a.wav", "b.wav"},
             kit::FrequencyFormat::MapFileDamage{"desc.map", QStringLiteral("cut"), true}       },
            {"a.wav.cache",
             0,                {"a.wav"},
             kit::FrequencyFormat::MapFileDamage{"a.wav.cache", QStringLiteral("orphan"), false}},
            {"b.wav.cache", 0, {"b.wav"}, std::nullopt                                          },
        };
        files[root / "sub"] = {
            {"desc.map", -1, {"c.wav", "d.wav"}, std::nullopt},
        };
        files[root / "two"] = {
            {"desc.map", -1, {"e.wav", "f.wav"}, std::nullopt, false},
        };
        const Format format;
        RepairFrequencyMapsDialog dialog(format, root, files);
        QCOMPARE(dialog.columnCount(), 2);

        const auto maps = dialog.list(0);
        QCOMPARE(maps->count(), 3);
        QVERIFY(checkable(maps->item(0)));
        QCOMPARE(maps->item(0)->checkState(), Qt::Checked);
        QVERIFY(maps->item(0)->text().contains(QStringLiteral("cut")));
        QVERIFY(!checkable(maps->item(1)));
        QVERIFY(maps->item(1)->text().contains(QStringLiteral("sub")));
        // A missing file is listed with its absence below its name.
        QVERIFY(!checkable(maps->item(2)));
        QVERIFY(maps->item(2)->text().contains(QStringLiteral("two")));
        QVERIFY(maps->item(2)->text().contains(QLatin1Char('\n')));
        QVERIFY(maps->item(2)->text() !=
                maps->item(1)->text().replace(QStringLiteral("sub"), QStringLiteral("two")));

        const auto caches = dialog.list(1);
        QCOMPARE(caches->count(), 2);
        QCOMPARE(caches->item(0)->checkState(), Qt::Checked);
        QVERIFY(checkable(caches->item(1)));
        QCOMPARE(caches->item(1)->checkState(), Qt::Unchecked);

        auto checked = dialog.checkedFiles();
        QCOMPARE(checked.size(), size_t(1));
        QCOMPARE(checked.at(root).size(), 2);
        QCOMPARE(checked.at(root).at(0).path, fs::path("desc.map"));
        QCOMPARE(checked.at(root).at(1).path, fs::path("a.wav.cache"));
        QVERIFY(dialog.repairButton()->isEnabled());
        const auto summary = dialog.summary()->text();

        maps->item(0)->setCheckState(Qt::Unchecked);
        caches->item(0)->setCheckState(Qt::Unchecked);
        QVERIFY(dialog.checkedFiles().empty());
        QVERIFY(!dialog.repairButton()->isEnabled());
        QVERIFY(dialog.summary()->text() != summary);

        caches->item(1)->setCheckState(Qt::Checked);
        checked = dialog.checkedFiles();
        QCOMPARE(checked.at(root).size(), 1);
        QCOMPARE(checked.at(root).at(0).path, fs::path("b.wav.cache"));
        QVERIFY(dialog.repairButton()->isEnabled());
    }

    // Each column has a box that checks or unchecks all of its checkable files, which the
    // summary and the checked files follow.
    void a_column_is_checked_at_once() {
        const fs::path root("C:/bank");
        RepairFrequencyMapsDialog::Files files;
        files[root] = {
            {"a.wav.cache", 0, {"a.wav"}, std::nullopt},
            {"b.wav.cache", 0, {"b.wav"}, std::nullopt},
        };
        const Format format;
        RepairFrequencyMapsDialog dialog(format, root, files);
        const auto boxes = dialog.findChildren<CheckAllBox *>();
        QCOMPARE(boxes.size(), dialog.columnCount());
        const auto caches = dialog.list(1);
        const auto box = std::find_if(boxes.cbegin(), boxes.cend(),
                                      [](const CheckAllBox *b) { return b->isEnabled(); });
        QVERIFY(box != boxes.cend());
        QVERIFY(!dialog.repairButton()->isEnabled());

        (*box)->click();
        QCOMPARE(caches->item(0)->checkState(), Qt::Checked);
        QCOMPARE(caches->item(1)->checkState(), Qt::Checked);
        QCOMPARE(dialog.checkedFiles().at(root).size(), 2);
        QVERIFY(dialog.repairButton()->isEnabled());

        (*box)->click();
        QVERIFY(dialog.checkedFiles().empty());
        QVERIFY(!dialog.repairButton()->isEnabled());
    }

    // The columns are the panes of a splitter, which the user resizes and cannot collapse.
    void the_columns_are_in_a_splitter() {
        const fs::path root("C:/bank");
        RepairFrequencyMapsDialog::Files files;
        files[root] = {
            {"a.wav.cache", 0, {"a.wav"}, std::nullopt},
        };
        const Format format;
        RepairFrequencyMapsDialog dialog(format, root, files);
        const auto splitter = dialog.findChild<QSplitter *>();
        QVERIFY(splitter);
        QCOMPARE(splitter->count(), dialog.columnCount());
        QVERIFY(!splitter->childrenCollapsible());
        for (int column = 0; column < dialog.columnCount(); ++column) {
            QVERIFY(splitter->widget(column)->isAncestorOf(dialog.list(column)));
        }
    }
};

QTEST_MAIN(test_RepairFrequencyMapsDialog)
#include "test_RepairFrequencyMapsDialog.moc"
