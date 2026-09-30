#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <helloutau/Editor/AppSettings.h>

using namespace hello::daw;

class test_AppSettings : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void unset_values_are_empty_except_the_export_encoding() {
        QTemporaryDir dir;
        const AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        QVERIFY(settings.utauDirectory().empty());
        QVERIFY(settings.resampler().isEmpty());
        QVERIFY(settings.wavtool().isEmpty());
        QCOMPARE(settings.ustExportCharset(), QStringLiteral("UTF-8"));
        QCOMPARE(settings.playbackMode(), AppSettings::Prerender);
    }

    // The latest first, each once, at most recentFileCount, whole paths in any script
    void recent_files_are_kept_latest_first() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
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
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
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
        const auto file = dir.filePath(QStringLiteral("settings.json"));
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

    // The file keeps its groups: the settings of the application under engines, playback,
    // files and commandPalette, and a group empties away with its last value.
    void the_file_keeps_its_groups() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        {
            AppSettings settings(file);
            settings.setResampler(QStringLiteral("r.exe"));
            settings.setWavtool(QStringLiteral("w.exe"));
            settings.setPlaybackMode(AppSettings::Realtime);
            settings.addRecentFile(std::filesystem::path(u"C:/a.usth"));
        }
        const auto root = readFile(file);
        QCOMPARE(
            root.value(QStringLiteral("engines")).toObject().value(QStringLiteral("resampler")),
            QJsonValue(QStringLiteral("r.exe")));
        QCOMPARE(root.value(QStringLiteral("playback")).toObject().value(QStringLiteral("mode")),
                 QJsonValue(QStringLiteral("realtime")));
        QCOMPARE(root.value(QStringLiteral("files")).toObject().value(QStringLiteral("recent")),
                 QJsonValue(QJsonArray({QStringLiteral("C:/a.usth")})));

        AppSettings settings(file);
        settings.clearRecentFiles();
        QVERIFY(!readFile(file).contains(QStringLiteral("files")));
        QCOMPARE(settings.value(QStringLiteral("engines/wavtool")),
                 QJsonValue(QStringLiteral("w.exe")));
    }

    // Any value by its key, a path of names: set within its groups, which are created, and
    // removed with those it leaves empty.
    void values_by_key() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        {
            AppSettings settings(file);
            QVERIFY(settings.value(QStringLiteral("a/b/c")).isUndefined());
            settings.setValue(QStringLiteral("a/b/c"), 1);
            settings.setValue(QStringLiteral("a/b/d"),
                              QJsonArray({true, 2.5, QStringLiteral("x")}));
            settings.setValue(QStringLiteral("a/e"), QJsonObject({
                                                         {QStringLiteral("f"), 3}
            }));
        }
        AppSettings settings(file);
        QCOMPARE(settings.value(QStringLiteral("a/b/c")), QJsonValue(1));
        QCOMPARE(settings.value(QStringLiteral("a/b/d")),
                 QJsonValue(QJsonArray({true, 2.5, QStringLiteral("x")})));
        QCOMPARE(settings.value(QStringLiteral("a/e/f")), QJsonValue(3));
        // A value is no group to look into.
        QVERIFY(settings.value(QStringLiteral("a/b/c/g")).isUndefined());

        settings.setValue(QStringLiteral("a/b/c"), QJsonValue());
        settings.setValue(QStringLiteral("a/b/d"), QJsonValue::Undefined);
        QVERIFY(settings.value(QStringLiteral("a/b")).isUndefined());
        settings.setValue(QStringLiteral("a/e/f"), QJsonValue());
        QVERIFY(!readFile(file).contains(QStringLiteral("a")));
    }

    // A plugin keeps its values in its own group of plugins/userData.
    void a_plugin_has_a_group() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        {
            AppSettings settings(file);
            settings.setValue(
                AppSettings::pluginKey(QStringLiteral("org.test.p")) + QStringLiteral("/count"), 7);
        }
        QCOMPARE(readFile(file)
                     .value(QStringLiteral("plugins"))
                     .toObject()
                     .value(QStringLiteral("userData"))
                     .toObject()
                     .value(QStringLiteral("org.test.p"))
                     .toObject()
                     .value(QStringLiteral("count")),
                 QJsonValue(7));
    }

    // A file that is not a JSON object is read as empty, and replaced by the next change.
    void an_unreadable_file_is_empty() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        {
            QFile out(file);
            QVERIFY(out.open(QIODevice::WriteOnly));
            out.write("[#VERSION]");
        }
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("cannot be read")));
        AppSettings settings(file);
        QVERIFY(settings.resampler().isEmpty());
        settings.setResampler(QStringLiteral("r.exe"));
        QCOMPARE(AppSettings(file).resampler(), QStringLiteral("r.exe"));
    }

private:
    static QJsonObject readFile(const QString &file) {
        QFile in(file);
        if (!in.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QJsonDocument::fromJson(in.readAll()).object();
    }
};

QTEST_APPLESS_MAIN(test_AppSettings)

#include "test_AppSettings.moc"
