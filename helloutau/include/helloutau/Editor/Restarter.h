#ifndef HELLOUTAU_EDITOR_RESTARTER_H
#define HELLOUTAU_EDITOR_RESTARTER_H

#include <QtCore/QCoreApplication>
#include <QtCore/QStringList>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QWidget;

namespace hello::daw {

    class Editor;

    /// Restarts the application for the settings that take effect only at a start, such as the
    /// language.
    ///
    /// A settings page that applies such a setting calls markNeeded(). Once the settings dialog
    /// closes, Editor::showSettings() calls offer(), which asks the user whether to restart now.
    /// A restart closes the windows as quitting does, and once the event loop has ended,
    /// AppLoader::run() calls startAgain().
    class HELLOUTAU_EDITOR_EXPORT Restarter {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::Restarter)
    public:
        /// Records that an applied setting takes effect only after a restart.
        static void markNeeded();

        /// Returns whether a setting applied since the last offer() takes effect only after a
        /// restart.
        static bool isNeeded();

        /// Asks the user over \a parent whether to restart now, if isNeeded(). If the user
        /// agrees, closes the windows of \a editor, each prompting to save its document as
        /// quitting does, and if all close, quits the application to start it again.
        ///
        /// \return Whether the application quits to start again. A user who declines, or who
        ///         cancels the closing of a window, restarts later by hand.
        static bool offer(QWidget *parent, Editor *editor);

        /// Returns whether the application has quit to start again.
        static bool isRestarting();

        /// Starts the program again with \a arguments, which exclude the program, if
        /// isRestarting().
        ///
        /// \return Whether a restart was due and the program was started.
        static bool startAgain(const QStringList &arguments);
    };

}

#endif // HELLOUTAU_EDITOR_RESTARTER_H
