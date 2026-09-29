#ifndef HELLOUTAU_EDITOR_EDITOR_H
#define HELLOUTAU_EDITOR_EDITOR_H

#include <filesystem>
#include <memory>

#include <QtCore/QList>
#include <QtCore/QObject>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QMenu;
class QWidget;

namespace QAK {
    class ActionRegistry;
}

namespace hello::daw {

    class AppSettings;
    class MainWindow;
    class ThemeManager;
    class VoiceBankWindow;

    /// The editor as a whole: the settings, the actions shared by every window, the themes, and
    /// the windows, each of which edits one project or one voice bank.
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

        /// The open voice bank windows, in the order in which they were opened.
        QList<VoiceBankWindow *> voiceBankWindows() const;

        /// Opens the voice bank in the folder \a root in a window of its own. A window that
        /// already shows it is activated instead. The user is asked for the encoding of each
        /// folder that does not record it, and errors are shown, over \a from.
        ///
        /// \return the window that shows the voice bank, or \c nullptr if it was not opened
        VoiceBankWindow *openVoiceBank(const std::filesystem::path &root, QWidget *from = nullptr);

        /// Fills \a menu, the menu "Open Recent" of a window, with the files opened last. A
        /// project opens in \a from if it is a project window that is unused.
        void fillRecentMenu(QMenu *menu, QWidget *from);

        /// Closes every window, each asking to save its document first. Stops at the first
        /// window whose user cancels, and returns whether every window was closed.
        bool closeAll();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;

        friend class MainWindow;
    };

}

#endif // HELLOUTAU_EDITOR_EDITOR_H
