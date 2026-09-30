#include <filesystem>
#include <fstream>
#include <string>

#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <ClassicPluginHost/ClassicPlugin.h>

using namespace hello;
using namespace hello::daw;

namespace fs = std::filesystem;

class test_ClassicPlugin : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    fs::path root() const {
        return fs::path(m_dir.path().toStdU16String());
    }

    // Creates the folder <directory>/<name> with the plugin.txt \a txt in ASCII and the files
    // \a files .
    fs::path plugin(const std::string &name, const std::string &txt,
                    std::initializer_list<const char *> files = {},
                    const char *directory = "plugins") const {
        const auto folder = root() / directory / name;
        fs::create_directories(folder);
        std::ofstream(folder / "plugin.txt", std::ios::binary) << txt;
        for (const auto file : files) {
            std::ofstream(folder / file, std::ios::binary);
        }
        return folder;
    }

    // Writes \a json as the plugin.json of \a folder .
    static void manifest(const fs::path &folder, const std::string &json) {
        fs::create_directories(folder);
        std::ofstream(folder / "plugin.json", std::ios::binary) << json;
    }

private Q_SLOTS:
    // A plugin.json in UTF-8 replaces plugin.txt entirely. The plugin.txt is not read, and the
    // encoding of the temporary file is the charset of the plugin.json.
    void plugin_json_replaces_plugin_txt() {
        const auto folder = plugin("json", "name=Old\r\nexecute=old.bat\r\nnotes=all\r\n",
                                   {"old.bat", "new.bat"}, "manifests");
        manifest(folder, R"({"name": "新しい", "execute": "new.bat", "shell": true, )"
                         R"("notes": "selection", "charset": "UTF-8"})");
        kit::DiagnosticList diagnostics;
        const auto read = ClassicPlugin::read(folder, diagnostics);
        QVERIFY(read);
        QVERIFY(read->isAvailable());
        QCOMPARE(read->name, QStringLiteral("新しい"));
        QVERIFY(fs::equivalent(read->program, folder / "new.bat"));
        QVERIFY(read->shell);
        QVERIFY(!read->wholeTrack);
        QCOMPARE(read->charset, QStringLiteral("UTF-8"));
    }

    // The defaults of plugin.json: the folder name, the program started directly, the
    // selection, and the encoding of UTAU.
    void plugin_json_defaults() {
        const auto folder = root() / "manifests" / "defaults";
        manifest(folder, R"({"execute": "a.bat"})");
        std::ofstream(folder / "a.bat", std::ios::binary);
        kit::DiagnosticList diagnostics;
        const auto read = ClassicPlugin::read(folder, diagnostics);
        QVERIFY(read);
        QVERIFY(read->isAvailable());
        QCOMPARE(read->name, QStringLiteral("defaults"));
        QVERIFY(!read->shell);
        QVERIFY(!read->wholeTrack);
        QCOMPARE(read->charset, ClassicPlugin::localCharset());

        manifest(folder, R"({"execute": "a.bat", "notes": "all"})");
        QVERIFY(ClassicPlugin::read(folder, diagnostics)->wholeTrack);
        QVERIFY(diagnostics.isEmpty());
    }

    // An unknown value makes the plugin unavailable with the reason. A plugin.json that is not
    // a JSON object is reported, and the folder is skipped.
    void plugin_json_that_is_wrong() {
        const auto folder = root() / "manifests" / "wrong";
        std::error_code error;
        fs::create_directories(folder, error);
        std::ofstream(folder / "a.bat", std::ios::binary);
        for (const auto json : {R"({"execute": "a.bat", "notes": "some"})",
                                R"({"execute": "a.bat", "charset": "no-such-encoding"})",
                                R"({"execute": "../a.bat"})"}) {
            manifest(folder, json);
            kit::DiagnosticList diagnostics;
            const auto read = ClassicPlugin::read(folder, diagnostics);
            QVERIFY(read);
            QVERIFY2(!read->isAvailable(), json);
        }

        manifest(folder, "[1,");
        kit::DiagnosticList diagnostics;
        QVERIFY(!ClassicPlugin::read(folder, diagnostics));
        QCOMPARE(diagnostics.size(), 1);
    }

    // The folders of a directory are read in the order of their names, each from its
    // plugin.txt. A folder without a plugin.txt is not a plugin.
    void plugins_are_found_by_folder() {
        plugin("b", "name=Beta\r\nexecute=b.bat\r\nshell=use\r\nnotes=all\r\n", {"b.bat"});
        plugin("a", "name=Alpha\r\nexecute=a.bat\r\n", {"a.bat"});
        fs::create_directories(root() / "plugins" / "c");
        plugin("d", "execute=d.bat\r\n", {"d.bat"});

        kit::DiagnosticList diagnostics;
        const auto plugins =
            ClassicPlugin::discover({root() / "plugins", root() / "none"}, diagnostics);
        QVERIFY(diagnostics.isEmpty());
        QCOMPARE(plugins.size(), 3);

        QCOMPARE(plugins[0].name, QStringLiteral("Alpha"));
        QVERIFY(plugins[0].isAvailable());
        QVERIFY(!plugins[0].shell);
        QVERIFY(!plugins[0].wholeTrack);
        QVERIFY(fs::equivalent(plugins[0].program, root() / "plugins" / "a" / "a.bat"));
        QCOMPARE(plugins[0].charset, ClassicPlugin::localCharset());

        QCOMPARE(plugins[1].name, QStringLiteral("Beta"));
        QVERIFY(plugins[1].shell);
        QVERIFY(plugins[1].wholeTrack);

        // Without a name, the folder name is used.
        QCOMPARE(plugins[2].name, QStringLiteral("d"));
    }

    // The program must be inside the plugin folder. An absolute path, any .. component, or a
    // missing file makes the plugin unavailable, with the reason.
    void the_program_must_be_in_the_folder() {
        fs::create_directories(root() / "outside");
        std::ofstream(root() / "outside" / "x.bat", std::ios::binary);
        const auto absolute =
            QString::fromStdU16String((root() / "outside" / "x.bat").u16string()).toStdString();
        const std::string cases[] = {
            "execute=" + absolute + "\r\n",
            "execute=..\\..\\outside\\x.bat\r\n",
            "execute=sub/../../../outside/x.bat\r\n",
            "execute=missing.bat\r\n",
            "name=Nothing\r\n",
        };
        for (const auto &txt : cases) {
            const auto folder = plugin("p", txt);
            kit::DiagnosticList diagnostics;
            const auto read = ClassicPlugin::read(folder, diagnostics);
            QVERIFY(read);
            QVERIFY2(!read->isAvailable(), txt.c_str());
            QVERIFY(read->program.empty());
        }

        // A program in a subfolder of the plugin folder is accepted.
        const auto folder = plugin("q", "execute=sub/q.bat\r\n");
        fs::create_directories(folder / "sub");
        std::ofstream(folder / "sub" / "q.bat", std::ios::binary);
        kit::DiagnosticList diagnostics;
        const auto read = ClassicPlugin::read(folder, diagnostics);
        QVERIFY(read);
        QVERIFY(read->isAvailable());
    }
};

QTEST_GUILESS_MAIN(test_ClassicPlugin)

#include "test_ClassicPlugin.moc"
