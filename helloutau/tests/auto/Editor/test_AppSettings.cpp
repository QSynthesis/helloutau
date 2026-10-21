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
    // The roots are the classic extensions of the user directory, the data folder of UTAU and
    // the UTAU folder, which the caller may give before it is applied. An empty folder yields
    // an empty root.
    void the_classic_roots_follow_the_folders() {
        QTemporaryDir dir;
        const auto user = dir.filePath(QStringLiteral("user"));
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")), user);
        const auto data =
            std::filesystem::path(dir.filePath(QStringLiteral("data")).toStdU16String());
        UtauSettings::setDataDirectory(data);
        const auto utau =
            std::filesystem::path(dir.filePath(QStringLiteral("utau")).toStdU16String());
        settings.utau().setDirectory(utau);

        QCOMPARE(settings.classicRoot(AppSettings::HelloUtauRoot),
                 std::filesystem::path(user.toStdU16String()) / u"ClassicExtensions");
        QCOMPARE(settings.classicRoot(AppSettings::UtauDataRoot), data);
        QCOMPARE(settings.classicRoot(AppSettings::UtauRoot), utau);
        const auto edited = std::filesystem::path(u"edited");
        QCOMPARE(settings.classicRoot(AppSettings::UtauRoot, edited), edited);
        QVERIFY(settings.classicRoot(AppSettings::UtauRoot, {}).empty());

        UtauSettings::setDataDirectory(std::filesystem::path());
        QVERIFY(settings.classicRoot(AppSettings::UtauDataRoot).empty());
        UtauSettings::setDataDirectory(std::nullopt);
    }

    // The read folders are voice and plugins of each root and the extensions of the user
    // directory. A root that does not apply has none. The written folders are those of the
    // classic extensions.
    void the_checked_folders_follow_the_roots() {
        QTemporaryDir dir;
        const auto user =
            std::filesystem::path(dir.filePath(QStringLiteral("user")).toStdU16String());
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")),
                             QString::fromStdU16String(user.u16string()));
        const auto data =
            std::filesystem::path(dir.filePath(QStringLiteral("data")).toStdU16String());
        UtauSettings::setDataDirectory(data);
        const auto classic = user / u"ClassicExtensions";
        const std::vector<std::filesystem::path> withoutUtau{
            classic / u"voice",
            classic / u"plugins",
            data / u"voice",
            data / u"plugins",
            user / u"Extensions" / u"plugins",
            user / u"Extensions" / u"translations",
        };
        QCOMPARE(settings.readFolders(), withoutUtau);

        const auto utau =
            std::filesystem::path(dir.filePath(QStringLiteral("utau")).toStdU16String());
        settings.utau().setDirectory(utau);
        auto withUtau = withoutUtau;
        withUtau.insert(withUtau.begin() + 4, {utau / u"voice", utau / u"plugins"});
        QCOMPARE(settings.readFolders(), withUtau);
        QCOMPARE(settings.writtenFolders(),
                 (std::vector<std::filesystem::path>{classic / u"voice", classic / u"plugins"}));
        UtauSettings::setDataDirectory(std::nullopt);
    }

    void unset_values_are_empty_except_the_export_encoding() {
        QTemporaryDir dir;
        const AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        QVERIFY(settings.utau().directory().empty());
        QVERIFY(settings.rendering().resampler().isEmpty());
        QVERIFY(settings.rendering().wavtool().isEmpty());
        QCOMPARE(settings.files().ustExportCharset(), QStringLiteral("UTF-8"));
        QCOMPARE(settings.rendering().playbackMode(), RenderingSettings::Prerender);
        QVERIFY(settings.view().isPitchVisible());
        QVERIFY(settings.appearance().language().isEmpty());
        QVERIFY(settings.view().isRenderedPitchVisible());
        QVERIFY(settings.view().areEnvelopesVisible());
        QVERIFY(!settings.view().areParametersVisible());
        QCOMPARE(settings.rendering().threadCount(), 0);
        QCOMPARE(settings.view().quantization(), 120);
        QCOMPARE(settings.view().playheadFollow(), ViewSettings::PageFollow);
        QVERIFY(settings.rendering().isLogAccumulated());
        QCOMPARE(settings.rendering().logLimit(), 1024 * 1024);
    }

    // The three playback modes and the thread count persist. The automatic thread count is not
    // written.
    void playback_modes_and_threads_persist() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        for (const auto mode : {RenderingSettings::Prerender, RenderingSettings::ThreadedPrerender,
                                RenderingSettings::Realtime}) {
            {
                AppSettings settings(file);
                settings.rendering().setPlaybackMode(mode);
                settings.rendering().setThreadCount(3);
            }
            const AppSettings settings(file);
            QCOMPARE(settings.rendering().playbackMode(), mode);
            QCOMPARE(settings.rendering().threadCount(), 3);
        }
        {
            AppSettings settings(file);
            settings.rendering().setThreadCount(0);
            QVERIFY(settings.value(QStringLiteral("playback/threads")).isUndefined());
        }
        QCOMPARE(AppSettings(file).rendering().threadCount(), 0);
    }

    // The thread count, the regeneration and the removal of the caches on save of frequency maps
    // persist, independent of the rendering threads. The defaults are not written.
    void the_frequency_map_settings_persist() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        {
            const AppSettings settings(file);
            QCOMPARE(settings.frequencyMaps().threadCount(), 0);
            QCOMPARE(settings.frequencyMaps().regeneration(), FrequencyMapSettings::MapsAndCaches);
            QVERIFY(settings.frequencyMaps().removesCachesOnSave());
        }
        {
            AppSettings settings(file);
            settings.frequencyMaps().setThreadCount(2);
            settings.frequencyMaps().setRegeneration(FrequencyMapSettings::CachesOnly);
            settings.frequencyMaps().setRemovesCachesOnSave(false);
        }
        {
            AppSettings settings(file);
            QCOMPARE(settings.frequencyMaps().threadCount(), 2);
            QCOMPARE(settings.rendering().threadCount(), 0);
            QCOMPARE(settings.frequencyMaps().regeneration(), FrequencyMapSettings::CachesOnly);
            QVERIFY(!settings.frequencyMaps().removesCachesOnSave());
            settings.frequencyMaps().setThreadCount(0);
            settings.frequencyMaps().setRegeneration(FrequencyMapSettings::MapsAndCaches);
            settings.frequencyMaps().setRemovesCachesOnSave(true);
            QVERIFY(settings.value(QStringLiteral("frequencyMaps/threads")).isUndefined());
            QVERIFY(settings.value(QStringLiteral("frequencyMaps/regeneration")).isUndefined());
            QVERIFY(
                settings.value(QStringLiteral("frequencyMaps/removesCachesOnSave")).isUndefined());
        }
    }

    // The settings of the check of frequency maps persist, the note limited to the range C1 to B7.
    // The defaults are not written.
    void the_check_settings_of_frequency_maps_persist() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        {
            AppSettings settings(file);
            const auto defaults = settings.frequencyMaps().checkSettings();
            QCOMPARE(defaults.note, 60);
            QVERIFY(!defaults.allowsPitchChange);
            QVERIFY(!defaults.allowsChangeAtStart);
            QVERIFY(!defaults.allowsChangeAtEnd);
            hello::kit::FrequencyMapCheck::Settings check;
            check.note = 67;
            check.allowsPitchChange = true;
            check.allowsChangeAtEnd = true;
            settings.frequencyMaps().setCheckSettings(check);
        }
        {
            AppSettings settings(file);
            const auto check = settings.frequencyMaps().checkSettings();
            QCOMPARE(check.note, 67);
            QVERIFY(check.allowsPitchChange);
            QVERIFY(!check.allowsChangeAtStart);
            QVERIFY(check.allowsChangeAtEnd);
            settings.frequencyMaps().setCheckSettings({200});
            QCOMPARE(settings.frequencyMaps().checkSettings().note, 107);
            settings.frequencyMaps().setCheckSettings({});
            for (const auto key :
                 {"frequencyMaps/check/note", "frequencyMaps/check/allowsPitchChange",
                  "frequencyMaps/check/allowsChangeAtStart",
                  "frequencyMaps/check/allowsChangeAtEnd"}) {
                QVERIFY2(settings.value(QLatin1String(key)).isUndefined(), key);
            }
        }
    }

    // The view, the brightness and the color scheme of the editing area of frequency maps
    // persist, limited to their ranges. The defaults are not written.
    void the_editing_area_of_frequency_maps_persists() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        {
            AppSettings settings(file);
            QVERIFY(!settings.frequencyMaps().showsSpectrogram());
            QCOMPARE(settings.frequencyMaps().spectrogramBrightness(), 0);
            QCOMPARE(settings.frequencyMaps().spectrogramColorScheme(), 0);
            settings.frequencyMaps().setShowsSpectrogram(true);
            settings.frequencyMaps().setSpectrogramBrightness(-2);
            settings.frequencyMaps().setSpectrogramColorScheme(4);
        }
        {
            AppSettings settings(file);
            QVERIFY(settings.frequencyMaps().showsSpectrogram());
            QCOMPARE(settings.frequencyMaps().spectrogramBrightness(), -2);
            QCOMPARE(settings.frequencyMaps().spectrogramColorScheme(), 4);
            settings.frequencyMaps().setSpectrogramBrightness(9);
            settings.frequencyMaps().setSpectrogramColorScheme(-1);
            QCOMPARE(settings.frequencyMaps().spectrogramBrightness(), 3);
            QCOMPARE(settings.frequencyMaps().spectrogramColorScheme(), 0);
            settings.frequencyMaps().setShowsSpectrogram(false);
            settings.frequencyMaps().setSpectrogramBrightness(0);
            for (const auto key :
                 {"frequencyMaps/editor/spectrogram", "frequencyMaps/editor/brightness",
                  "frequencyMaps/editor/colorScheme"}) {
                QVERIFY2(settings.value(QLatin1String(key)).isUndefined(), key);
            }
        }
    }

    // The thread count of the general tasks persists, independent of the other thread counts.
    // The default is not written.
    void the_task_thread_count_persists() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        QCOMPARE(AppSettings(file).tasks().threadCount(), 0);
        {
            AppSettings settings(file);
            settings.tasks().setThreadCount(4);
        }
        {
            AppSettings settings(file);
            QCOMPARE(settings.tasks().threadCount(), 4);
            QCOMPARE(settings.frequencyMaps().threadCount(), 0);
            settings.tasks().setThreadCount(0);
            QVERIFY(settings.value(QStringLiteral("tasks/threads")).isUndefined());
        }
    }

    // The extraction thread count of updates persists. The default is not written.
    void the_extraction_thread_count_persists() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        QCOMPARE(AppSettings(file).updates().extractionThreadCount(),
                 UpdateSettings::defaultExtractionThreadCount);
        {
            AppSettings settings(file);
            settings.updates().setExtractionThreadCount(2);
        }
        QCOMPARE(AppSettings(file).updates().extractionThreadCount(), 2);
        {
            AppSettings settings(file);
            settings.updates().setExtractionThreadCount(
                UpdateSettings::defaultExtractionThreadCount);
            QVERIFY(settings.value(QStringLiteral("updates/extractionThreads")).isUndefined());
            settings.updates().setExtractionThreadCount(0);
            QVERIFY(settings.value(QStringLiteral("updates/extractionThreads")).isUndefined());
        }
        QCOMPARE(AppSettings(file).updates().extractionThreadCount(),
                 UpdateSettings::defaultExtractionThreadCount);
    }

    // The settings of the updates persist. The defaults are not written.
    void update_settings_persist() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        const auto time = QDateTime(QDate(2026, 10, 11), QTime(12, 30), QTimeZone::UTC);
        {
            AppSettings settings(file);
            QVERIFY(settings.updates().source().isEmpty());
            QCOMPARE(settings.updates().mode(), UpdateSettings::Notified);
            QVERIFY(!settings.updates().lastCheck().isValid());
            QVERIFY(settings.updates().skippedVersion().isEmpty());
            settings.updates().setSource(QStringLiteral("Mirror"));
            settings.updates().setMode(UpdateSettings::Automatic);
            settings.updates().setLastCheck(time);
            settings.updates().setSkippedVersion(QStringLiteral("0.1.7.0"));
        }
        {
            AppSettings settings(file);
            QCOMPARE(settings.updates().source(), QStringLiteral("Mirror"));
            QCOMPARE(settings.updates().mode(), UpdateSettings::Automatic);
            settings.updates().setMode(UpdateSettings::Manual);
            QCOMPARE(settings.updates().mode(), UpdateSettings::Manual);
            settings.updates().setMode(UpdateSettings::Notified);
            QVERIFY(settings.value(QStringLiteral("updates/mode")).isUndefined());
            QCOMPARE(settings.updates().lastCheck(), time);
            QCOMPARE(settings.updates().skippedVersion(), QStringLiteral("0.1.7.0"));
            settings.updates().setSource(QString());
            settings.updates().setSkippedVersion(QString());
            QVERIFY(settings.value(QStringLiteral("updates/source")).isUndefined());
            QVERIFY(settings.value(QStringLiteral("updates/skippedVersion")).isUndefined());
            QVERIFY(settings.updates().pendingVersion().isEmpty());
            settings.updates().setPendingInstallation(
                QStringLiteral("0.1.7.0"), QUrl(QStringLiteral("https://example.com/v0.1.7.0")));
        }
        {
            AppSettings settings(file);
            QCOMPARE(settings.updates().pendingVersion(), QStringLiteral("0.1.7.0"));
            QCOMPARE(settings.updates().pendingPage(),
                     QUrl(QStringLiteral("https://example.com/v0.1.7.0")));
            settings.updates().clearPendingInstallation();
            QVERIFY(settings.updates().pendingVersion().isEmpty());
            QVERIFY(settings.value(QStringLiteral("updates/pending")).isUndefined());
        }
    }

    void quantization_persists() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        {
            AppSettings settings(file);
            settings.view().setQuantization(60);
        }
        QCOMPARE(AppSettings(file).view().quantization(), 60);
    }

    void render_log_settings_persist() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        {
            AppSettings settings(file);
            settings.rendering().setLogAccumulated(false);
            settings.rendering().setLogLimit(4 * 1024 * 1024);
        }
        const AppSettings settings(file);
        QVERIFY(!settings.rendering().isLogAccumulated());
        QCOMPARE(settings.rendering().logLimit(), 4 * 1024 * 1024);
    }

    // Recent files are stored most recent first, without duplicates, at most recentFileCount,
    // as full paths in any script.
    void recent_files_are_kept_latest_first() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        {
            AppSettings settings(file);
            QVERIFY(settings.files().recentFiles().isEmpty());
            for (int i = 0; i < FileSettings::recentFileCount + 2; ++i) {
                settings.files().addRecentFile(std::filesystem::path(u"C:/songs/歌") /
                                               (std::to_string(i) + ".usth"));
            }
            settings.files().addRecentFile(std::filesystem::path(u"C:/songs/歌") / "5.usth");
        }
        AppSettings settings(file);
        auto files = settings.files().recentFiles();
        QCOMPARE(files.size(), FileSettings::recentFileCount);
        QCOMPARE(files.first(), std::filesystem::path(u"C:/songs/歌") / "5.usth");
        QCOMPARE(files.at(1), std::filesystem::path(u"C:/songs/歌") /
                                  (std::to_string(FileSettings::recentFileCount + 1) + ".usth"));
        QCOMPARE(files.count(files.first()), 1);

        settings.files().removeRecentFile(files.at(1));
        QCOMPARE(settings.files().recentFiles().size(), FileSettings::recentFileCount - 1);
        QVERIFY(!settings.files().recentFiles().contains(files.at(1)));
        settings.files().clearRecentFiles();
        QVERIFY(settings.files().recentFiles().isEmpty());
    }

    void existing_recent_file_duplicates_are_hidden() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        settings.setValue(
            QStringLiteral("files/recent"),
            QJsonArray{QStringLiteral("C:/songs/a.usth"), QStringLiteral("C:/songs/a.usth")});
        QCOMPARE(settings.files().recentFiles(),
                 (QList<std::filesystem::path>{std::filesystem::path(u"C:/songs/a.usth")}));
    }

    // The voice banks form a separate list, stored in the same manner as the files.
    void recent_voice_banks_are_kept_apart() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        const auto bank = std::filesystem::path(u"C:/voice/音源");
        settings.files().addRecentFile(std::filesystem::path(u"C:/songs/a.usth"));
        settings.files().addRecentVoiceBank(bank);
        settings.files().addRecentVoiceBank(std::filesystem::path(u"C:/voice/other"));
        settings.files().addRecentVoiceBank(bank);
        QCOMPARE(settings.files().recentVoiceBanks(),
                 (QList<std::filesystem::path>{bank, std::filesystem::path(u"C:/voice/other")}));
        QCOMPARE(settings.files().recentFiles().size(), 1);

        settings.files().removeRecentVoiceBank(bank);
        QCOMPARE(settings.files().recentVoiceBanks().size(), 1);
        settings.files().clearRecentVoiceBanks();
        QVERIFY(settings.files().recentVoiceBanks().isEmpty());
        QCOMPARE(settings.files().recentFiles().size(), 1);
    }

    void values_persist_in_the_file() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        const auto utau = std::filesystem::path(u"C:/UTAU/歌");
        {
            AppSettings settings(file);
            settings.utau().setDirectory(utau);
            settings.rendering().setResampler(QStringLiteral("C:/UTAU/resampler.exe"));
            settings.rendering().setWavtool(QStringLiteral("C:/UTAU/wavtool.exe"));
            settings.files().setUstExportCharset(QStringLiteral("Shift_JIS"));
            settings.rendering().setPlaybackMode(RenderingSettings::Realtime);
            settings.view().setPitchVisible(false);
            settings.appearance().setLanguage(QStringLiteral("zh_CN"));
            settings.view().setRenderedPitchVisible(false);
            settings.view().setEnvelopesVisible(false);
            settings.view().setParametersVisible(true);
            settings.view().setPlayheadFollow(ViewSettings::SmoothFollow);
        }
        const AppSettings settings(file);
        QVERIFY(!settings.view().isPitchVisible());
        QCOMPARE(settings.appearance().language(), QStringLiteral("zh_CN"));
        QVERIFY(!settings.view().isRenderedPitchVisible());
        QVERIFY(!settings.view().areEnvelopesVisible());
        QVERIFY(settings.view().areParametersVisible());
        QCOMPARE(settings.view().playheadFollow(), ViewSettings::SmoothFollow);
        QCOMPARE(settings.utau().directory(), utau);
        QCOMPARE(settings.rendering().resampler(),
                 QDir::toNativeSeparators(QStringLiteral("C:/UTAU/resampler.exe")));
        QCOMPARE(settings.rendering().wavtool(),
                 QDir::toNativeSeparators(QStringLiteral("C:/UTAU/wavtool.exe")));
        QCOMPARE(settings.files().ustExportCharset(), QStringLiteral("Shift_JIS"));
        QCOMPARE(settings.rendering().playbackMode(), RenderingSettings::Realtime);
    }

    // The file stores the settings of the application in the groups synth tools, playback, files
    // and commandPalette. A group is removed together with its last value.
    void the_file_keeps_its_groups() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        {
            AppSettings settings(file);
            settings.rendering().setResampler(QStringLiteral("r.exe"));
            settings.rendering().setWavtool(QStringLiteral("w.exe"));
            settings.rendering().setPlaybackMode(RenderingSettings::Realtime);
            settings.files().addRecentFile(std::filesystem::path(u"C:/a.usth"));
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
        settings.files().clearRecentFiles();
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
        settings.rendering().setResampler(QStringLiteral("r.exe"));
        settings.rendering().setWavtool(QStringLiteral("w.exe"));
        QVERIFY(!QFile::exists(file));
        QTRY_VERIFY(QFile::exists(file));
        const auto synthTools = readFile(file).value(QStringLiteral("synthTools")).toObject();
        QCOMPARE(synthTools.value(QStringLiteral("resampler")),
                 QJsonValue(QStringLiteral("r.exe")));
        QCOMPARE(synthTools.value(QStringLiteral("wavtool")), QJsonValue(QStringLiteral("w.exe")));

        settings.rendering().setResampler(QStringLiteral("s.exe"));
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
        QVERIFY(settings.rendering().resampler().isEmpty());
        settings.rendering().setResampler(QStringLiteral("r.exe"));
        settings.sync();
        QCOMPARE(AppSettings(file).rendering().resampler(), QStringLiteral("r.exe"));
    }


    // The settings offer the system language, English and Simplified Chinese, the last two
    // named in themselves.
    void the_languages_are_offered_in_themselves() {
        const auto languages = AppearanceSettings::languages();
        QCOMPARE(languages.size(), 3);
        QVERIFY(languages[0].first.isEmpty());
        QCOMPARE(languages[1], (std::pair{QStringLiteral("en"), QStringLiteral("English")}));
        QCOMPARE(languages[2], (std::pair{QStringLiteral("zh_CN"), QStringLiteral("简体中文")}));
        QCOMPARE(AppearanceSettings::localeOf(QString()), QLocale::system());
        QCOMPARE(AppearanceSettings::localeOf(QStringLiteral("zh_CN")).language(),
                 QLocale::Chinese);
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
        QCOMPARE(settings.voiceBanks().folder(), user / u"ClassicExtensions" / u"voice");
    }

    // %DATA% denotes the classic extensions directory of HelloUtau and then the data directory of
    // UTAU, the UTAU directory of the application data folder. %VOICE% denotes the voice directory
    // of UTAU if the UTAU folder exists. A relative VoiceDir is resolved against the UTAU folder
    // only if the folder exists and the setting enables it, and against the directory of the
    // program otherwise.
    void the_voice_locations_follow_the_utau_folder() {
        UtauSettings::setDataDirectory(std::nullopt);
        QCOMPARE(UtauSettings::dataDirectory().filename(), std::filesystem::path(u"UTAU"));

        QTemporaryDir dir;
        const auto root = std::filesystem::path(dir.path().toStdU16String());
        const auto appData = root / u"appdata" / u"UTAU";
        UtauSettings::setDataDirectory(appData);
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        const auto program =
            std::filesystem::path(QCoreApplication::applicationDirPath().toStdU16String());
        const auto classic = root / u"ClassicExtensions";
        QCOMPARE(settings.voiceBanks().folder(), classic / u"voice");

        const std::vector<std::filesystem::path> data{classic, appData};
        auto locations = settings.voiceBanks().locations();
        QCOMPARE(locations.dataFolders, data);
        QVERIFY(locations.voiceFolders.empty());
        QCOMPARE(locations.relativeBase, program);
        QCOMPARE(locations.voiceBankFolders().front(), settings.voiceBanks().folder());

        UtauSettings::setDataDirectory(std::filesystem::path());
        QCOMPARE(settings.voiceBanks().locations().dataFolders,
                 std::vector<std::filesystem::path>{classic});
        UtauSettings::setDataDirectory(appData);

        const auto utau = root / u"utau";
        settings.utau().setDirectory(utau);
        QVERIFY(settings.voiceBanks().locations().voiceFolders.empty());

        std::filesystem::create_directories(utau);
        locations = settings.voiceBanks().locations();
        QCOMPARE(locations.dataFolders, data);
        QCOMPARE(locations.voiceFolders, std::vector<std::filesystem::path>{utau / u"voice"});
        QCOMPARE(locations.relativeBase, utau);

        settings.voiceBanks().setRelativeToUtau(false);
        QCOMPARE(settings.voiceBanks().locations().relativeBase, program);
        UtauSettings::setDataDirectory(std::nullopt);
    }

    // The portamento settings of Pitch Control are stored as a whole, and the vibrato with its
    // preset.
    void the_pitch_control_defaults_persist() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.json"));
        {
            const AppSettings settings(file);
            QCOMPARE(settings.pitchControl().portamento(), hello::kit::PortamentoSettings());
            QVERIFY(settings.pitchControl().vibrato() == hello::kit::Vibrato::utauDefault());
            QCOMPARE(settings.pitchControl().vibratoPreset(), 0);
        }
        hello::kit::PortamentoSettings portamento;
        portamento.mode = hello::kit::PortamentoSettings::AddPoints;
        portamento.count = 4;
        portamento.evenlyDistributed = false;
        auto vibrato = hello::kit::Vibrato::utauDefault();
        vibrato.period = 150;
        {
            AppSettings settings(file);
            settings.pitchControl().setPortamento(portamento);
            settings.pitchControl().setVibrato(vibrato);
            settings.pitchControl().setVibratoPreset(2);
        }
        const AppSettings settings(file);
        QCOMPARE(settings.pitchControl().portamento(), portamento);
        QVERIFY(settings.pitchControl().vibrato() == vibrato);
        QCOMPARE(settings.pitchControl().vibratoPreset(), 2);
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
