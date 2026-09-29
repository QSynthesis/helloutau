#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <helloutau/Editor/AppSettings.h>

using namespace hello::daw;

class test_AppSettings : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void unset_values_are_empty_except_the_export_encoding() {
        QTemporaryDir dir;
        const AppSettings settings(dir.filePath(QStringLiteral("settings.ini")));
        QVERIFY(settings.utauDirectory().empty());
        QVERIFY(settings.resampler().isEmpty());
        QVERIFY(settings.wavtool().isEmpty());
        QCOMPARE(settings.ustExportCharset(), QStringLiteral("UTF-8"));
        QCOMPARE(settings.playbackMode(), AppSettings::Prerender);
    }

    // The latest first, each once, at most recentFileCount, whole paths in any script
    void recent_files_are_kept_latest_first() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.ini"));
        {
            AppSettings settings(file);
            QVERIFY(settings.recentFiles().isEmpty());
            for (int i = 0; i < AppSettings::recentFileCount + 2; ++i) {
                settings.addRecentFile(std::filesystem::path(u"C:/songs/歌") /
                                       (std::to_string(i) + ".usth"));
            }
            settings.addRecentFile(std::filesystem::path(u"C:/songs/歌") / "5.usth");
        }
        AppSettings settings(file);
        auto files = settings.recentFiles();
        QCOMPARE(files.size(), AppSettings::recentFileCount);
        QCOMPARE(files.first(), std::filesystem::path(u"C:/songs/歌") / "5.usth");
        QCOMPARE(files.at(1), std::filesystem::path(u"C:/songs/歌") /
                                  (std::to_string(AppSettings::recentFileCount + 1) + ".usth"));
        QCOMPARE(files.count(files.first()), 1);

        settings.removeRecentFile(files.at(1));
        QCOMPARE(settings.recentFiles().size(), AppSettings::recentFileCount - 1);
        QVERIFY(!settings.recentFiles().contains(files.at(1)));
        settings.clearRecentFiles();
        QVERIFY(settings.recentFiles().isEmpty());
    }

    // The voice banks are a list of their own, kept as the files are.
    void recent_voice_banks_are_kept_apart() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.ini")));
        const auto bank = std::filesystem::path(u"C:/voice/音源");
        settings.addRecentFile(std::filesystem::path(u"C:/songs/a.usth"));
        settings.addRecentVoiceBank(bank);
        settings.addRecentVoiceBank(std::filesystem::path(u"C:/voice/other"));
        settings.addRecentVoiceBank(bank);
        QCOMPARE(settings.recentVoiceBanks(),
                 (QList<std::filesystem::path>{bank, std::filesystem::path(u"C:/voice/other")}));
        QCOMPARE(settings.recentFiles().size(), 1);

        settings.removeRecentVoiceBank(bank);
        QCOMPARE(settings.recentVoiceBanks().size(), 1);
        settings.clearRecentVoiceBanks();
        QVERIFY(settings.recentVoiceBanks().isEmpty());
        QCOMPARE(settings.recentFiles().size(), 1);
    }

    void values_persist_in_the_file() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.ini"));
        const auto utau = std::filesystem::path(u"C:/UTAU/歌");
        {
            AppSettings settings(file);
            settings.setUtauDirectory(utau);
            settings.setResampler(QStringLiteral("C:/UTAU/resampler.exe"));
            settings.setWavtool(QStringLiteral("C:/UTAU/wavtool.exe"));
            settings.setUstExportCharset(QStringLiteral("Shift_JIS"));
            settings.setPlaybackMode(AppSettings::Realtime);
        }
        const AppSettings settings(file);
        QCOMPARE(settings.utauDirectory(), utau);
        QCOMPARE(settings.resampler(), QStringLiteral("C:/UTAU/resampler.exe"));
        QCOMPARE(settings.wavtool(), QStringLiteral("C:/UTAU/wavtool.exe"));
        QCOMPARE(settings.ustExportCharset(), QStringLiteral("Shift_JIS"));
        QCOMPARE(settings.playbackMode(), AppSettings::Realtime);
    }
};

QTEST_APPLESS_MAIN(test_AppSettings)

#include "test_AppSettings.moc"
