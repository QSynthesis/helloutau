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

namespace hello::kit {
    class FrequencyFormatRegistry;
}

namespace hello::daw {

    class AppSettings;
    class ProjectWindow;
    class SettingCatalog;
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

        /// The formats of frequency tables that the voice bank windows read, those registered
        /// by FrequencyFormatRegistration, the built-in ones by the plugin FrequencyEditor; see
        /// docs/FrequencyTables.md.
        kit::FrequencyFormatRegistry &frequencyFormats() const;

        /// The pages of the settings dialog, those of the editor registered at start; see the
        /// settings dialog in docs/Widgets.md.
        SettingCatalog *settingCatalog() const;

        /// Whether a voice bank window opened from now on follows the disk on its own, when a
        /// file changes, when it is activated and every minute; true by default. Tests turn it
        /// off, and call VoiceBankWindow::checkDisk() instead.
        bool watchesDisk() const;
        void setWatchesDisk(bool watches);

        /// The open project windows, in the order in which they were opened.
        QList<ProjectWindow *> windows() const;

        /// Opens a window with a new project.
        ProjectWindow *newWindow();

        /// Opens \a path. A window that already shows the file is activated instead. Otherwise
        /// the file opens in \a from if \a from shows an unmodified new project, and in a new
        /// window if not. Errors are shown to the user.
        ///
        /// \return the window that shows the file, or \c nullptr if it was not opened
        ProjectWindow *openFile(const std::filesystem::path &path, ProjectWindow *from = nullptr);

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

        /// Shows the settings over \a from, on the page of \a page if given, and each time they
        /// are applied, applies them to every project window: reads the voice banks anew if the
        /// UTAU folder changed, and plays in the playback mode chosen.
        void showSettings(QWidget *from, const QString &page = {});

        /// Closes every window, each asking to save its document first. Stops at the first
        /// window whose user cancels, and returns whether every window was closed.
        bool closeAll();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;

        friend class ProjectWindow;
    };

}

#endif // HELLOUTAU_EDITOR_EDITOR_H
