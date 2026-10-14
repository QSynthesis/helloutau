#include <filesystem>
#include <optional>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <ClassicPluginHost/ClassicPlugin.h>
#include <ClassicPluginHost/ClassicPluginPreviewInstallation.h>

using namespace hello;
using namespace hello::daw;

namespace fs = std::filesystem;

using Installation = ClassicPluginPreviewInstallation;

namespace {

    // The name of a Japanese plugin, テスト, in Shift_JIS
    const QByteArray japaneseName("\x83\x65\x83\x58\x83\x67");

    fs::path pathOf(const QString &text) {
        return fs::path(text.toStdU16String());
    }

    void write(const fs::path &path, const QByteArray &bytes) {
        QFile file(QString::fromStdU16String(path.u16string()));
        if (file.open(QIODevice::WriteOnly)) {
            file.write(bytes);
        }
    }

    QByteArray read(const fs::path &path) {
        QFile file(QString::fromStdU16String(path.u16string()));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    std::optional<QJsonObject> manifestOf(const fs::path &folder) {
        if (!fs::exists(folder / "plugin.json")) {
            return std::nullopt;
        }
        return QJsonDocument::fromJson(read(folder / "plugin.json")).object();
    }

    // Returns the record of the installation in manifest, hostData/ClassicPluginHost/playinplugin.
    QJsonObject recordIn(const QJsonObject &manifest) {
        return manifest.value(QLatin1String("hostData"))
            .toObject()
            .value(QLatin1String("ClassicPluginHost"))
            .toObject()
            .value(QLatin1String("playinplugin"))
            .toObject();
    }

    // A plugin folder as PitTrace is distributed: plugin.txt in Shift_JIS with ustversion=1.00,
    // the program, and the original playinplugin.exe beside it
    fs::path pitTrace(const QTemporaryDir &dir) {
        const auto folder = pathOf(dir.path()) / "PitTrace";
        fs::create_directories(folder);
        write(folder / "plugin.txt",
              "name=" + japaneseName + "\r\nexecute=pittrace.exe\r\nustversion=1.00\r\n");
        write(folder / "pittrace.exe", "the plugin");
        write(folder / "playinplugin.exe", "the original");
        return folder;
    }

    fs::path program(const QTemporaryDir &dir, const QByteArray &content = "plugin-play") {
        const auto path = pathOf(dir.path()) / "hello-plugin-play.exe";
        write(path, content);
        return path;
    }

}

class test_ClassicPluginPreviewInstallation : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // A plugin with only a plugin.txt receives a plugin.json transcribed from it. The original
    // program is kept, and the plugin reads as before.
    void an_installation_transcribes_plugin_txt_and_keeps_the_original() {
        QTemporaryDir dir;
        const auto folder = pitTrace(dir);
        const auto playPath = program(dir);
        QVERIFY(Installation::transcribes(folder));
        kit::DiagnosticList diagnostics;
        QVERIFY(Installation::install(folder, playPath, QStringLiteral("Shift_JIS"), diagnostics));
        QVERIFY(diagnostics.isEmpty());

        QCOMPARE(read(folder / "playinplugin.exe"), QByteArray("plugin-play"));
        QCOMPARE(read(folder / "playinplugin-original.exe"), QByteArray("the original"));
        const auto manifest = manifestOf(folder);
        QVERIFY(manifest);
        QCOMPARE(manifest->value(QLatin1String("name")).toString(), QString::fromUtf8("テスト"));
        QCOMPARE(manifest->value(QLatin1String("execute")).toString(),
                 QStringLiteral("pittrace.exe"));
        QCOMPARE(manifest->value(QLatin1String("notes")).toString(), QStringLiteral("selection"));
        QVERIFY(!manifest->contains(QLatin1String("charset")));
        const auto record = recordIn(*manifest);
        QCOMPARE(record.value(QLatin1String("original")).toString(),
                 QStringLiteral("playinplugin-original.exe"));

        const auto plugin = ClassicPlugin::read(folder, diagnostics);
        QVERIFY(plugin);
        QCOMPARE(plugin->name, QString::fromUtf8("テスト"));
        QCOMPARE(plugin->ustVersion, QStringLiteral("1.00"));
        QCOMPARE(plugin->charset, ClassicPlugin::localCharset());
        QCOMPARE(Installation::stateOf(folder, playPath), Installation::Installed);
    }

