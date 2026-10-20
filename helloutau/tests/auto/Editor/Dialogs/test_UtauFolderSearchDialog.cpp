#include <filesystem>

#include <QtCore/QDir>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <helloutau/Editor/Dialogs/UtauFolderSearchDialog.h>

using namespace hello::daw;
namespace fs = std::filesystem;

class test_UtauFolderSearchDialog : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The candidates are listed in the order of arrival, the first selected.
    void the_candidates_are_listed_and_the_first_is_selected() {
        UtauFolderSearchDialog dialog;
        QVERIFY(!dialog.selectedFolder());
        dialog.addCandidate(
            {fs::path(u"D:/UTAU-test"), UtauFolderSearch::Installer, QStringLiteral("0.4.19")});
        dialog.addCandidate({fs::path(u"E:/UTAU"), UtauFolderSearch::RunHistory, QString()});
        QCOMPARE(dialog.selectedFolder(), std::optional(fs::path(u"D:/UTAU-test")));
        QCOMPARE(dialog.rows(),
                 (QStringList{QDir::toNativeSeparators(QStringLiteral("D:/UTAU-test")) +
                                  QStringLiteral("\t0.4.19\t") +
                                  UtauFolderSearch::sourceName(UtauFolderSearch::Installer),
                              QDir::toNativeSeparators(QStringLiteral("E:/UTAU")) +
                                  QStringLiteral("\t\t") +
                                  UtauFolderSearch::sourceName(UtauFolderSearch::RunHistory)}));
        QVERIFY(dialog.statusText().contains(QStringLiteral("2")));
    }

    // The dialog stops waiting once the search finishes or the time runs out, whichever is
    // first, and then reports the result.
    void the_dialog_stops_waiting() {
        UtauFolderSearchDialog dialog;
        dialog.setTimeout(1000);
        dialog.start();
        QVERIFY(dialog.isSearching());
        QVERIFY(dialog.statusText().contains(QStringLiteral("Searching")));
        QTRY_VERIFY_WITH_TIMEOUT(!dialog.isSearching(), 2000);
        QVERIFY(!dialog.statusText().contains(QStringLiteral("Searching")));
    }

    // A candidate that arrives after the dialog stopped waiting is not added, and the dialog
    // closes while the search may still run.
    void the_dialog_closes_during_a_search() {
        auto dialog = new UtauFolderSearchDialog();
        dialog->setTimeout(0);
        dialog->start();
        QTRY_VERIFY(!dialog->isSearching());
        const auto rows = dialog->rows().size();
        QTest::qWait(100);
        QCOMPARE(dialog->rows().size(), rows);
        delete dialog;
        QTest::qWait(100);
    }
};

QTEST_MAIN(test_UtauFolderSearchDialog)

#include "test_UtauFolderSearchDialog.moc"
