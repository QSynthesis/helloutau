#include "Editor.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include <QtCore/QDir>
#include <QtCore/QPointer>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

#include <stdcorelib/pimpl.h>

#include <QAKCore/actionregistry.h>
#include <QAKWidgets/widgetactioncontext.h>

#include <hellokit/Edit/ProjectDocument.h>
#include <hellokit/Edit/VoiceBankDocument.h>
#include <hellokit/VoiceBank/FrequencyFormatRegistry.h>
#include <hellokit/VoiceBank/VoiceBank.h>

#include <helloutau/Theme/ThemeManager.h>
#include <helloutau/Audio/AudioOutput.h>
#include <helloutau/Widgets/CommandPalette.h>
#include <helloutau/Widgets/SettingPage.h>
#include <helloutau/Widgets/SettingsDialog.h>

#include "ActionRegistrations_p.h"
#include "AppSettings.h"
#include "DiagnosticBox_p.h"
#include "EditorIcons_p.h"
#include "EditorSettingPages_p.h"
#include "KeymapFile_p.h"
#include "ActionLayoutsFile_p.h"
#include "ProjectWindow.h"
#include "Restarter.h"
#include "UstCharsetDialog.h"
#include "VoiceBankCharsetDialog.h"
#include "VoiceBankWindow.h"

namespace hello::daw {

    namespace {

        // The number of recent projects, and of recent voice banks, listed in "Open Recent". The
        // palette of each kind lists every item that the settings keep.
        constexpr int RecentMenuCount = 10;

        // The palette of the recent items, and the prefixes of its entries by kind
        constexpr char RecentPaletteName[] = "recentPalette";
        const QString ProjectPrefix = QStringLiteral("project:");
        const QString VoiceBankPrefix = QStringLiteral("voicebank:");

        QString textOf(const std::filesystem::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

        bool isSameFile(const std::filesystem::path &a, const std::filesystem::path &b) {
            if (a.empty() || b.empty()) {
                return false;
            }
            std::error_code error;
            return std::filesystem::equivalent(a, b, error);
        }

        void placeNewWindow(QWidget *window, QWidget *previous) {
            if (!previous) {
                return;
            }
            constexpr int CascadeStep = 32;
            const auto wanted = previous->frameGeometry().topLeft() + QPoint(CascadeStep, CascadeStep);
            const auto screen = QGuiApplication::screenAt(wanted);
            if (!screen) {
                window->move(wanted);
                return;
            }
            const auto available = screen->availableGeometry();
            const int right = std::max(available.left(),
                                       available.right() - window->frameGeometry().width() + 1);
            const int bottom = std::max(available.top(),
                                        available.bottom() - window->frameGeometry().height() + 1);
            const int x = std::clamp(wanted.x(), available.left(), right);
            const int y = std::clamp(wanted.y(), available.top(), bottom);
            window->move(x, y);
        }

    }

    class Editor::Impl : public ActionRegistrations::Listener {
    public:
        using Decl = Editor;

        // The settings of \a owned, or else \a settings, which another object owns
        Impl(std::unique_ptr<AppSettings> owned, AppSettings *settings)
            : ownedSettings(std::move(owned)),
              settings(ownedSettings ? ownedSettings.get() : settings) {
        }

        void init(Editor *decl) {
            AudioOutput::setOutputDeviceId(settings->audioOutputDevice());
            // A registry for each kind of window. The extensions of the editor itself come as
            // contributions too, see BuiltinActions.
            for (const auto kind : Editor::windowKinds) {
                const auto registry = new QAK::ActionRegistry(decl);
                registries[kind] = registry;
                for (const auto contribution : ActionRegistrations::instance().contributions()) {
                    if (const auto extension = contribution->extension(kind)) {
                        registry->addExtension(extension);
                    }
                }
                addEditorIcons(registry);
            }
            ActionRegistrations::instance().addListener(this);
            // The shortcuts that the user assigned and the changes to the menus and tool bars,
            // each in a file of its own beside the settings with a section for each kind
            keymapFile = KeymapFile::fileNameFor(settings->fileName());
            KeymapFile::read(sections(), keymapFile);
            actionLayoutsFile = ActionLayoutsFile::fileNameFor(settings->fileName());
            ActionLayoutsFile::read(sections(), actionLayoutsFile);
            themes = new ThemeManager(decl);
            themes->addSearchPath(QStringLiteral(":/helloutau/themes"));
            catalog = new SettingCatalog(decl);
            addEditorSettingPages(catalog, *settings);
        }