    // A removal restores the original program and keeps the transcribed plugin.json without
    // the record, which reads as the plugin.txt.
    void a_removal_restores_the_program() {
        QTemporaryDir dir;
        const auto folder = pitTrace(dir);
        const auto playPath = program(dir);
        kit::DiagnosticList diagnostics;
        QVERIFY(Installation::install(folder, playPath, QStringLiteral("Shift_JIS"), diagnostics));
        QVERIFY(Installation::remove(folder, diagnostics));
        QVERIFY(diagnostics.isEmpty());
        QCOMPARE(read(folder / "playinplugin.exe"), QByteArray("the original"));
        QVERIFY(!fs::exists(folder / "playinplugin-original.exe"));
        const auto manifest = manifestOf(folder);
        QVERIFY(manifest);
        QVERIFY(!manifest->contains(QLatin1String("hostData")));
        QCOMPARE(Installation::stateOf(folder, playPath), Installation::Absent);
        const auto plugin = ClassicPlugin::read(folder, diagnostics);
        QVERIFY(plugin);
        QCOMPARE(plugin->name, QString::fromUtf8("テスト"));
        QCOMPARE(plugin->ustVersion, QStringLiteral("1.00"));
    }

    // A plugin.json of the plugin keeps its other keys through the installation and the
    // removal. Without an original program, none is recorded or restored.
    void an_existing_plugin_json_keeps_its_keys() {
        QTemporaryDir dir;
        const auto folder = pathOf(dir.path()) / "native";
        fs::create_directories(folder);
        write(folder / "plugin.json", R"({"execute": "a.exe", "custom": 1})");
        const auto playPath = program(dir);
        QVERIFY(!Installation::transcribes(folder));

        kit::DiagnosticList diagnostics;
        QVERIFY(Installation::install(folder, playPath, {}, diagnostics));
        auto manifest = manifestOf(folder);
        QVERIFY(manifest);
        QCOMPARE(manifest->value(QLatin1String("custom")).toInt(), 1);
        const auto record = recordIn(*manifest);
        QVERIFY(record.value(QLatin1String("original")).isNull());

        QVERIFY(Installation::remove(folder, diagnostics));
        QVERIFY(!fs::exists(folder / "playinplugin.exe"));
        manifest = manifestOf(folder);
        QVERIFY(manifest);
        QCOMPARE(manifest->value(QLatin1String("custom")).toInt(), 1);
        QVERIFY(!manifest->contains(QLatin1String("hostData")));
    }

    // The removal keeps the changes of plugin.json since the installation, those of the user and
    // the keys of another native plugin in hostData, and removes only the record.
    void the_removal_keeps_the_other_changes() {
        QTemporaryDir dir;
        const auto folder = pitTrace(dir);
        const auto playPath = program(dir);
        kit::DiagnosticList diagnostics;
        QVERIFY(Installation::install(folder, playPath, QStringLiteral("Shift_JIS"), diagnostics));
        auto manifest = manifestOf(folder);
        QVERIFY(manifest);
        manifest->insert(QLatin1String("charset"), QStringLiteral("UTF-8"));
        auto hostData = manifest->value(QLatin1String("hostData")).toObject();
        hostData.insert(QLatin1String("Other"), QJsonObject{
                                                    {QLatin1String("kept"), true}
        });
        manifest->insert(QLatin1String("hostData"), hostData);
        write(folder / "plugin.json", QJsonDocument(*manifest).toJson());

        QVERIFY(Installation::remove(folder, diagnostics));
        QVERIFY(diagnostics.isEmpty());
        QCOMPARE(read(folder / "playinplugin.exe"), QByteArray("the original"));
        manifest = manifestOf(folder);
        QVERIFY(manifest);
        QCOMPARE(manifest->value(QLatin1String("charset")).toString(), QStringLiteral("UTF-8"));
        hostData = manifest->value(QLatin1String("hostData")).toObject();
        QVERIFY(!hostData.contains(QLatin1String("ClassicPluginHost")));
        QVERIFY(hostData.value(QLatin1String("Other"))
                    .toObject()
                    .value(QLatin1String("kept"))
                    .toBool());
        QCOMPARE(Installation::stateOf(folder, playPath), Installation::Absent);
    }

