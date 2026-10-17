#include <filesystem>
#include <fstream>

#include <QtCore/QDir>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <ClassicPluginHost/ClassicPlugin.h>
#include <ClassicPluginHost/ClassicPluginTrust.h>

using namespace hello::daw;
namespace fs = std::filesystem;
namespace json = stdc::json;

namespace {

    // Returns a plugin whose program is bin/name.exe in the folder name of dir, with content
    ClassicPlugin pluginIn(const QTemporaryDir &dir, const char *name, const char *content) {
        ClassicPlugin plugin;
        plugin.name = QString::fromLatin1(name);
        plugin.folder = fs::path(dir.path().toStdU16String()) / name;
        plugin.program = plugin.folder / "bin" / (std::string(name) + ".exe");
        fs::create_directories(plugin.program.parent_path());
        std::ofstream(plugin.program, std::ios::binary | std::ios::trunc) << content;
        return plugin;
    }

    std::string textOf(const json::Value &record, const char *key) {
        return record[key].toString(std::string());
    }

}

class test_ClassicPluginTrust : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void the_records_are_written_under_the_id_of_the_plugin() {
        QCOMPARE(QLatin1String(ClassicPluginTrust::userDataPath),
                 QLatin1String("org.helloutau.classicpluginhost/approved"));
    }

    // A trusted program is recorded with its folder, its path relative to the folder and its
    // SHA-256, and is trusted until its content changes.
    void a_program_is_trusted_until_it_changes() {
        QTemporaryDir dir;
        auto plugin = pluginIn(dir, "Foo", "program");
        QCOMPARE(ClassicPluginTrust::stateOf({}, plugin), ClassicPluginTrust::New);

        const auto records = ClassicPluginTrust::trusted({}, plugin);
        QCOMPARE(records.size(), size_t(1));
        QCOMPARE(QString::fromStdString(textOf(records[0], "folder")),
                 QDir::toNativeSeparators(QString::fromStdU16String(plugin.folder.u16string())));
        QCOMPARE(QString::fromStdString(textOf(records[0], "relativePath")),
                 QDir::toNativeSeparators(QStringLiteral("bin/Foo.exe")));
        QCOMPARE(textOf(records[0], "sha256").size(), size_t(64));
        QCOMPARE(ClassicPluginTrust::stateOf(records, plugin), ClassicPluginTrust::Trusted);

        std::ofstream(plugin.program, std::ios::binary | std::ios::trunc) << "changed";
        QCOMPARE(ClassicPluginTrust::stateOf(records, plugin), ClassicPluginTrust::Changed);
    }

    // Trusting the program of a folder again replaces its record, and a program of another
    // folder is added.
    void a_folder_has_one_record() {
        QTemporaryDir dir;
        const auto foo = pluginIn(dir, "Foo", "program");
        const auto bar = pluginIn(dir, "Bar", "other");
        auto records = ClassicPluginTrust::trusted({}, foo);
        records = ClassicPluginTrust::trusted(records, bar);
        QCOMPARE(records.size(), size_t(2));
        QCOMPARE(ClassicPluginTrust::stateOf(records, bar), ClassicPluginTrust::Trusted);

        std::ofstream(foo.program, std::ios::binary | std::ios::trunc) << "changed";
        QCOMPARE(ClassicPluginTrust::stateOf(records, foo), ClassicPluginTrust::Changed);
        records = ClassicPluginTrust::trusted(records, foo);
        QCOMPARE(records.size(), size_t(2));
        QCOMPARE(ClassicPluginTrust::stateOf(records, foo), ClassicPluginTrust::Trusted);
        QCOMPARE(ClassicPluginTrust::stateOf(records, bar), ClassicPluginTrust::Trusted);
    }
};

QTEST_APPLESS_MAIN(test_ClassicPluginTrust)

#include "test_ClassicPluginTrust.moc"