        // The windows take the actions of a contribution as it comes, and the menus and
        // shortcuts are rebuilt with the extension of the contribution
        void contributionAdded(ActionContribution *contribution) override {
            for (const auto kind : Editor::windowKinds) {
                if (const auto extension = contribution->extension(kind)) {
                    registries[kind]->addExtension(extension);
                }
            }
            for (const auto &window : std::as_const(windows)) {
                if (window) {
                    contribution->addActions(window.data(), window->actionContext());
                }
            }
            for (const auto &window : std::as_const(voiceBankWindows)) {
                if (window) {
                    contribution->addActions(window.data(), window->actionContext());
                }
            }
            updateContexts();
        }

        void contributionRemoved(ActionContribution *contribution) override {
            if (const auto extension = contribution->extension(Editor::ProjectWindowKind)) {
                for (const auto &window : std::as_const(windows)) {
                    if (window) {
                        ActionRegistrations::removeActions(extension, window->actionContext());
                    }
                }
                registries[Editor::ProjectWindowKind]->removeExtension(extension);
            }
            if (const auto extension = contribution->extension(Editor::VoiceBankWindowKind)) {
                for (const auto &window : std::as_const(voiceBankWindows)) {
                    if (window) {
                        ActionRegistrations::removeActions(extension, window->actionContext());
                    }
                }
                registries[Editor::VoiceBankWindowKind]->removeExtension(extension);
            }
            updateContexts();
        }

        void updateContexts() const {
            for (const auto registry : registries) {
                for (const auto element :
                     {QAK::AE_Layouts, QAK::AE_Texts, QAK::AE_Keymap, QAK::AE_Icons}) {
                    registry->updateContext(element);
                }
            }
        }

        // The registries with the keys of their sections in the keymap and layout files
        QList<std::pair<QString, QAK::ActionRegistry *>> sections() const {
            return {
                {QStringLiteral("projectWindow"),   registries[Editor::ProjectWindowKind]  },
                {QStringLiteral("voiceBankWindow"), registries[Editor::VoiceBankWindowKind]},
            };
        }

        std::unique_ptr<AppSettings> ownedSettings;
        AppSettings *settings;
        // The registry of each kind of window, by Editor::WindowKind
        QAK::ActionRegistry *registries[std::size(Editor::windowKinds)] = {};
        // The files of the shortcuts and of the changes to the menus, beside the settings
        QString keymapFile;
        QString actionLayoutsFile;
        ThemeManager *themes = nullptr;
        SettingCatalog *catalog = nullptr;
        std::unique_ptr<kit::FrequencyFormatRegistry> frequencyFormats =
            std::make_unique<kit::FrequencyFormatRegistry>();
        bool watchesDisk = true;
        QList<QPointer<ProjectWindow>> windows;
        QList<QPointer<VoiceBankWindow>> voiceBankWindows;

        // Opens a recent project, or voice bank if bank, over from, or forgets it if it is gone
        void openRecent(Editor *editor, const std::filesystem::path &path, bool bank,
                        QWidget *from) {
            std::error_code error;
            if (bank ? !std::filesystem::is_directory(path, error)
                     : !std::filesystem::is_regular_file(path, error)) {
                QMessageBox::warning(
                    from, Editor::tr("Open Recent"),
                    Editor::tr("%1 no longer exists.").arg(QDir::toNativeSeparators(textOf(path))));
                if (bank) {
                    settings->removeRecentVoiceBank(path);
                } else {
                    settings->removeRecentFile(path);
                }
                return;
            }
            if (bank) {
                editor->openVoiceBank(path, from);
            } else {
                editor->openFile(path, qobject_cast<ProjectWindow *>(from));
            }
        }

