#include <filesystem>

#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <helloutau/Editor/UtauFolderSearch.h>

using namespace hello::daw;
namespace fs = std::filesystem;

class test_UtauFolderSearch : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Each fixed disk has three common folders, and the local data folder one more.
    void the_common_locations_cover_each_drive() {
        const auto folders = UtauFolderSearch::commonLocations(
            {fs::path(u"C:/"), fs::path(u"D:/")}, fs::path(u"C:/Users/a/AppData/Local"));
        QCOMPARE(folders, (QList<fs::path>{
                              fs::path(u"C:/Program Files (x86)/UTAU"),
                              fs::path(u"C:/Program Files/UTAU"),
                              fs::path(u"C:/UTAU"),
                              fs::path(u"D:/Program Files (x86)/UTAU"),
                              fs::path(u"D:/Program Files/UTAU"),
                              fs::path(u"D:/UTAU"),
                              fs::path(u"C:/Users/a/AppData/Local/Programs/UTAU"),
                          }));
        QCOMPARE(UtauFolderSearch::commonLocations({}, {}), QList<fs::path>());
    }

    void the_wine_locations_are_in_drive_c() {
        QCOMPARE(UtauFolderSearch::wineLocations(fs::path(u"/home/a/.wine")),
                 (QList<fs::path>{fs::path(u"/home/a/.wine/drive_c/Program Files (x86)/UTAU"),
                                  fs::path(u"/home/a/.wine/drive_c/Program Files/UTAU"),
                                  fs::path(u"/home/a/.wine/drive_c/UTAU")}));
    }

    // The names of the run history are paths, followed by a suffix in the MuiCache. Only
    // utau.exe counts, in any letter case, and each folder once.
    void the_run_history_yields_the_folders_of_utau() {
        const QStringList names{
            QStringLiteral("E:\\UTAU\\utau.exe.FriendlyAppName"),
            QStringLiteral("E:\\UTAU\\utau.exe.ApplicationCompany"),
            QStringLiteral("E:\\UTAU\\UTAU.EXE"),
            QStringLiteral("C:\\Users\\a\\Downloads\\汉化版\\UTAU\\utau.exe"),
            QStringLiteral("E:\\tools\\resampler.exe"),
            QStringLiteral("E:\\tools\\notutau.exe"),
            QStringLiteral("utau.exe"),
            QStringLiteral("no program"),
        };
        QCOMPARE(
            UtauFolderSearch::foldersOfPrograms(names),
            (QList<fs::path>{fs::path(u"E:/UTAU"), fs::path(u"C:/Users/a/Downloads/汉化版/UTAU")}));
    }

    // The version as UTAU writes it in its resource, and the product name of a translation
    void the_version_is_written_shortly() {
        QCOMPARE(UtauFolderSearch::versionText(QStringLiteral("0.04.0019"),
                                               QString::fromUtf8("歌声合成ツール UTAU")),
                 QStringLiteral("0.4.19"));
        QCOMPARE(UtauFolderSearch::versionText(QStringLiteral("0.04.0018"),
                                               QString::fromUtf8("歌声合成软件 - UTAU")),
                 QString::fromUtf8("0.4.18 (歌声合成软件 - UTAU)"));
        QCOMPARE(UtauFolderSearch::versionText(QStringLiteral("0.04.0018"), QString()),
                 QStringLiteral("0.4.18"));
        QCOMPARE(UtauFolderSearch::versionText(QStringLiteral("beta"), QString()),
                 QStringLiteral("beta"));
    }

    // A folder is a candidate only with a utau.exe, in any letter case.
    void a_candidate_holds_utau_exe() {
        QTemporaryDir dir;
        const auto root = fs::path(dir.path().toStdU16String());
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("a")));
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("b/utau.exe")));
        QFile program(dir.filePath(QStringLiteral("a/UTAU.EXE")));
        QVERIFY(program.open(QIODevice::WriteOnly));
        program.close();

        const auto candidate = UtauFolderSearch::candidateOf(root / u"a", UtauFolderSearch::Wine);
        QVERIFY(candidate);
        QCOMPARE(candidate->folder, root / u"a");
        QCOMPARE(candidate->source, UtauFolderSearch::Wine);
        // An empty file has no version resource.
        QCOMPARE(candidate->version, QString());
        // A folder named utau.exe is no program.
        QVERIFY(!UtauFolderSearch::candidateOf(root / u"b", UtauFolderSearch::Wine));
        QVERIFY(!UtauFolderSearch::candidateOf(root / u"missing", UtauFolderSearch::Wine));
    }

    // The search of this system ends with finished() once, whatever it finds.
    void the_search_finishes() {
        UtauFolderSearch search;
        QSignalSpy found(&search, &UtauFolderSearch::found);
        QSignalSpy finished(&search, &UtauFolderSearch::finished);
        search.run();
        QCOMPARE(finished.count(), 1);
        for (const auto &arguments : std::as_const(found)) {
            const auto candidate = arguments.at(0).value<UtauFolderSearch::Candidate>();
            QVERIFY(UtauFolderSearch::candidateOf(candidate.folder, candidate.source));
        }
    }
};

QTEST_GUILESS_MAIN(test_UtauFolderSearch)

#include "test_UtauFolderSearch.moc"
