#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QDir>
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
        QVERIFY(settings.isPitchVisible());
        QVERIFY(settings.language().isEmpty());
        QVERIFY(settings.isRenderedPitchVisible());
        QVERIFY(settings.areEnvelopesVisible());
        QVERIFY(!settings.areParametersVisible());
        QCOMPARE(settings.renderThreadCount(), 0);
        QCOMPARE(settings.quantization(), 120);
        QVERIFY(settings.isRenderLogAccumulated());
        QCOMPARE(settings.renderLogLimit(), 1024 * 1024);
    }

    // The three playback modes and the thread count persist. The automatic thread count is not
    // written.
    void playback_modes_and_threads_persist() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        for (const auto mode :
             {AppSettings::Prerender, AppSettings::ThreadedPrerender, AppSettings::Realtime}) {
            {
                AppSettings settings(file);
                settings.setPlaybackMode(mode);
                settings.setRenderThreadCount(3);
            }
            const AppSettings settings(file);
            QCOMPARE(settings.playbackMode(), mode);
            QCOMPARE(settings.renderThreadCount(), 3);
        }
        {
            AppSettings settings(file);
            settings.setRenderThreadCount(0);
            QVERIFY(settings.value(QStringLiteral("playback/threads")).isUndefined());
        }
        QCOMPARE(AppSettings(file).renderThreadCount(), 0);
    }

    void quantization_persists() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        {
            AppSettings settings(file);
            settings.setQuantization(60);
        }
        QCOMPARE(AppSettings(file).quantization(), 60);
    }

    void render_log_settings_persist() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        {
            AppSettings settings(file);
            settings.setRenderLogAccumulated(false);
            settings.setRenderLogLimit(4 * 1024 * 1024);
        }
        const AppSettings settings(file);
        QVERIFY(!settings.isRenderLogAccumulated());
        QCOMPARE(settings.renderLogLimit(), 4 * 1024 * 1024);
    }

    // Recent files are stored most recent first, without duplicates, at most recentFileCount,
    // as full paths in any script.
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

    void existing_recent_file_duplicates_are_hidden() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        settings.setValue(QStringLiteral("files/recent"),
                          QJsonArray{QStringLiteral("C:/songs/a.usth"),
                                     QStringLiteral("C:/songs/a.usth")});
        QCOMPARE(settings.recentFiles(),
                 (QList<std::filesystem::path>{std::filesystem::path(u"C:/songs/a.usth")}));
    }

    // The voice banks form a separate list, stored in the same manner as the files.
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
            settings.setPitchVisible(false);
            settings.setLanguage(QStringLiteral("zh_CN"));
            settings.setRenderedPitchVisible(false);
            settings.setEnvelopesVisible(false);
            settings.setParametersVisible(true);
        }
        const AppSettings settings(file);
        QVERIFY(!settings.isPitchVisible());
        QCOMPARE(settings.language(), QStringLiteral("zh_CN"));
        QVERIFY(!settings.isRenderedPitchVisible());
        QVERIFY(!settings.areEnvelopesVisible());
        QVERIFY(settings.areParametersVisible());
        QCOMPARE(settings.utauDirectory(), utau);
        QCOMPARE(settings.resampler(), QDir::toNativeSeparators(QStringLiteral("C:/UTAU/resampler.exe")));
        QCOMPARE(settings.wavtool(), QDir::toNativeSeparators(QStringLiteral("C:/UTAU/wavtool.exe")));
        QCOMPARE(settings.ustExportCharset(), QStringLiteral("Shift_JIS"));
        QCOMPARE(settings.playbackMode(), AppSettings::Realtime);
    }

    // The file stores the settings of the application in the groups synth tools, playback, files
    // and commandPalette. A group is removed together with its last value.
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
            root.value(QStringLiteral("synthTools")).toObject().value(QStringLiteral("resampler")),
            QJsonValue(QStringLiteral("r.exe")));
        QCOMPARE(root.value(QStringLiteral("playback")).toObject().value(QStringLiteral("mode")),
                 QJsonValue(QStringLiteral("realtime")));
        QCOMPARE(root.value(QStringLiteral("files")).toObject().value(QStringLiteral("recent")),
                 QJsonValue(QJsonArray{QDir::toNativeSeparators(QStringLiteral("C:/a.usth"))}));

        AppSettings settings(file);
        settings.clearRecentFiles();
        settings.sync();
        QVERIFY(!readFile(file).contains(QStringLiteral("files")));
        QCOMPARE(settings.value(QStringLiteral("synthTools/wavtool")),
                 QJsonValue(QStringLiteral("w.exe")));
    }

    // Any value is accessible by its key, a path of names. Setting a value creates the enclosing
    // groups. Removing a value removes each group that the removal leaves empty.
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
        // A value is not a group, and a key below it has no value.
        QVERIFY(settings.value(QStringLiteral("a/b/c/g")).isUndefined());

        settings.setValue(QStringLiteral("a/b/c"), QJsonValue());
        settings.setValue(QStringLiteral("a/b/d"), QJsonValue::Undefined);
        QVERIFY(settings.value(QStringLiteral("a/b")).isUndefined());
        settings.setValue(QStringLiteral("a/e/f"), QJsonValue());
        settings.sync();
        QVERIFY(!readFile(file).contains(QStringLiteral("a")));
    }

    // The changes are written once the event loop runs, the changes of one pass in one write, or
    // immediately by sync().
    void changes_are_written_once_the_loop_runs() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        AppSettings settings(file);
        settings.setResampler(QStringLiteral("r.exe"));
        settings.setWavtool(QStringLiteral("w.exe"));
        QVERIFY(!QFile::exists(file));
        QTRY_VERIFY(QFile::exists(file));
        const auto synthTools = readFile(file).value(QStringLiteral("synthTools")).toObject();
        QCOMPARE(synthTools.value(QStringLiteral("resampler")),
                 QJsonValue(QStringLiteral("r.exe")));
        QCOMPARE(synthTools.value(QStringLiteral("wavtool")), QJsonValue(QStringLiteral("w.exe")));

        settings.setResampler(QStringLiteral("s.exe"));
        settings.sync();
        QCOMPARE(readFile(file)
                     .value(QStringLiteral("synthTools"))
                     .toObject()
                     .value(QStringLiteral("resampler")),
                 QJsonValue(QStringLiteral("s.exe")));
    }

    // A file that does not contain a JSON object is read as empty and replaced by the next
    // change.
    void an_unreadable_file_is_empty() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        {
            QFile out(file);
            QVERIFY(out.open(QIODevice::WriteOnly));
            out.write("[#VERSION]");
        }
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("could not be read")));
        AppSettings settings(file);
        QVERIFY(settings.resampler().isEmpty());
        settings.setResampler(QStringLiteral("r.exe"));
        settings.sync();
        QCOMPARE(AppSettings(file).resampler(), QStringLiteral("r.exe"));
    }


    // The settings offer the system language, English and Simplified Chinese, the last two
    // named in themselves.
    void the_languages_are_offered_in_themselves() {
        const auto languages = AppSettings::languages();
        QCOMPARE(languages.size(), 3);
        QVERIFY(languages[0].first.isEmpty());
        QCOMPARE(languages[1], (std::pair{QStringLiteral("en"), QStringLiteral("English")}));
        QCOMPARE(languages[2], (std::pair{QStringLiteral("zh_CN"), QStringLiteral("简体中文")}));
        QCOMPARE(AppSettings::localeOf(QString()), QLocale::system());
        QCOMPARE(AppSettings::localeOf(QStringLiteral("zh_CN")).language(), QLocale::Chinese);
    }

    // The user directory is the directory of the settings file unless it is given, and holds the
    // voice folder of HelloUtau.
    void the_user_directory_holds_the_voice_folder() {
        QTemporaryDir dir;
        const auto settingsFile = dir.filePath(QStringLiteral("settings.json"));
        const auto directory = std::filesystem::path(dir.path().toStdU16String());
        QCOMPARE(AppSettings(settingsFile).userDirectory(), directory);

        const auto user = directory / u"user";
        const AppSettings settings(settingsFile, QString::fromStdU16String(user.u16string()));
        QCOMPARE(settings.userDirectory(), user);
        QCOMPARE(settings.voiceFolder(), user / u"Singers");
    }

    // The voice folder of HelloUtau comes first, then the voice directory of UTAU if the UTAU
    // folder exists. A relative VoiceDir is resolved against the UTAU folder only if it exists
    // and the setting says so, and against the directory of the program otherwise.
    void the_voice_locations_follow_the_utau_folder() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        const auto program =
            std::filesystem::path(QCoreApplication::applicationDirPath().toStdU16String());
        const auto voice = std::filesystem::path(dir.path().toStdU16String()) / u"Singers";
        QCOMPARE(settings.voiceFolder(), voice);

        auto locations = settings.voiceLocations();
        QCOMPARE(locations.voiceFolders, std::vector<std::filesystem::path>{voice});
        QCOMPARE(locations.relativeBase, program);

        const auto utau = std::filesystem::path(dir.path().toStdU16String()) / u"utau";
        settings.setUtauDirectory(utau);
        QCOMPARE(settings.voiceLocations().voiceFolders.size(), size_t(1));

        std::filesystem::create_directories(utau);
        locations = settings.voiceLocations();
        QCOMPARE(locations.voiceFolders,
                 (std::vector<std::filesystem::path>{voice, utau / u"voice"}));
        QCOMPARE(locations.relativeBase, utau);

        settings.setRelativeVoiceDirInUtau(false);
        QCOMPARE(settings.voiceLocations().relativeBase, program);
    }

    // The portamento settings of Pitch Control are stored as a whole, and the vibrato with its
    // preset.
    void the_pitch_control_defaults_persist() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        {
            const AppSettings settings(file);
            QCOMPARE(settings.pitchControlPortamento(), hello::kit::PortamentoSettings());
            QVERIFY(settings.pitchControlVibrato() == hello::kit::Vibrato::utauDefault());
            QCOMPARE(settings.pitchControlVibratoPreset(), 0);
        }
        hello::kit::PortamentoSettings portamento;
        portamento.mode = hello::kit::PortamentoSettings::AddPoints;
        portamento.count = 4;
        portamento.evenlyDistributed = false;
        auto vibrato = hello::kit::Vibrato::utauDefault();
        vibrato.period = 150;
        {
            AppSettings settings(file);
            settings.setPitchControlPortamento(portamento);
            settings.setPitchControlVibrato(vibrato);
            settings.setPitchControlVibratoPreset(2);
        }
        const AppSettings settings(file);
        QCOMPARE(settings.pitchControlPortamento(), portamento);
        QVERIFY(settings.pitchControlVibrato() == vibrato);
        QCOMPARE(settings.pitchControlVibratoPreset(), 2);
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

QTEST_GUILESS_MAIN(test_AppSettings)

#include "test_AppSettings.moc"
