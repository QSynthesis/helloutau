#ifndef HELLOUTAU_EDITOR_EDITOR_H
#define HELLOUTAU_EDITOR_EDITOR_H

#include <filesystem>
#include <memory>

#include <QtCore/QList>
#include <QtCore/QObject>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace QAK {
    class ActionRegistry;
}

namespace hello::daw {

    class AppSettings;
    class MainWindow;
    class ThemeManager;

    /// The editor as a whole: the settings, the actions shared by every window, the themes, and
    /// the windows, each of which edits one project.
    class HELLOUTAU_EDITOR_EXPORT Editor : public QObject {
        Q_OBJECT
    public:
        /// An editor with the settings of the current user.
        explicit Editor(QObject *parent = nullptr);

        /// An editor with \a settings, for tests.
        explicit Editor(std::unique_ptr<AppSettings> settings, QObject *parent = nullptr);

        ~Editor();

        AppSettings &settings() const;

        /// The registry of the actions of all windows, each window being one context of it.
        QAK::ActionRegistry *actionRegistry() const;

        /// The themes of all windows, with the built-in one under \c :/helloutau/themes.
        ThemeManager *themeManager() const;

        /// The open windows, in the order in which they were opened.
        QList<MainWindow *> windows() const;

        /// Opens a window with a new project.
        MainWindow *newWindow();

        /// Opens \a path. A window that already shows the file is activated instead. Otherwise
        /// the file opens in \a from if \a from shows an unmodified new project, and in a new
        /// window if not. Errors are shown to the user.
        ///
        /// \return the window that shows the file, or \c nullptr if it was not opened
        MainWindow *openFile(const std::filesystem::path &path, MainWindow *from = nullptr);

        /// Closes every window, each asking to save its project first. Stops at the first window
        /// whose user cancels, and returns whether every window was closed.
        bool closeAll();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;

        friend class MainWindow;
    };

}

#endif // HELLOUTAU_EDITOR_EDITOR_H
