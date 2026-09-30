#include "ClassicPluginRunner.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QRandomGenerator>
#include <QtCore/QTemporaryDir>

#ifdef Q_OS_WINDOWS
#  include <string>

#  include <QtCore/QWinEventNotifier>
#  include <QtCore/qt_windows.h>

#  include <shellapi.h>
#else
#  include <signal.h>
#  include <unistd.h>

#  include <QtCore/QProcess>
#endif

#include "ClassicPlugin.h"

namespace hello::daw {

    namespace {

#ifdef Q_OS_WINDOWS
        // \a argument quoted for CommandLineToArgvW(), by which a program reads its command
        // line: a backslash doubled before a quotation mark and at the end, and the quotation
        // mark escaped.
        std::wstring quotedArgument(const std::wstring &argument) {
            std::wstring out = L"\"";
            for (size_t i = 0;; ++i) {
                size_t backslashes = 0;
                while (i < argument.size() && argument[i] == L'\\') {
                    ++i;
                    ++backslashes;
                }
                if (i == argument.size()) {
                    out.append(backslashes * 2, L'\\');
                    break;
                }
                if (argument[i] == L'"') {
                    out.append(backslashes * 2 + 1, L'\\');
                } else {
                    out.append(backslashes, L'\\');
                }
                out.push_back(argument[i]);
            }
            out.push_back(L'"');
            return out;
        }
#endif

    }

    class ClassicPluginRunner::Impl {
    public:
        explicit Impl(ClassicPluginRunner *q) : q(q) {
        }

        ~Impl() {
            if (running) {
                kill();
                release();
            }
        }

        ClassicPluginRunner *q;

        bool showsConsole = true;
        std::unique_ptr<QTemporaryDir> directory;
        QString file;
        QByteArray input;
        QByteArray result;

        bool running = false;
        bool cancelled = false;
        bool unobservable = false;

#ifdef Q_OS_WINDOWS
        // The job holds the program and every process that it starts, so that cancel() ends
        // them all.
        HANDLE job = nullptr;
        HANDLE process = nullptr;
        QWinEventNotifier *notifier = nullptr;
#else
        QProcess *process = nullptr;
#endif

        bool writeInput(QString *error) {
            directory = std::make_unique<QTemporaryDir>(QDir::tempPath() +
                                                        QStringLiteral("/HelloUtau-XXXXXX"));
            if (!directory->isValid()) {
                *error = directory->errorString();
                return false;
            }
            // tmp and up to four hexadecimal digits, as UTAU names it
            const auto number = QString::number(QRandomGenerator::global()->bounded(0x10000), 16);
            file = directory->filePath(QStringLiteral("tmp%1.tmp").arg(number.toUpper()));
            QFile out(file);
            if (!out.open(QIODevice::WriteOnly) || out.write(input) != input.size()) {
                *error = out.errorString();
                return false;
            }
            return true;
        }

        bool startProgram(const ClassicPlugin &plugin, QString *error);

        // Ends the program and what it started, without reading the result.
        void kill();

        // Frees the handles of the program.
        void release();

        // Ends the run: reads the result unless it was cancelled.
        void end() {
            if (!running) {
                return;
            }
            running = false;
            release();
            if (!cancelled) {
                QFile in(file);
                if (in.open(QIODevice::ReadOnly)) {
                    result = in.readAll();
                }
            }
            Q_EMIT q->finished();
        }
    };

#ifdef Q_OS_WINDOWS
    bool ClassicPluginRunner::Impl::startProgram(const ClassicPlugin &plugin, QString *error) {
        job = CreateJobObjectW(nullptr, nullptr);
        if (!job) {
            *error = qt_error_string(int(GetLastError()));
            return false;
        }

        const auto program = plugin.program.wstring();
        const auto folder = plugin.folder.wstring();
        const auto argument = quotedArgument(QDir::toNativeSeparators(file).toStdWString());

        if (plugin.shell) {
            // The handler of the file type, as UTAU starts such a plugin
            SHELLEXECUTEINFOW info = {};
            info.cbSize = sizeof(info);
            info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
            info.lpFile = program.c_str();
            info.lpParameters = argument.c_str();
            info.lpDirectory = folder.c_str();
            info.nShow = showsConsole ? SW_SHOWNORMAL : SW_HIDE;
            if (!ShellExecuteExW(&info)) {
                *error = qt_error_string(int(GetLastError()));
                return false;
            }
            if (!info.hProcess) {
                unobservable = true;
                return true;
            }
            // The handler may have started processes already, which the job then misses.
            AssignProcessToJobObject(job, info.hProcess);
            process = info.hProcess;
        } else {
            auto application = program;
            auto commandLine = quotedArgument(program) + L' ' + argument;

            // A batch file runs in the command processor, which Windows would start with the
            // command line after /c and strip its first and last quotation marks, cutting both
            // paths. /s strips only the pair around the whole command. Within the quotation
            // marks nothing is a command but the variables, which a path may not contain.
            const auto extension =
                QString::fromStdU16String(plugin.program.extension().u16string());
            if (extension.compare(QLatin1String(".bat"), Qt::CaseInsensitive) == 0 ||
                extension.compare(QLatin1String(".cmd"), Qt::CaseInsensitive) == 0) {
                if (commandLine.find(L'%') != std::wstring::npos) {
                    *error = ClassicPluginRunner::tr(
                        "A batch file cannot run on a path that contains \"%\".");
                    return false;
                }
                wchar_t processor[MAX_PATH];
                const auto size = GetEnvironmentVariableW(L"ComSpec", processor, MAX_PATH);
                application = size > 0 && size < MAX_PATH ? std::wstring(processor, size)
                                                          : std::wstring(L"cmd.exe");
                commandLine = quotedArgument(application) + L" /d /s /c \"" + commandLine + L'"';
            }

            // Suspended until it is in the job, so that nothing it starts escapes the job
            STARTUPINFOW startup = {};
            startup.cb = sizeof(startup);
            PROCESS_INFORMATION info = {};
            const DWORD flags =
                CREATE_SUSPENDED | (showsConsole ? CREATE_NEW_CONSOLE : CREATE_NO_WINDOW);
            if (!CreateProcessW(application.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
                                flags, nullptr, folder.c_str(), &startup, &info)) {
                *error = qt_error_string(int(GetLastError()));
                return false;
            }
            AssignProcessToJobObject(job, info.hProcess);
            ResumeThread(info.hThread);
            CloseHandle(info.hThread);
            process = info.hProcess;
        }

        notifier = new QWinEventNotifier(process);
        QObject::connect(notifier, &QWinEventNotifier::activated, q, [this] { end(); });
        return true;
    }

