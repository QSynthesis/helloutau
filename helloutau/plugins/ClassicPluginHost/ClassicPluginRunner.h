#ifndef HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINRUNNER_H
#define HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINRUNNER_H

#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <ClassicPluginHost/ClassicPluginHostPluginGlobal.h>

namespace hello::daw {

    class ClassicPlugin;

    /// One run of a ClassicPlugin: the temporary file written, the program started on it, and
    /// the file read back when the program ends.
    ///
    /// The file lies in a directory of its own under the system temporary directory, named
    /// \c tmpXXXX.tmp as UTAU names it, and is removed with the runner. The program starts in
    /// the folder of the plugin with the path of the file as its only argument, never through a
    /// command line of the shell (AGENTS.md). With \c shell=use it is started by the handler of
    /// its file type, as \c ShellExecuteEx starts it.
    ///
    /// cancel() ends the program and every process that it started. There is no time limit, and
    /// the exit code is not checked: the file alone is the result. See docs/ClassicPluginHost.md.
    class CLASSICPLUGINHOST_EXPORT ClassicPluginRunner : public QObject {
        Q_OBJECT
    public:
        explicit ClassicPluginRunner(QObject *parent = nullptr);

        /// Ends the program if it still runs, and removes the file.
        ~ClassicPluginRunner();

        /// Whether the program shows its console window, as UTAU shows it. On by default; the
        /// tests turn it off.
        bool showsConsole() const;
        void setShowsConsole(bool shows);

        /// Writes \a input and starts \a plugin on it. Returns whether the program started, with
        /// the reason in \a error otherwise. finished() follows a start.
        bool start(const ClassicPlugin &plugin, const QByteArray &input, QString *error);

        /// Whether the program has been started and has not ended.
        bool isRunning() const;

        /// Whether the end of the program cannot be observed: the handler of a file type may
        /// pass the file to a program that is already open, such as a browser, and give no
        /// process to wait for. The user then says when the plugin is done, and finish() ends
        /// the run.
        bool isUnobservable() const;

        /// Ends the run of an unobservable program, which the user says is done.
        void finish();

        /// Ends the program and every process that it started, and discards the result.
        void cancel();

        /// Whether the run was cancelled.
        bool isCancelled() const;

        /// The file as the program left it, read when the run ended. Empty before.
        QByteArray result() const;

        /// Whether the program left the file as it was written.
        bool isUnchanged() const;

    Q_SIGNALS:
        /// The program ended, was cancelled, or the user said that it is done.
        void finished();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINRUNNER_H