        // Shows every recent item of kind in a command palette over from, the latest first
        void showRecent(Editor *editor, RecentKind kind, QWidget *from) {
            // The path of an item follows its prefix in the identifier.
            const auto pathOf = [](const QString &id) {
                const bool bank = id.startsWith(VoiceBankPrefix);
                const auto text = id.mid(bank ? VoiceBankPrefix.size() : ProjectPrefix.size());
                return std::pair{std::filesystem::path(text.toStdU16String()), bank};
            };
            auto palette = from->findChild<CommandPalette *>(RecentPaletteName);
            if (!palette) {
                palette = new CommandPalette(from);
                palette->setObjectName(RecentPaletteName);
                palette->setOrder(CommandMatcher::AsGiven);
                palette->setRemovable(true);
                QObject::connect(palette, &CommandPalette::commandActivated, from,
                                 [this, editor, from, pathOf](const QString &id) {
                                     const auto [path, bank] = pathOf(id);
                                     openRecent(editor, path, bank, from);
                                 });
                QObject::connect(palette, &CommandPalette::commandRemoved, from,
                                 [this, pathOf](const QString &id) {
                                     const auto [path, bank] = pathOf(id);
                                     if (bank) {
                                         settings->removeRecentVoiceBank(path);
                                     } else {
                                         settings->removeRecentFile(path);
                                     }
                                 });
            }
            palette->setPlaceholderText(kind == RecentProjects
                                            ? Editor::tr("Select a recent project to open")
                                            : Editor::tr("Select a recent voice bank to open"));

            QList<CommandEntry> entries;
            const auto add = [&](const QString &prefix, const std::filesystem::path &path) {
                CommandEntry e;
                e.id = prefix + textOf(path);
                e.label = QString::fromStdU16String(path.filename().u16string());
                e.description = QDir::toNativeSeparators(textOf(path.parent_path()));
                entries.push_back(e);
            };
            if (kind == RecentProjects) {
                for (const auto &path : settings->recentFiles()) {
                    add(ProjectPrefix, path);
                }
            } else {
                for (const auto &path : settings->recentVoiceBanks()) {
                    add(VoiceBankPrefix, path);
                }
            }
            palette->setCommands(entries);
            palette->setRecentIds({});
            palette->popup();
        }

        ProjectWindow *createWindow(Editor *editor, std::unique_ptr<kit::ProjectDocument> document) {
            ProjectWindow *previous = nullptr;
            for (auto it = windows.crbegin(); it != windows.crend(); ++it) {
                if (*it) {
                    previous = *it;
                    break;
                }
            }
            auto window = new ProjectWindow(editor, std::move(document));
            window->setAttribute(Qt::WA_DeleteOnClose);
            windows.removeAll(nullptr);
            windows.push_back(window);
            placeNewWindow(window, previous);
            window->show();
            return window;
        }
    };

    Editor::Editor(QObject *parent) : Editor(std::make_unique<AppSettings>(), parent) {
    }

    Editor::Editor(std::unique_ptr<AppSettings> settings, QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(std::move(settings), nullptr)) {
        _impl->init(this);
    }

    Editor::Editor(AppSettings &settings, QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(nullptr, &settings)) {
        _impl->init(this);
    }

    kit::FrequencyFormatRegistry &Editor::frequencyFormats() const {
        stdc_impl_t;
        return *impl.frequencyFormats;
    }

    SettingCatalog *Editor::settingCatalog() const {
        stdc_impl_t;
        return impl.catalog;
    }

    bool Editor::saveKeymap(QString *error) const {
        stdc_impl_t;
        return KeymapFile::write(impl.sections(), impl.keymapFile, error);
    }

    bool Editor::saveActionLayouts(QString *error) const {
        stdc_impl_t;
        return ActionLayoutsFile::write(impl.sections(), impl.actionLayoutsFile, error);
    }

    bool Editor::watchesDisk() const {
        stdc_impl_t;
        return impl.watchesDisk;
    }

    void Editor::setWatchesDisk(bool watches) {
        stdc_impl_t;
        impl.watchesDisk = watches;
    }

