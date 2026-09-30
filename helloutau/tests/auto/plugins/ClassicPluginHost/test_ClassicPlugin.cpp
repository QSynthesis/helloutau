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

    // A folder of plugins/<name> with the plugin.txt \a txt , in ASCII, and the files \a files
    fs::path plugin(const std::string &name, const std::string &txt,
                    std::initializer_list<const char *> files = {}) const {
        const auto folder = root() / "plugins" / name;
        fs::create_directories(folder);
        std::ofstream(folder / "plugin.txt", std::ios::binary) << txt;
        for (const auto file : files) {
            std::ofstream(folder / file, std::ios::binary);
        }
        return folder;
    }

private Q_SLOTS:
    // The folders of a directory in the order of their names, each read from its plugin.txt. A
    // folder without one is no plugin.
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

        // Without a name, the folder names the plugin.
        QCOMPARE(plugins[2].name, QStringLiteral("d"));
    }

    // The program must lie in the folder of the plugin: an absolute path, any parent step or a
    // missing file leaves the plugin unavailable, with the reason.
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

        // Within the folder, a subfolder is fine.
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
