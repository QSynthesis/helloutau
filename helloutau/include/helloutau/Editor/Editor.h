#ifndef HELLOUTAU_EDITOR_EDITOR_H
#define HELLOUTAU_EDITOR_EDITOR_H

#include <filesystem>
#include <memory>

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <helloutau/Widgets/ActionContributionRegistry.h>
#include <helloutau/Widgets/KeyboardModifierBindings.h>
#include <helloutau/Widgets/SettingPageRegistry.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QMenu;
class QWidget;

namespace QAK {
    class ActionRegistry;
    class WidgetActionContext;
}

namespace hello::kit {
    class FrequencyFormats;
    class SynthDiagnosers;
}

namespace hello::daw {

    class AppSettings;
    class ProjectWindow;
    class SettingCatalog;
    class ThemeManager;
    class UpdateChecker;
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
            FrequencyMapEditorKind, ///< The dialog that edits frequency maps
        };

        /// The kinds of window in the order in which the settings list them
        static constexpr WindowKind windowKinds[] = {ProjectWindowKind, VoiceBankWindowKind,
                                                     FrequencyMapEditorKind};

        /// \name Names of the kinds of window
        ///
        /// The names by which an ActionContribution identifies the kinds of window, which are
        /// also the keys of their sections in the keymap and layout files.
        /// @{
        static constexpr char projectWindowName[] = "projectWindow";
        static constexpr char voiceBankWindowName[] = "voiceBankWindow";
        static constexpr char frequencyMapEditorName[] = "frequencyMapEditor";
        /// @}

        /// Returns the name of \a kind.
        static QString nameOf(WindowKind kind);

        AppSettings &settings() const;

        /// Returns the title of a window's project, disambiguated against the other project
        /// windows.
        QString projectDisplayName(const ProjectWindow *window) const;

        /// Returns the modifier schemes of the views in the windows of \a kind, in the order in
        /// which the settings list them.
        static QList<const KeyboardModifierScheme *> keyboardModifierSchemes(WindowKind kind);

        /// Returns the modifier bindings of the windows of \a kind, one for each of
        /// keyboardModifierSchemes() in its order. The bindings of one kind do not affect the
        /// other.
        QList<KeyboardModifierBindings> keyboardModifierBindings(WindowKind kind) const;

        /// Returns the bindings of \a scheme, which is one of keyboardModifierSchemes(), in the
        /// windows of \a kind.
        KeyboardModifierBindings
            keyboardModifierBindings(WindowKind kind, const KeyboardModifierScheme &scheme) const;

        /// Replaces the bindings of the windows of \a kind, one for each of
        /// keyboardModifierSchemes() in its order. The windows receive them through
        /// keyboardModifierBindingsChanged().
        void setKeyboardModifierBindings(WindowKind kind,
                                         const QList<KeyboardModifierBindings> &bindings);

    Q_SIGNALS:
        void keyboardModifierBindingsChanged(hello::daw::Editor::WindowKind kind);

    public:
        /// Returns the action registry of the windows of \a kind, with the menus, the tool bars,
        /// the commands and the shortcuts of that kind. Each window of the kind is a context of
        /// the registry. The shortcuts and the changes to the menus of one kind do not affect
        /// the other.
        QAK::ActionRegistry *actionRegistry(WindowKind kind) const;

        /// Returns the theme manager of all windows, with the built-in theme under
        /// \c :/helloutau/themes.
        ThemeManager *themeManager() const;

        /// Returns the frequency map formats that the voice bank windows read. The formats are
        /// registered in its registry(), the built-in formats by the plugin SynthToolSupport. See
        /// docs/FrequencyMaps.md.
        kit::FrequencyFormats &frequencyFormats() const;

        /// Returns the diagnosers of render failures that the project windows run. The editor
        /// registers the built-in diagnosers of HelloKitSynth, and plugins register others in its
        /// registry(). See docs/SynthDiagnostics.md.
        kit::SynthDiagnosers &synthDiagnosers() const;

        /// Returns the catalog of the pages of the settings dialog, including the pages of the
        /// editor registered at start, whose ids are the pageId of the classes in SettingPages. See
        /// the settings dialog in docs/Widgets.md.
        SettingCatalog *settingCatalog() const;

        /// Returns the registry of the action contributions that the windows of this editor
        /// apply. The editor creates one instance of each contribution. A plugin registers its
        /// contribution in this registry, and BuiltinActions registers the extensions of the
        /// editor itself.
        ActionContributionRegistry &actionContributions() const;

        /// Adds the actions of every contribution to \a context of \a window. A window calls this
        /// function when it is created.
        void addContributedActions(QWidget *window, QAK::WidgetActionContext *context) const;

        /// Returns the registry of the setting pages that this editor adds to settingCatalog().
        /// The editor creates the page of each entry once and deletes it when the entry is
        /// removed. A plugin registers its page in this registry.
        SettingPageRegistry &settingPages() const;

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

        /// Returns whether a voice bank window opened after the change monitors the disk
        /// automatically: when a file changes, when the window is activated, and every minute.
        /// The default is true. Tests disable the monitoring and call
        /// VoiceBankWindow::checkDisk() instead.
        bool watchesDisk() const;
        void setWatchesDisk(bool watches);

        /// Returns the open project windows, in the order of opening.
        QList<ProjectWindow *> windows() const;

        /// Opens a window with a new project.
        ProjectWindow *newWindow();

        /// Opens \a path. If a window already shows the file, that window is activated instead.
        /// Otherwise the file replaces the project of \a from, after the user is asked whether to
        /// save that project, or opens in a new window if \a from is \c nullptr. Errors are shown
        /// to the user.
        ///
        /// \return the window that shows the file, or \c nullptr if the file was not opened or
        ///         the user canceled the question to save
        ProjectWindow *openFile(const std::filesystem::path &path, ProjectWindow *from = nullptr);

        /// Opens \a paths, the first as openFile() does with \a from and the others in windows of
        /// their own. The user is asked once, before any file opens, whether to save the project
        /// of \a from. If the user cancels, no file opens. The installation archives among
        /// \a paths (InstallArchiveDialog::isInstallArchive()) are installed with
        /// installArchive() after the projects open, and need no question to save.
        ///
        /// \return the windows that show the files that were opened
        QList<ProjectWindow *> openFiles(const QList<std::filesystem::path> &paths,
                                         ProjectWindow *from);

        /// Installs the voice bank or the plugin of the installation archive \a path over
        /// \a from, see InstallArchiveDialog::install(). Once a voice bank is installed, the user
        /// is offered to open it.
        ///
        /// \return whether the archive was installed
        bool installArchive(const std::filesystem::path &path, QWidget *from = nullptr);

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

        /// Closes \a window and opens its voice bank again in a new window in its place, the undo
        /// history cleared. Intended after a change on disk that the window does not follow, such
        /// as file names converted on disk. The user is asked to save unsaved changes. The projects
        /// that sing the voice bank take the voice bank as read again.
        ///
        /// \return the new window, or \c nullptr if \a window was not closed or the voice bank
        ///         was not opened
        VoiceBankWindow *reopenVoiceBank(VoiceBankWindow *window);

        /// Fills \a menu, the "Open Recent" menu of a window: the ten latest projects, then the
        /// ten latest voice banks, and "Clear Recently Opened...", which calls clearRecent(). A
        /// section with ten items ends with a "More" item, which calls showRecent() for its kind.
        /// A project opens in \a from if \a from is an unused project window.
        void fillRecentMenu(QMenu *menu, QWidget *from);

        /// Shows a confirmation over \a from, as "Clear Recently Opened..." of VS Code does, and
        /// clears every recent project and voice bank if the user confirms. Returns whether they
        /// were cleared.
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
        /// offers a restart if an applied setting requires one (offerRestart()).
        void showSettings(QWidget *from, const QString &page = {});

        /// Offers the user a restart over \a parent, if an applied setting takes effect only
        /// after a restart (kit::ExitLauncher::isRestartRequired()). If the user
        /// agrees, closes every window as quitting does (closeAll()), and if all close, schedules
        /// the restart (kit::ExitLauncher::scheduleRestart()) and quits the application. Without
        /// an instance of kit::ExitLauncher, no restart is offered.
        ///
        /// \return whether the application quits to start again. A user who declines, or who
        ///         cancels the closing of a window, restarts later manually.
        bool offerRestart(QWidget *parent);

        /// Returns the check for newer releases of the application.
        UpdateChecker *updateChecker() const;

        /// Checks for a newer release over \a from, from the update source of the settings, and
        /// reports the result (Help > Check for Updates...).
        void checkForUpdates(QWidget *from);

        /// Holds the mutex for which the installer of an update waits, reports the result of the
        /// installation started when the application last quit, and checks for a newer release
        /// at start, each over the last opened project window. The core plugin calls this
        /// function once the first windows are open. See UpdateChecker.
        void checkForUpdatesAtStartup();

        /// Closes every window. Each window prompts the user to save its document first. Stops
        /// at the first window for which the user cancels, and returns whether every window was
        /// closed.
        bool closeAll();

    private:
        ProjectWindow *openFile(const std::filesystem::path &path, ProjectWindow *from,
                                bool askToSave);

        class Impl;
        std::unique_ptr<Impl> _impl;

        friend class ProjectWindow;
    };

}

#endif // HELLOUTAU_EDITOR_EDITOR_H
