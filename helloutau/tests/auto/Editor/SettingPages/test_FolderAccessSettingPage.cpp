#include <filesystem>

#include <QtCore/QDir>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>

#include <hellokit/Support/PlatformFileSystem.h>

#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/SettingPages/FolderAccessSettingPage.h>

using namespace hello;
using namespace hello::daw;

namespace fs = std::filesystem;

class test_FolderAccessSettingPage : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    fs::path userDirectory() const {
        return fs::path(m_dir.filePath(QStringLiteral("user")).toStdU16String());
    }

private Q_SLOTS:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        UtauSettings::setDataDirectory(fs::path());
    }

    void cleanupTestCase() {
        UtauSettings::setDataDirectory(std::nullopt);
    }

    // The page records the read folders when it opens and lists the record, one row for each
    // folder. Check Again checks the writing of the written folders as well.
    void the_page_lists_the_record_and_checks_again() {
        kit::FolderAccessRecord record;
        AppSettings settings(m_dir.filePath(QStringLiteral("settings.json")),
                             QString::fromStdU16String(userDirectory().u16string()));
        const auto voice = settings.writtenFolders().front();
        fs::create_directories(voice);

        FolderAccessSettingPage page(settings);
        QVERIFY(page.widget());
        const auto list = page.folderList();
        QCOMPARE(size_t(list->topLevelItemCount()), settings.readFolders().size());
        QCOMPARE(list->topLevelItem(0)->text(0),
                 QDir::toNativeSeparators(QString::fromStdU16String(voice.u16string())));
        QCOMPARE(record.entryOf(voice)->state, kit::FolderAccessRecord::Readable);

        const auto checkAgain =
            page.widget()->findChild<QPushButton *>(QStringLiteral("checkAgain"));
        QVERIFY(checkAgain);
        checkAgain->click();
        QCOMPARE(record.entryOf(voice)->state, kit::FolderAccessRecord::Writable);
        QCOMPARE(size_t(list->topLevelItemCount()), settings.readFolders().size());
    }
};

QTEST_MAIN(test_FolderAccessSettingPage)

#include "test_FolderAccessSettingPage.moc"
