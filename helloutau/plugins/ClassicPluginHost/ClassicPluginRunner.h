#ifndef HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINRUNNER_H
#define HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINRUNNER_H

#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <ClassicPluginHost/ClassicPluginHostPluginGlobal.h>

namespace hello::daw {

    class ClassicPlugin;

    /// One run of a ClassicPlugin: writing the temporary file, starting the program on it, and
    /// reading the file back when the program ends.
    ///
    /// The file is named \c tmpXXXX.tmp as in UTAU, is located in a dedicated directory under
    /// the system temporary directory, and is removed with the runner. The program starts in
    /// the plugin folder with the file path as its only argument, never through a shell command
    /// line (CLAUDE.md). With \c shell=use , the program is started by the handler of its file
    /// type, as with \c ShellExecuteEx .
    ///
    /// cancel() terminates the program and every process started by it. There is no time
    /// limit, and the exit code is not checked. The file is the only result. See
    /// docs/ClassicPluginHost.md.
    class CLASSICPLUGINHOST_EXPORT ClassicPluginRunner : public QObject {
        Q_OBJECT
    public:
        explicit ClassicPluginRunner(QObject *parent = nullptr);

        /// Terminates the program if it is running, and removes the file.
        ~ClassicPluginRunner();

        /// Returns whether the console window of the program is shown, as in UTAU. The default
        /// is true, and the tests disable it.
        bool showsConsole() const;
        void setShowsConsole(bool shows);

        /// Writes \a input and starts \a plugin on it. finished() is emitted after every
        /// successful start.
        ///
        /// \return whether the program started. If it did not, \a error receives the reason.
        bool start(const ClassicPlugin &plugin, const QByteArray &input, QString *error);

        /// Returns whether the program has started and has not yet ended.
        bool isRunning() const;

        /// Returns whether the end of the program cannot be observed. The handler of a file
        /// type may pass the file to a program that is already running, such as a browser, and
        /// provide no process to wait for. The user then confirms that the plugin is done, and
        /// finish() ends the run.
        bool isUnobservable() const;

        /// Ends the run of an unobservable program after the user confirms that it is done.
        void finish();

        /// Terminates the program and every process started by it, and discards the result.
        void cancel();

        /// Returns whether the run was cancelled.
        bool isCancelled() const;

        /// Returns the file as left by the program, read when the run ended, or an empty array
        /// before the run ends.
        QByteArray result() const;

        /// Returns whether the program left the file unchanged.
        bool isUnchanged() const;

    Q_SIGNALS:
        /// Emitted when the program ends, when the run is cancelled, or when the user confirms
        /// that the plugin is done.
        void finished();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINRUNNER_H