    // A program of another version is outdated until it is updated.
    void an_outdated_program_is_updated() {
        QTemporaryDir dir;
        const auto folder = pitTrace(dir);
        kit::DiagnosticList diagnostics;
        QVERIFY(
            Installation::install(folder, program(dir), QStringLiteral("Shift_JIS"), diagnostics));
        const auto newer = program(dir, "plugin-play 2");
        QCOMPARE(Installation::stateOf(folder, newer), Installation::Outdated);
        QVERIFY(Installation::update(folder, newer, diagnostics));
        QCOMPARE(Installation::stateOf(folder, newer), Installation::Installed);
        QCOMPARE(read(folder / "playinplugin.exe"), QByteArray("plugin-play 2"));
        QCOMPARE(read(folder / "playinplugin-original.exe"), QByteArray("the original"));
    }

    // Only a plugin that comes with a playinplugin.exe is recommended: the installation in its
    // folder, and the update of its outdated installation. A plugin without the program is not
    // recommended, even with an outdated installation, and neither is a current installation.
    void the_plugins_with_playinplugin_are_recommended() {
        QTemporaryDir dir;
        const auto folder = pitTrace(dir);
        const auto playPath = program(dir);
        QVERIFY(Installation::isRecommended(folder, playPath));

        const auto other = pathOf(dir.path()) / "other";
        fs::create_directories(other);
        write(other / "plugin.txt", "name=other\r\nexecute=other.exe\r\n");
        QVERIFY(!Installation::isRecommended(other, playPath));

        kit::DiagnosticList diagnostics;
        QVERIFY(Installation::install(folder, playPath, QStringLiteral("Shift_JIS"), diagnostics));
        QVERIFY(Installation::install(other, playPath, QStringLiteral("Shift_JIS"), diagnostics));
        QVERIFY(!Installation::isRecommended(folder, playPath));

        // The program at the same path, of a newer version
        const auto newer = program(dir, "plugin-play 2");
        QCOMPARE(Installation::stateOf(folder, newer), Installation::Outdated);
        QVERIFY(Installation::isRecommended(folder, newer));
        QCOMPARE(Installation::stateOf(other, newer), Installation::Outdated);
        QVERIFY(!Installation::isRecommended(other, newer));
    }

    // An installation that cannot keep the original, or that exists, changes nothing.
    void a_refused_installation_changes_nothing() {
        QTemporaryDir dir;
        const auto folder = pitTrace(dir);
        write(folder / "playinplugin-original.exe", "another file");
        kit::DiagnosticList diagnostics;
        QVERIFY(
            !Installation::install(folder, program(dir), QStringLiteral("Shift_JIS"), diagnostics));
        QVERIFY(kit::hasError(diagnostics));
        QCOMPARE(read(folder / "playinplugin.exe"), QByteArray("the original"));
        QCOMPARE(read(folder / "playinplugin-original.exe"), QByteArray("another file"));
        QVERIFY(!fs::exists(folder / "plugin.json"));

        // A copy that fails after the original was renamed undoes the rename.
        fs::remove(folder / "playinplugin-original.exe");
        diagnostics.clear();
        const auto directory = pathOf(dir.path()) / "not a program";
        fs::create_directories(directory);
        QVERIFY(
            !Installation::install(folder, directory, QStringLiteral("Shift_JIS"), diagnostics));
        QVERIFY(kit::hasError(diagnostics));
        QCOMPARE(read(folder / "playinplugin.exe"), QByteArray("the original"));
        QVERIFY(!fs::exists(folder / "playinplugin-original.exe"));
        QVERIFY(!fs::exists(folder / "plugin.json"));

        diagnostics.clear();
        QVERIFY(
            Installation::install(folder, program(dir), QStringLiteral("Shift_JIS"), diagnostics));
        QVERIFY(
            !Installation::install(folder, program(dir), QStringLiteral("Shift_JIS"), diagnostics));
        QVERIFY(kit::hasError(diagnostics));
        QCOMPARE(read(folder / "playinplugin-original.exe"), QByteArray("the original"));

        diagnostics.clear();
        const auto empty = pathOf(dir.path()) / "empty";
        fs::create_directories(empty);
        QVERIFY(!Installation::remove(empty, diagnostics));
        QVERIFY(kit::hasError(diagnostics));
    }
};

QTEST_GUILESS_MAIN(test_ClassicPluginPreviewInstallation)

#include "test_ClassicPluginPreviewInstallation.moc"
