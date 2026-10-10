#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>

#include <helloutau/Editor/Dialogs/RegionDialog.h>

using namespace hello;
using namespace hello::daw;

namespace {

    const kit::Region verse{QStringLiteral("Verse"), 0, 3};
    const kit::Region chorus{QStringLiteral("Chorus"), 4, 4};
    const kit::Region bridge{QStringLiteral("Bridge"), 5, 9};

}

class test_RegionDialog : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The regions are listed in the given order, with their notes numbered from 1, a single note
    // as one number.
    void the_regions_are_listed_with_their_notes() {
        RegionDialog dialog;
        dialog.setRegions({verse, chorus});
        QCOMPARE(dialog.regions(), (QList<kit::Region>{verse, chorus}));
        const auto list = dialog.list();
        QCOMPARE(list->topLevelItemCount(), 2);
        QCOMPARE(list->topLevelItem(0)->text(0), QStringLiteral("Verse"));
        QCOMPARE(list->topLevelItem(0)->text(1),
                 QStringLiteral("1") + QChar(0x2013) + QStringLiteral("4"));
        QCOMPARE(list->topLevelItem(1)->text(1), QStringLiteral("5"));
    }

    void the_buttons_need_a_current_region() {
        RegionDialog dialog;
        QVERIFY(!dialog.currentRegion());
        QVERIFY(!dialog.goToButton()->isEnabled());
        QVERIFY(!dialog.removeButton()->isEnabled());

        dialog.setRegions({verse, chorus});
        dialog.setCurrentRegion(chorus);
        QCOMPARE(dialog.currentRegion(), std::optional(chorus));
        QVERIFY(dialog.goToButton()->isEnabled());
        QVERIFY(dialog.removeButton()->isEnabled());

        // A region that is not listed does not change the current one.
        dialog.setCurrentRegion(bridge);
        QCOMPARE(dialog.currentRegion(), std::optional(chorus));
    }

    void the_buttons_and_a_double_click_request_the_current_region() {
        RegionDialog dialog;
        dialog.setRegions({verse, chorus});
        dialog.setCurrentRegion(chorus);
        QSignalSpy goTo(&dialog, &RegionDialog::goToRequested);
        QSignalSpy remove(&dialog, &RegionDialog::removeRequested);

        dialog.goToButton()->click();
        Q_EMIT dialog.list()->itemActivated(dialog.list()->topLevelItem(1), 0);
        dialog.removeButton()->click();
        QCOMPARE(goTo.size(), 2);
        QCOMPARE(goTo.at(0).at(0).value<kit::Region>(), chorus);
        QCOMPARE(goTo.at(1).at(0).value<kit::Region>(), chorus);
        QCOMPARE(remove.size(), 1);
        QCOMPARE(remove.at(0).at(0).value<kit::Region>(), chorus);
    }

    // After new regions are set, the current region stays current if listed, and otherwise the
    // region in the same row, or the last one if the list has become shorter.
    void the_current_region_survives_new_regions() {
        RegionDialog dialog;
        dialog.setRegions({verse, chorus, bridge});
        dialog.setCurrentRegion(chorus);
        dialog.setRegions({bridge, verse, chorus});
        QCOMPARE(dialog.currentRegion(), std::optional(chorus));

        // Chorus in row 2 is removed: the region in that row becomes current.
        dialog.setRegions({bridge, verse, chorus});
        dialog.setCurrentRegion(verse);
        dialog.setRegions({bridge, chorus});
        QCOMPARE(dialog.currentRegion(), std::optional(chorus));

        // The last row is removed: the new last region becomes current.
        dialog.setCurrentRegion(chorus);
        dialog.setRegions({bridge});
        QCOMPARE(dialog.currentRegion(), std::optional(bridge));
    }
};

QTEST_MAIN(test_RegionDialog)

#include "test_RegionDialog.moc"