    Editor::~Editor() {
        stdc_impl_t;
        ActionRegistrations::instance().removeListener(&impl);
        // The windows refer to the registries and the settings, so they go first.
        for (const auto &window : std::as_const(impl.windows)) {
            delete window.data();
        }
        for (const auto &window : std::as_const(impl.voiceBankWindows)) {
            delete window.data();
        }
    }

    AppSettings &Editor::settings() const {
        stdc_impl_t;
        return *impl.settings;
    }

    QAK::ActionRegistry *Editor::actionRegistry(WindowKind kind) const {
        stdc_impl_t;
        return impl.registries[kind];
    }

    ThemeManager *Editor::themeManager() const {
        stdc_impl_t;
        return impl.themes;
    }

    QList<ProjectWindow *> Editor::windows() const {
        stdc_impl_t;
        QList<ProjectWindow *> result;
        for (const auto &window : std::as_const(impl.windows)) {
            if (window) {
                result.push_back(window);
            }
        }
        return result;
    }

    ProjectWindow *Editor::newWindow() {
        stdc_impl_t;
        return impl.createWindow(this, std::make_unique<kit::ProjectDocument>());
    }

    ProjectWindow *Editor::openFile(const std::filesystem::path &path, ProjectWindow *from) {
        stdc_impl_t;
        for (const auto window : windows()) {
            if (isSameFile(window->document()->sourcePath(), path)) {
                window->raise();
                window->activateWindow();
                return window;
            }
        }

        if (from && !from->maybeSave()) {
            return nullptr;
        }

        UstCharsetDialog selector(from);
        kit::DiagnosticList diagnostics;
        auto document = kit::ProjectDocument::open(path, &selector, diagnostics);
        const auto title =
            tr("Open %1").arg(QString::fromStdU16String(path.filename().u16string()));
        if (!document) {
            DiagnosticBox::show(from, title, diagnostics);
            return nullptr;
        }
        ProjectWindow *window = from;
        if (window) {
            window->setDocument(std::move(document));
        } else {
            window = impl.createWindow(this, std::move(document));
        }
        impl.settings->addRecentFile(path);
        // Shown after the window, so that the user sees which project they concern, and before
        // the voice bank is read, which may ask more.
        DiagnosticBox::show(window, title, diagnostics);
        window->loadVoiceBank();
        window->showPropertiesIfPathsAreInvalid();
        return window;
    }

    QList<VoiceBankWindow *> Editor::voiceBankWindows() const {
        stdc_impl_t;
        QList<VoiceBankWindow *> result;
        for (const auto &window : std::as_const(impl.voiceBankWindows)) {
            if (window) {
                result.push_back(window);
            }
        }
        return result;
    }

    VoiceBankWindow *Editor::openVoiceBank(const std::filesystem::path &root, QWidget *from) {
        stdc_impl_t;
        for (const auto window : voiceBankWindows()) {
            if (isSameFile(window->document()->rootPath(), root)) {
                window->raise();
                window->activateWindow();
                return window;
            }
        }
        VoiceBankCharsetDialog selector(from);
        selector.setRoot(root);
        kit::DiagnosticList diagnostics;
        auto document = kit::VoiceBankDocument::open(root, &selector, diagnostics);
        const auto title = tr("Open %1").arg(QDir::toNativeSeparators(textOf(root)));
        if (!document) {
            DiagnosticBox::show(from, title, diagnostics);
            return nullptr;
        }
        impl.settings->addRecentVoiceBank(root);

        QWidget *previous = nullptr;
        for (auto it = impl.voiceBankWindows.crbegin(); it != impl.voiceBankWindows.crend(); ++it) {
            if (*it) {
                previous = *it;
                break;
            }
        }
        if (!previous) {
            previous = from;
        }
        auto window = new VoiceBankWindow(this, std::move(document));
        window->setAttribute(Qt::WA_DeleteOnClose);
        // The projects that sing the voice bank take it as saved (docs/Editing.md).
        const auto saved = window->document();
        QObject::connect(saved, &kit::VoiceBankDocument::saved, window, [this, saved] {
            std::shared_ptr<const kit::VoiceBank> bank;
            for (const auto project : windows()) {
                const auto sung = project->document()->voiceBank();
                if (sung && isSameFile(sung->root(), saved->rootPath())) {
                    if (!bank) {
                        bank = std::make_shared<const kit::VoiceBank>(saved->session()->snapshot());
                    }
                    project->document()->setVoiceBank(bank);
                }
            }
        });
        impl.voiceBankWindows.removeAll(nullptr);
        impl.voiceBankWindows.push_back(window);
        placeNewWindow(window, previous);
        window->show();
        DiagnosticBox::show(window, title, diagnostics);
        return window;
    }