    void ClassicPluginRunner::Impl::kill() {
        if (job) {
            TerminateJobObject(job, 1);
        }
        // A process that the job missed
        if (process) {
            TerminateProcess(process, 1);
            WaitForSingleObject(process, INFINITE);
        }
    }

    void ClassicPluginRunner::Impl::release() {
        // Called from its signal as well
        if (notifier) {
            notifier->setEnabled(false);
            notifier->deleteLater();
            notifier = nullptr;
        }
        if (process) {
            CloseHandle(process);
            process = nullptr;
        }
        if (job) {
            CloseHandle(job);
            job = nullptr;
        }
    }
#else
    bool ClassicPluginRunner::Impl::startProgram(const ClassicPlugin &plugin, QString *error) {
        process = new QProcess();
        process->setProgram(QString::fromStdU16String(plugin.program.u16string()));
        process->setArguments({file});
        process->setWorkingDirectory(QString::fromStdU16String(plugin.folder.u16string()));
        // A process group of its own, so that cancel() ends what it started
        process->setChildProcessModifier([] { ::setpgid(0, 0); });
        QObject::connect(process, &QProcess::finished, q, [this] { end(); });
        process->start();
        if (!process->waitForStarted()) {
            *error = process->errorString();
            delete process;
            process = nullptr;
            return false;
        }
        return true;
    }

    void ClassicPluginRunner::Impl::kill() {
        if (process && process->state() != QProcess::NotRunning) {
            ::kill(-pid_t(process->processId()), SIGKILL);
            process->waitForFinished();
        }
    }

    void ClassicPluginRunner::Impl::release() {
        if (process) {
            process->disconnect(q);
            process->deleteLater();
            process = nullptr;
        }
    }
#endif

    ClassicPluginRunner::ClassicPluginRunner(QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(this)) {
    }

    ClassicPluginRunner::~ClassicPluginRunner() = default;

    bool ClassicPluginRunner::showsConsole() const {
        return _impl->showsConsole;
    }

    void ClassicPluginRunner::setShowsConsole(bool shows) {
        _impl->showsConsole = shows;
    }

    bool ClassicPluginRunner::start(const ClassicPlugin &plugin, const QByteArray &input,
                                    QString *error) {
        auto &impl = *_impl;
        Q_ASSERT(!impl.running);
        impl.input = input;
        impl.result.clear();
        impl.cancelled = false;
        impl.unobservable = false;
        QString reason;
        if (!impl.writeInput(&reason) || !impl.startProgram(plugin, &reason)) {
            impl.release();
            if (error) {
                *error = tr("The plugin \"%1\" could not be started: %2").arg(plugin.name, reason);
            }
            return false;
        }
        impl.running = true;
        return true;
    }

    bool ClassicPluginRunner::isRunning() const {
        return _impl->running;
    }

    bool ClassicPluginRunner::isUnobservable() const {
        return _impl->unobservable;
    }

    void ClassicPluginRunner::finish() {
        _impl->end();
    }

    void ClassicPluginRunner::cancel() {
        auto &impl = *_impl;
        if (!impl.running) {
            return;
        }
        impl.cancelled = true;
        impl.kill();
        impl.end();
    }

    bool ClassicPluginRunner::isCancelled() const {
        return _impl->cancelled;
    }

    QByteArray ClassicPluginRunner::result() const {
        return _impl->result;
    }

    bool ClassicPluginRunner::isUnchanged() const {
        return _impl->result == _impl->input;
    }

}
