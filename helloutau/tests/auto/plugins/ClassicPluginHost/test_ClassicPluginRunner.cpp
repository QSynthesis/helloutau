#include <filesystem>
#include <fstream>
#include <string>

#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <ClassicPluginHost/ClassicPlugin.h>
#include <ClassicPluginHost/ClassicPluginRunner.h>

using namespace hello;
using namespace hello::daw;

namespace fs = std::filesystem;

namespace {

    // Set for a copy of this test that acts as a plugin. The copy writes the value of the
    // variable over the file given as its only argument.
    const char replyVariable[] = "HELLOUTAU_TEST_PLUGIN_REPLY";

}

class test_ClassicPluginRunner : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    // Returns a plugin in the folder \a name whose program is the script \a script , written
    // as a batch file on Windows and as a shell script elsewhere.
    ClassicPlugin plugin(const std::string &name, const std::string &script) const {
        const auto folder = fs::path(m_dir.path().toStdU16String()) / name;
        fs::create_directories(folder);
#ifdef Q_OS_WINDOWS
        const auto program = folder / "plugin.bat";
        std::ofstream(program, std::ios::binary) << "@echo off\r\n" << script;
#else
        const auto program = folder / "plugin.sh";
        std::ofstream(program, std::ios::binary) << "#!/bin/sh\n" << script;
        fs::permissions(program, fs::perms::owner_all, fs::perm_options::add);
#endif
        std::ofstream(folder / "reply.txt", std::ios::binary) << "[#0000]\r\nLyric=z\r\n";

        ClassicPlugin plugin;
        plugin.folder = folder;
        plugin.name = QString::fromStdString(name);
        plugin.program = program;
        return plugin;
    }

    static QString read(const fs::path &file) {
        QFile in(QString::fromStdU16String(file.u16string()));
        if (!in.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QString::fromLocal8Bit(in.readAll()).trimmed();
    }

private Q_SLOTS:
    // The program runs in its folder on a file named as in UTAU, and the file as left by the
    // program is the result.
    void the_result_is_read_back() {
#ifdef Q_OS_WINDOWS
        const auto p = plugin("reply", "echo %~nx1> \"%~dp0name.txt\"\r\n"
                                       "cd> \"%~dp0cwd.txt\"\r\n"
                                       "copy /y \"%~dp0reply.txt\" \"%~1\" >nul\r\n");
#else
        const auto p = plugin("reply", "basename \"$1\" > \"$(dirname \"$0\")/name.txt\"\n"
                                       "pwd > \"$(dirname \"$0\")/cwd.txt\"\n"
                                       "cp \"$(dirname \"$0\")/reply.txt\" \"$1\"\n");
#endif
        ClassicPluginRunner runner;
        runner.setShowsConsole(false);
        QSignalSpy finished(&runner, &ClassicPluginRunner::finished);
        QString error;
        QVERIFY2(runner.start(p, "[#0000]\r\nLyric=a\r\n", &error), qPrintable(error));
        QVERIFY(runner.isRunning());
        QVERIFY(finished.wait(20000));
        QVERIFY(!runner.isRunning());
        QVERIFY(!runner.isCancelled());
        QCOMPARE(runner.result(), QByteArray("[#0000]\r\nLyric=z\r\n"));
        QVERIFY(!runner.isUnchanged());

        QVERIFY(QRegularExpression(QStringLiteral("^tmp[0-9A-F]{1,4}\\.tmp$"))
                    .match(read(p.folder / "name.txt"))
                    .hasMatch());
        QCOMPARE(QDir(read(p.folder / "cwd.txt")),
                 QDir(QString::fromStdU16String(p.folder.u16string())));
    }

    // A program that does not write the file leaves it unchanged.
    void an_untouched_file_is_unchanged() {
        const auto p = plugin("nothing", "");
        ClassicPluginRunner runner;
        runner.setShowsConsole(false);
        QSignalSpy finished(&runner, &ClassicPluginRunner::finished);
        QVERIFY(runner.start(p, "[#0000]\r\n", nullptr));
        QVERIFY(finished.wait(20000));
        QVERIFY(runner.isUnchanged());
    }

    // Cancellation terminates the program immediately, together with the processes started by
    // it. The background process started by the program never writes its file.
    void cancelling_ends_what_the_program_started() {
#ifdef Q_OS_WINDOWS
        const auto p = plugin("waiting", "start \"\" /b cmd /c \"ping -n 3 127.0.0.1 >nul & "
                                         "echo x> \"%~dp0late.txt\"\"\r\n"
                                         "ping -n 30 127.0.0.1 >nul\r\n");
#else
        const auto p = plugin("waiting", "(sleep 2; echo x > \"$(dirname \"$0\")/late.txt\") &\n"
                                         "sleep 30\n");
#endif
        ClassicPluginRunner runner;
        runner.setShowsConsole(false);
        QSignalSpy finished(&runner, &ClassicPluginRunner::finished);
        QVERIFY(runner.start(p, "[#0000]\r\n", nullptr));
        QTest::qWait(500);
        QVERIFY(runner.isRunning());

        QElapsedTimer timer;
        timer.start();
        runner.cancel();
        QVERIFY(timer.elapsed() < 5000);
        QCOMPARE(finished.count(), 1);
        QVERIFY(runner.isCancelled());
        QVERIFY(runner.result().isEmpty());

        QTest::qWait(3500);
        QVERIFY(!fs::exists(p.folder / "late.txt"));
    }

    // With shell=use, the handler of the file type starts the program, as in UTAU.
    void the_shell_starts_a_plugin_that_asks_for_it() {
#ifdef Q_OS_WINDOWS
        auto p = plugin("shell", "copy /y \"%~dp0reply.txt\" \"%~1\" >nul\r\n");
        p.shell = true;
        ClassicPluginRunner runner;
        runner.setShowsConsole(false);
        QSignalSpy finished(&runner, &ClassicPluginRunner::finished);
        QString error;
        QVERIFY2(runner.start(p, "[#0000]\r\n", &error), qPrintable(error));
        QVERIFY(!runner.isUnobservable());
        QVERIFY(finished.wait(20000));
        QCOMPARE(runner.result(), QByteArray("[#0000]\r\nLyric=z\r\n"));
#else
        QSKIP("shell=use runs on Windows only");
#endif
    }

    // An executable runs directly, with its path and the file path as separate arguments. This
    // test, copied into a folder whose name contains a space, acts as the plugin.
    void a_program_runs_on_the_file() {
        const auto folder = fs::path(m_dir.path().toStdU16String()) / "a program";
        fs::create_directories(folder);
        const auto self = fs::path(QCoreApplication::applicationFilePath().toStdU16String());
        const auto program = folder / self.filename();
        fs::copy_file(self, program, fs::copy_options::overwrite_existing);

        ClassicPlugin p;
        p.folder = folder;
        p.name = QStringLiteral("program");
        p.program = program;
        // The libraries beside this test are located through the PATH inherited by the copy.
        const auto path = qgetenv("PATH");
        qputenv("PATH", QFile::encodeName(
                            QDir::toNativeSeparators(QCoreApplication::applicationDirPath())) +
                            QDir::listSeparator().toLatin1() + path);
        qputenv(replyVariable, "[#0000]\r\nLyric=p\r\n");
        ClassicPluginRunner runner;
        runner.setShowsConsole(false);
        QSignalSpy finished(&runner, &ClassicPluginRunner::finished);
        QString error;
        const bool started = runner.start(p, "[#0000]\r\n", &error);
        qunsetenv(replyVariable);
        qputenv("PATH", path);
        QVERIFY2(started, qPrintable(error));
        QVERIFY(finished.wait(20000));
        QCOMPARE(runner.result(), QByteArray("[#0000]\r\nLyric=p\r\n"));
    }

    // A program that cannot start is reported, and no run begins.
    void a_missing_program_is_reported() {
        auto p = plugin("missing", "");
        p.program = p.folder / "missing.exe";
        ClassicPluginRunner runner;
        QString error;
        QVERIFY(!runner.start(p, "[#0000]\r\n", &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!runner.isRunning());
    }
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    if (qEnvironmentVariableIsSet(replyVariable)) {
        const auto arguments = QCoreApplication::arguments();
        if (arguments.size() != 2) {
            return 2;
        }
        QFile out(arguments[1]);
        if (!out.open(QIODevice::WriteOnly)) {
            return 3;
        }
        out.write(qgetenv(replyVariable));
        return 0;
    }
    test_ClassicPluginRunner test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_ClassicPluginRunner.moc"