    void Editor::fillRecentMenu(QMenu *menu, QWidget *from) {
        stdc_impl_t;
        menu->clear();
        const auto files = impl.settings->recentFiles();
        const auto banks = impl.settings->recentVoiceBanks();
        if (files.isEmpty() && banks.isEmpty()) {
            menu->addAction(tr("No Recent Files"))->setEnabled(false);
            return;
        }
        // Numbered 1 to 9 and then 0, as the keys of the first ten items
        int number = 0;
        const auto add = [&](const std::filesystem::path &path, bool bank) {
            const auto text = QString(QDir::toNativeSeparators(textOf(path)))
                                  .replace(QLatin1Char('&'), QStringLiteral("&&"));
            const auto action = menu->addAction(
                ++number <= 10 ? QStringLiteral("&%1 %2").arg(number % 10).arg(text) : text);
            connect(action, &QAction::triggered, menu, [this, path, bank, from] {
                stdc_impl_t;
                impl.openRecent(this, path, bank, from);
            });
        };
        // A section with ten items ends with the palette of all items of its kind.
        const auto more = [&](const QString &text, RecentKind kind) {
            connect(menu->addAction(text), &QAction::triggered, menu,
                    [this, kind, from] { showRecent(kind, from); });
        };
        for (const auto &path : files.mid(0, RecentMenuCount)) {
            add(path, false);
        }
        if (files.size() >= RecentMenuCount) {
            more(tr("More &Projects..."), RecentProjects);
        }
        if (!files.isEmpty() && !banks.isEmpty()) {
            menu->addSeparator();
        }
        for (const auto &path : banks.mid(0, RecentMenuCount)) {
            add(path, true);
        }
        if (banks.size() >= RecentMenuCount) {
            more(tr("More &Voice Banks..."), RecentVoiceBanks);
        }
        menu->addSeparator();
        connect(menu->addAction(tr("&Clear Recently Opened...")), &QAction::triggered, menu,
                [this, from] { clearRecent(from); });
    }

    bool Editor::clearRecent(QWidget *from) {
        stdc_impl_t;
        QMessageBox box(QMessageBox::Warning, tr("Clear Recently Opened"),
                        tr("Clear the recently opened projects and voice banks?"),
                        QMessageBox::Cancel, from);
        box.setInformativeText(tr("This action cannot be undone."));
        const auto clear = box.addButton(tr("&Clear"), QMessageBox::DestructiveRole);
        box.setDefaultButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() != clear) {
            return false;
        }
        impl.settings->clearRecentFiles();
        impl.settings->clearRecentVoiceBanks();
        return true;
    }

    void Editor::showRecent(RecentKind kind, QWidget *from) {
        stdc_impl_t;
        impl.showRecent(this, kind, from);
    }

    void Editor::showSettings(QWidget *from, const QString &page) {
        stdc_impl_t;
        auto utau = settings().utauDirectory();
        SettingsDialog dialog(impl.catalog, from);
        if (!page.isEmpty()) {
            dialog.selectPage(page);
        }
        connect(&dialog, &SettingsDialog::applied, this, [this, &utau] {
            // Every voice bank named relative to UTAU is now elsewhere.
            const bool moved = settings().utauDirectory() != utau;
            utau = settings().utauDirectory();
            for (const auto window : windows()) {
                if (moved) {
                    window->loadVoiceBank();
                }
                window->applySettings();
            }
        });
        dialog.exec();
        Restarter::offer(from, this);
    }

    bool Editor::closeAll() {
        for (const auto window : windows()) {
            if (!window->close()) {
                return false;
            }
        }
        for (const auto window : voiceBankWindows()) {
            if (!window->close()) {
                return false;
            }
        }
        return true;
    }

}
