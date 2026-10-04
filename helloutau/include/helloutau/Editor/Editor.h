#ifndef HELLOUTAU_EDITOR_EDITOR_H
#define HELLOUTAU_EDITOR_EDITOR_H

#include <filesystem>
#include <memory>

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>
#include <helloutau/Editor/EditorModifierBindings.h>

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

    /// The editor of the application. It holds the settings, the actions shared by every window,
    /// the themes, and the windows. Each window edits one project or one voice bank.
    class HELLOUTAU_EDITOR_EXPORT Editor : public QObject {
        Q_OBJECT
    public:
        /// Constructs an editor with the settings of the current user.
        explicit Editor(QObject *parent = nullptr);

        /// Constructs an editor with \a settings, for tests.
        explicit Editor(std::unique_ptr<AppSettings> settings, QObject *parent = nullptr);

        /// Constructs an editor with \a settings, which another object owns and which outlive
        /// the editor. The core plugin passes the settings of AppLoader in this way.
        explicit Editor(AppSettings &settings, QObject *parent = nullptr);

        ~Editor();

        /// The kinds of window, each with an action registry of its own
        enum WindowKind {
            ProjectWindowKind,
            VoiceBankWindowKind,
        };

        /// The kinds of window in the order in which the settings list them
        static constexpr WindowKind windowKinds[] = {ProjectWindowKind, VoiceBankWindowKind};

        AppSettings &settings() const;

        /// Returns the title of a window's project, disambiguated against the other project
        /// windows.
        QString projectDisplayName(const ProjectWindow *window) const;

        EditorModifierBindings modifierBindings() const;
        void setModifierBindings(const EditorModifierBindings &bindings);

    Q_SIGNALS:
        void modifierBindingsChanged();

    public:
        /// Returns the action registry of the windows of \a kind, with the menus, the tool bars,
        /// the commands and the shortcuts of that kind. Each window of the kind is a context of
        /// the registry. The shortcuts and the changes to the menus of one kind do not affect
        /// the other.
        QAK::ActionRegistry *actionRegistry(WindowKind kind) const;

        /// Returns the theme manager of all windows, with the built-in theme under
        /// \c :/helloutau/themes.
        ThemeManager *themeManager() const;

        /// Returns the registry of the frequency table formats that the voice bank windows read.
        /// The formats are registered by FrequencyFormatRegistration, the built-in formats by
        /// the plugin FrequencyEditor. See docs/FrequencyTables.md.
        kit::FrequencyFormatRegistry &frequencyFormats() const;

        /// Returns the catalog of the pages of the settings dialog, including the pages of the
        /// editor registered at start, whose ids are in EditorSettingPageIds. See the settings
        /// dialog in docs/Widgets.md.
        SettingCatalog *settingCatalog() const;

        /// Writes the shortcuts that the action registries override to \c keymap.json beside the
        /// settings, a section for each kind of window, which the next editor reads at start.
        /// See the keymap in docs/Widgets.md.
        ///
        /// \return whether the file is written, with the reason in \a error otherwise
        bool saveKeymap(QString *error) const;

        /// Writes the changes of the action registries to the default layouts of the menus and
        /// the tool bars to \c actionLayouts.json beside the settings, a section for each kind of
        /// window, which the next editor reads at start. See the menus and tool bars in
        /// docs/Widgets.md.
        ///
        /// \return whether the file is written, with the reason in \a error otherwise
        bool saveActionLayouts(QString *error) const;

        /// Whether a voice bank window opened after the change monitors the disk automatically:
        /// when a file changes, when the window is activated, and every minute. The default is
        /// true. Tests disable the monitoring and call VoiceBankWindow::checkDisk() instead.
        bool watchesDisk() const;
        void setWatchesDisk(bool watches);

        /// Returns the open project windows, in the order of opening.
        QList<ProjectWindow *> windows() const;

        /// Opens a window with a new project.
        ProjectWindow *newWindow();

        /// Opens \a path. If a window already shows the file, that window is activated instead.
        /// Otherwise the file opens in \a from if \a from shows an unmodified new project, and in
        /// a new window if not. Errors are shown to the user.
        ///
        /// \return the window that shows the file, or \c nullptr if the file was not opened
        ProjectWindow *openFile(const std::filesystem::path &path, ProjectWindow *from = nullptr);

        /// Returns the open voice bank windows, in the order of opening.
        QList<VoiceBankWindow *> voiceBankWindows() const;

        /// Opens the voice bank in the folder \a root in a separate window. If a window already
        /// shows the voice bank, that window is activated instead. The user is prompted for the
        /// encoding of each folder that does not record its encoding. Prompts and errors are
        /// shown over \a from.
        ///
        /// \return the window that shows the voice bank, or \c nullptr if the voice bank was not
        /// opened
        VoiceBankWindow *openVoiceBank(const std::filesystem::path &root, QWidget *from = nullptr);

        /// Fills \a menu, the "Open Recent" menu of a window: the ten latest projects, then the
        /// ten latest voice banks, and "Clear Recently Opened...", which calls clearRecent(). A
        /// section with ten items ends with a "More" item, which calls showRecent() for its kind.
        /// A project opens in \a from if \a from is an unused project window.
        void fillRecentMenu(QMenu *menu, QWidget *from);

        /// Asks over \a from whether to forget every recent project and voice bank, as "Clear
        /// Recently Opened..." of VS Code asks, and forgets them if the user confirms. Returns
        /// whether they were forgotten.
        bool clearRecent(QWidget *from);

        /// The kind of recent items that showRecent() lists
        enum RecentKind {
            RecentProjects,
            RecentVoiceBanks,
        };

        /// Shows every recent item of \a kind that the settings keep, the latest first, in a
        /// command palette over \a from, as "More..." of "Open Recent" in VS Code: the name of
        /// each item with the folder that contains it after the name, both matched by the query,
        /// and the latest first while the user types. The chosen item opens as from "Open
        /// Recent", and the button of an item removes it from the recent items.
        void showRecent(RecentKind kind, QWidget *from);

        /// Shows the settings over \a from, on the page \a page if given. Each time the settings
        /// are applied, applies them to every project window: rereads the voice banks if the
        /// UTAU folder changed, and plays in the selected playback mode. Once the dialog closes,
        /// offers a restart if an applied setting requires one (Restarter::offer()).
        void showSettings(QWidget *from, const QString &page = {});

        /// Closes every window. Each window prompts the user to save its document first. Stops
        /// at the first window for which the user cancels, and returns whether every window was
        /// closed.
        bool closeAll();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;

        friend class ProjectWindow;
    };

}

#endif // HELLOUTAU_EDITOR_EDITOR_H
