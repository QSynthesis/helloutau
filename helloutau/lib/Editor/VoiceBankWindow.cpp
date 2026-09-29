#include "VoiceBankWindow.h"

#include <algorithm>
#include <functional>
#include <map>

#include <QtCore/QDir>
#include <QtCore/QRegularExpression>
#include <QtCore/QHash>
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtGui/QCloseEvent>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTreeWidgetItemIterator>
#include <QtWidgets/QVBoxLayout>

#include <stdcorelib/pimpl.h>

#include <QAKCore/actionregistry.h>
#include <QAKWidgets/widgetactioncontext.h>

#include <hellokit/Edit/VoiceBankDocument.h>
#include <hellokit/Edit/VoiceBankRefs.h>
#include <hellokit/VoiceBank/VoiceBankCheckScheduler.h>

#include <helloutau/Theme/ThemeManager.h>
#include <helloutau/Widgets/CommandPalette.h>

#include "AppSettings.h"
#include "CommandEntries_p.h"
#include "DiagnosticBox_p.h"
#include "Editor.h"
#include "VoiceBankCharsetDialog.h"
#include "VoiceBankEntryModel.h"

namespace hello::daw {

    namespace {

        QString textOf(const std::filesystem::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

        std::filesystem::path pathOf(const QString &text) {
            return std::filesystem::path(QDir::fromNativeSeparators(text).toStdU16String());
        }

        // The data of a folder item: its path relative to the root, which the item of all
        // folders lacks
        constexpr int PathRole = Qt::UserRole;
        constexpr int AllRole = Qt::UserRole + 1;

        // Keeps the rows whose file name or alias contains the search text.
        class EntryFilter : public QSortFilterProxyModel {
        public:
            using QSortFilterProxyModel::QSortFilterProxyModel;

        protected:
            bool filterAcceptsRow(int row, const QModelIndex &parent) const override {
                const auto text = filterRegularExpression().pattern();
                if (text.isEmpty()) {
                    return true;
                }
                const auto expression = filterRegularExpression();
                for (const int column :
                     {VoiceBankEntryModel::FileColumn, VoiceBankEntryModel::AliasColumn}) {
                    if (sourceModel()
                            ->index(row, column, parent)
                            .data()
                            .toString()
                            .contains(expression)) {
                        return true;
                    }
                }
                return false;
            }
        };

    }

    class VoiceBankWindow::Impl {
    public:
        using Decl = VoiceBankWindow;

        Impl(Decl *decl, Editor *editor) : _decl(decl), editor(editor) {
        }

        Decl *_decl;
        Editor *editor;
        std::unique_ptr<kit::VoiceBankDocument> document;
        QAK::WidgetActionContext *context = nullptr;
        QHash<QString, QAction *> actions;
        CommandPalette *palette = nullptr;
        QMenu *recentMenu = nullptr;

        // Follows the disk while the window is open; see checkDisk().
        kit::VoiceBankCheckScheduler *scheduler = nullptr;
        bool checking = false;
        // The changes of text files declined, with the stamp of the files at that time
        std::map<std::filesystem::path, QString> declined;
        // The changes that the bar lists
        kit::VoiceBankChanges pending;
        QWidget *bar = nullptr;
        QLabel *barText = nullptr;
        QPushButton *readChanged = nullptr;
        QPushButton *readAdded = nullptr;

        QTreeWidget *tree = nullptr;
        QTableView *table = nullptr;
        QLineEdit *search = nullptr;
        VoiceBankEntryModel *model = nullptr;
        QSortFilterProxyModel *proxy = nullptr;
        // The folders the tree shows, to rebuild it only when they change
        QList<std::filesystem::path> shownDirectories;
        QList<std::filesystem::path> shownExcluded;

        static QString tr(const char *text) {
            return VoiceBankWindow::tr(text);
        }

        QAction *addCommand(const QString &id, std::function<void()> handler) {
            stdc_decl_t;
            auto action = new QAction(&decl);
            QObject::connect(action, &QAction::triggered, &decl, std::move(handler));
            context->addAction(id, action);
            actions.insert(id, action);
            return action;
        }

        void initActions() {
            stdc_decl_t;
            context = new QAK::WidgetActionContext(&decl);
            context->addMenuBar(QStringLiteral("helloutau.voiceBankMenu"), decl.menuBar());

            addCommand(QStringLiteral("helloutau.file.new"), [this] { editor->newWindow(); });
            addCommand(QStringLiteral("helloutau.file.open"), [this] { open(); });
            addCommand(QStringLiteral("helloutau.file.openVoiceBank"), [this] {
                stdc_decl_t;
                const auto folder = QFileDialog::getExistingDirectory(&decl, tr("Open Voice Bank"));
                if (!folder.isEmpty()) {
                    editor->openVoiceBank(pathOf(folder), &decl);
                }
            });
            recentMenu = new QMenu(&decl);
            QObject::connect(recentMenu, &QMenu::aboutToShow, &decl, [this] {
                stdc_decl_t;
                editor->fillRecentMenu(recentMenu, &decl);
            });
            context->addAction(QStringLiteral("helloutau.file.openRecent"),
                               recentMenu->menuAction());
            addCommand(QStringLiteral("helloutau.file.save"), [this] {
                stdc_decl_t;
                decl.save();
            });
            addCommand(QStringLiteral("helloutau.file.saveAs"), [this] {
                stdc_decl_t;
                decl.saveAs();
            });
            addCommand(QStringLiteral("helloutau.file.close"), [this] {
                stdc_decl_t;
                decl.close();
            });
            addCommand(QStringLiteral("helloutau.file.quit"), [this] { editor->closeAll(); });
            addCommand(QStringLiteral("helloutau.edit.undo"),
                       [this] { document->session()->undo(); });
            addCommand(QStringLiteral("helloutau.edit.redo"),
                       [this] { document->session()->redo(); });
            addCommand(QStringLiteral("helloutau.view.commandPalette"), [this] {
                palette->setCommands(
                    commandEntriesOf(editor->actionRegistry(), context,
                                     QStringLiteral("helloutau.view.commandPalette")));
                palette->setRecentIds(editor->settings().recentCommands());
                palette->popup();
            });
            addCommand(QStringLiteral("helloutau.voiceBank.reloadAll"), [this] {
                stdc_decl_t;
                decl.reloadAll();
            });
            addCommand(QStringLiteral("helloutau.tools.settings"), [this] {
                stdc_decl_t;
                editor->showSettings(&decl);
            });

            const auto registry = editor->actionRegistry();
            registry->addContext(context);
            for (const auto element :
                 {QAK::AE_Layouts, QAK::AE_Texts, QAK::AE_Keymap, QAK::AE_Icons}) {
                registry->updateContext(element);
            }

            palette = new CommandPalette(&decl);
            QObject::connect(palette, &CommandPalette::commandActivated, &decl,
                             [this](const QString &id) {
                                 stdc_decl_t;
                                 editor->settings().addRecentCommand(id);
                                 // Run once the key press that chose it is over, since the command
                                 // may open a dialog. It may have been disabled in between.
                                 QTimer::singleShot(0, &decl, [this, id] {
                                     if (const auto action = context->action(id);
                                         action && action->isEnabled()) {
                                         action->trigger();
                                     }
                                 });
                             });
        }

        void open() {
            stdc_decl_t;
            const auto file = QFileDialog::getOpenFileName(
                &decl, tr("Open"), {},
                tr("Projects (*.usth *.ust);;HelloUtau projects (*.usth);;UTAU projects "
                   "(*.ust);;All files (*)"));
            if (!file.isEmpty()) {
                editor->openFile(pathOf(file));
            }
        }

        void initWidgets() {
            stdc_decl_t;
            tree = new QTreeWidget();
            tree->setHeaderHidden(true);
            tree->setColumnCount(1);

            model = new VoiceBankEntryModel(document->session(), &decl);
            proxy = new EntryFilter(&decl);
            proxy->setSourceModel(model);
            proxy->setSortRole(VoiceBankEntryModel::SortRole);
            proxy->setSortCaseSensitivity(Qt::CaseInsensitive);

            table = new QTableView();
            table->setModel(proxy);
            table->setSortingEnabled(true);
            table->sortByColumn(-1, Qt::AscendingOrder);
            table->setSelectionBehavior(QAbstractItemView::SelectRows);
            table->verticalHeader()->hide();
            table->horizontalHeader()->setStretchLastSection(false);

            search = new QLineEdit();
            search->setPlaceholderText(tr("Search file names and aliases"));
            search->setClearButtonEnabled(true);
            QObject::connect(search, &QLineEdit::textChanged, &decl, [this](const QString &text) {
                proxy->setFilterRegularExpression(QRegularExpression(
                    QRegularExpression::escape(text), QRegularExpression::CaseInsensitiveOption));
            });

            bar = new QFrame();
            static_cast<QFrame *>(bar)->setFrameShape(QFrame::StyledPanel);
            barText = new QLabel();
            barText->setWordWrap(true);
            readChanged = new QPushButton(tr("&Read Again"));
            readAdded = new QPushButton(tr("Read &New Folders"));
            auto barLayout = new QHBoxLayout(bar);
            barLayout->addWidget(barText, 1);
            barLayout->addWidget(readChanged);
            barLayout->addWidget(readAdded);
            bar->hide();
            QObject::connect(readChanged, &QPushButton::clicked, &decl, [this] {
                kit::VoiceBankChanges changes;
                changes.changed = pending.changed;
                changes.removed = pending.removed;
                read(changes);
            });
            QObject::connect(readAdded, &QPushButton::clicked, &decl, [this] {
                kit::VoiceBankChanges changes;
                changes.added = pending.added;
                read(changes);
            });

            auto right = new QWidget();
            auto layout = new QVBoxLayout(right);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->addWidget(bar);
            layout->addWidget(search);
            layout->addWidget(table);

            auto splitter = new QSplitter(Qt::Horizontal);
            splitter->addWidget(tree);
            splitter->addWidget(right);
            splitter->setStretchFactor(1, 1);
            splitter->setSizes({200, 760});
            decl.setCentralWidget(splitter);

            QObject::connect(tree, &QTreeWidget::currentItemChanged, &decl,
                             [this](QTreeWidgetItem *item) { showDirectoryOf(item); });
        }

        // The name of folder directory of the voice bank in a message
        QString folderName(const std::filesystem::path &directory) const {
            return directory.empty()
                       ? QString::fromStdU16String(document->rootPath().filename().u16string())
                       : QDir::toNativeSeparators(QString::fromStdU16String(directory.u16string()));
        }

        QString folderNames(const QList<std::filesystem::path> &directories) const {
            QStringList names;
            for (const auto &directory : directories) {
                names.push_back(folderName(directory));
            }
            return names.join(QStringLiteral(", "));
        }

        // The size and time of the text files of directory, by which a declined change is
        // known again, or "removed"
        QString stampOf(const std::filesystem::path &directory) const {
            namespace fs = std::filesystem;
            const auto folder = document->rootPath() / directory;
            std::error_code error;
            if (!fs::is_directory(folder, error)) {
                return QStringLiteral("removed");
            }
            QStringList parts;
            for (const auto &entry : fs::directory_iterator(folder, error)) {
                const auto name = QString::fromStdU16String(entry.path().filename().u16string());
                if (!name.endsWith(QLatin1String(".txt"), Qt::CaseInsensitive) &&
                    !name.endsWith(QLatin1String(".ini"), Qt::CaseInsensitive) &&
                    !name.endsWith(QLatin1String(".map"), Qt::CaseInsensitive)) {
                    continue;
                }
                std::error_code status;
                const auto size = entry.file_size(status);
                const auto time = entry.last_write_time(status).time_since_epoch().count();
                parts.push_back(QStringLiteral("%1:%2:%3").arg(name).arg(size).arg(time));
            }
            parts.sort();
            return parts.join(QLatin1Char('|'));
        }

        // Reads changes from the disk, one undo step unless only audio files changed.
        void read(const kit::VoiceBankChanges &changes) {
            stdc_decl_t;
            VoiceBankCharsetDialog selector(&decl);
            selector.setRoot(document->rootPath());
            kit::DiagnosticList diagnostics;
            document->reloadFromDisk(changes, &selector, diagnostics);
            DiagnosticBox::show(&decl, tr("Read from Disk"), diagnostics);
            for (const auto &directory : changes.changed) {
                declined.erase(directory);
            }
            for (const auto &directory : changes.removed) {
                declined.erase(directory);
            }
            check({});
        }

        // Handles what the disk holds that the voice bank does not, in places or everywhere.
        void check(const QList<std::filesystem::path> &places) {
            stdc_decl_t;
            if (checking) {
                return;
            }
            checking = true;
            auto changes = places.isEmpty() ? document->checkDisk() : document->checkDisk(places);

            // Audio files are taken as they are: nothing the user edited is replaced.
            if (!changes.audio.isEmpty()) {
                kit::VoiceBankChanges audio;
                audio.audio = changes.audio;
                kit::DiagnosticList diagnostics;
                document->reloadFromDisk(audio, nullptr, diagnostics);
                // Not an edit of the tree, which the model follows by itself
                model->refresh();
            }

            // A change of text not declined as it is now is asked about.
            QList<std::filesystem::path> ask;
            for (const auto &list : {changes.changed, changes.removed}) {
                for (const auto &directory : list) {
                    const auto found = declined.find(directory);
                    if (found == declined.end() || found->second != stampOf(directory)) {
                        ask.push_back(directory);
                    }
                }
            }
            if (!ask.isEmpty()) {
                const auto answer = QMessageBox::question(
                    &decl, tr("Changed on Disk"),
                    tr("These folders of the voice bank were changed by another program: %1.\n\n"
                       "Read them again? What you did not save in them is replaced, and Undo "
                       "brings it back.")
                        .arg(folderNames(ask)),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
                if (answer == QMessageBox::Yes) {
                    kit::VoiceBankChanges reread;
                    for (const auto &directory : ask) {
                        (changes.removed.contains(directory) ? reread.removed : reread.changed)
                            .push_back(directory);
                    }
                    checking = false;
                    read(reread);
                    return;
                }
                for (const auto &directory : ask) {
                    declined[directory] = stampOf(directory);
                }
            }

            pending = changes;
            updateBar();
            checking = false;
        }

        void updateBar() {
            QStringList lines;
            const auto changed = pending.changed + pending.removed;
            if (!changed.isEmpty()) {
                lines.push_back(tr("Changed on disk and not read: %1.").arg(folderNames(changed)));
            }
            if (!pending.added.isEmpty()) {
                lines.push_back(tr("New folders on disk: %1.").arg(folderNames(pending.added)));
            }
            if (pending.rootNotFound) {
                lines.push_back(tr("The folder of the voice bank no longer exists. Saving writes "
                                   "it again."));
            }
            barText->setText(lines.join(QLatin1Char('\n')));
            readChanged->setVisible(!changed.isEmpty());
            readAdded->setVisible(!pending.added.isEmpty());
            bar->setVisible(!lines.isEmpty());
        }

        void initScheduler() {
            stdc_decl_t;
            if (!editor->watchesDisk()) {
                return;
            }
            scheduler = new kit::VoiceBankCheckScheduler(&decl);
            QObject::connect(scheduler, &kit::VoiceBankCheckScheduler::checkNeeded, &decl,
                             [this](const QStringList &places) {
                                 QList<std::filesystem::path> paths;
                                 for (const auto &place : places) {
                                     paths.push_back(std::filesystem::path(place.toStdU16String()));
                                 }
                                 check(paths);
                             });
            followRoot();
        }

        void followRoot() {
            if (scheduler) {
                scheduler->setRoot(QDir::fromNativeSeparators(
                    QString::fromStdU16String(document->rootPath().u16string())));
            }
        }

        void showDirectoryOf(QTreeWidgetItem *item) {
            if (!item || item->data(0, AllRole).toBool()) {
                model->setDirectory(std::nullopt);
            } else {
                model->setDirectory(pathOf(item->data(0, PathRole).toString()));
            }
            table->setColumnHidden(VoiceBankEntryModel::DirectoryColumn,
                                   model->directory().has_value());
        }

        // Rebuilds the tree of folders if they changed, keeping the chosen folder where it
        // remains.
        void refreshTree() {
            QList<std::filesystem::path> directories;
            const auto list = kit::VoiceBankRef(document->session()).directories();
            for (int i = 0; i < list.size(); ++i) {
                directories.push_back(list.at(i).path());
            }
            QList<std::filesystem::path> excluded;
            for (const auto &directory : document->session()->excludedDirectories()) {
                excluded.push_back(directory.path);
            }
            if (directories == shownDirectories && excluded == shownExcluded &&
                tree->topLevelItemCount() > 0) {
                return;
            }
            shownDirectories = directories;
            shownExcluded = excluded;

            const auto chosen = model->directory();
            const QSignalBlocker blocker(tree);
            tree->clear();
            auto all = new QTreeWidgetItem(tree, {tr("All Folders")});
            all->setData(0, AllRole, true);

            QHash<QString, QTreeWidgetItem *> items;
            QTreeWidgetItem *current = all;
            const auto add = [&](const std::filesystem::path &path, bool read) {
                const auto key = QString::fromStdU16String(path.generic_u16string());
                QTreeWidgetItem *parent = nullptr;
                QString label = document->displayName();
                if (!path.empty()) {
                    const auto parentKey =
                        QString::fromStdU16String(path.parent_path().generic_u16string());
                    parent = items.value(parentKey);
                    label = QString::fromStdU16String(path.filename().u16string());
                }
                auto item = parent ? new QTreeWidgetItem(parent, {label})
                                   : new QTreeWidgetItem(tree, {label});
                item->setData(0, PathRole, textOf(path));
                if (!read) {
                    item->setDisabled(true);
                    item->setToolTip(0, tr("No encoding was chosen for this folder, so it was "
                                           "not read."));
                }
                items.insert(key, item);
                if (chosen && *chosen == path) {
                    current = item;
                }
            };
            // The paths are in order, a parent before its subfolders.
            auto every = directories;
            for (const auto &path : std::as_const(excluded)) {
                every.push_back(path);
            }
            std::sort(every.begin(), every.end());
            for (const auto &path : std::as_const(every)) {
                add(path, !excluded.contains(path));
            }
            tree->expandAll();
            tree->setCurrentItem(current);
            showDirectoryOf(current);
        }

        void updateTitle() {
            stdc_decl_t;
            decl.setWindowTitle(QStringLiteral("%1[*] - HelloUtau").arg(document->displayName()));
            decl.setWindowModified(document->isModified());
        }

        void updateUndoActions() {
            const auto session = document->session();
            actions.value(QStringLiteral("helloutau.edit.undo"))->setEnabled(session->canUndo());
            actions.value(QStringLiteral("helloutau.edit.redo"))->setEnabled(session->canRedo());
        }

        // Asks whether to save a modified voice bank before it is closed. Returns whether
        // closing may proceed.
        bool maybeSave() {
            stdc_decl_t;
            if (!document->isModified()) {
                return true;
            }
            const auto answer = QMessageBox::warning(
                &decl, tr("HelloUtau"), tr("Save the changes to %1?").arg(document->displayName()),
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
            if (answer == QMessageBox::Save) {
                return decl.save();
            }
            return answer == QMessageBox::Discard;
        }
    };

    VoiceBankWindow::VoiceBankWindow(Editor *editor,
                                     std::unique_ptr<kit::VoiceBankDocument> document)
        : _impl(std::make_unique<Impl>(this, editor)) {
        stdc_impl_t;
        impl.document = std::move(document);
        impl.initActions();
        impl.initWidgets();
        impl.refreshTree();
        impl.initScheduler();

        const auto session = impl.document->session();
        connect(session, &kit::VoiceBankSession::stepChanged, this, [this] {
            stdc_impl_t;
            impl.updateUndoActions();
            impl.refreshTree();
        });
        connect(impl.document.get(), &kit::VoiceBankDocument::modifiedChanged, this, [this] {
            stdc_impl_t;
            impl.updateTitle();
        });
        connect(impl.document.get(), &kit::VoiceBankDocument::rootPathChanged, this, [this] {
            stdc_impl_t;
            impl.updateTitle();
            impl.shownDirectories.clear();
            impl.refreshTree();
            impl.declined.clear();
            impl.followRoot();
        });
        impl.updateTitle();
        impl.updateUndoActions();
        editor->themeManager()->install(this, {QStringLiteral("VoiceBankWindow")});
        resize(960, 640);
    }

    VoiceBankWindow::~VoiceBankWindow() = default;

    kit::VoiceBankDocument *VoiceBankWindow::document() const {
        stdc_impl_t;
        return impl.document.get();
    }

    QTreeWidget *VoiceBankWindow::directoryTree() const {
        stdc_impl_t;
        return impl.tree;
    }

    QTableView *VoiceBankWindow::entryTable() const {
        stdc_impl_t;
        return impl.table;
    }

    VoiceBankEntryModel *VoiceBankWindow::entryModel() const {
        stdc_impl_t;
        return impl.model;
    }

    QLineEdit *VoiceBankWindow::searchBox() const {
        stdc_impl_t;
        return impl.search;
    }

    bool VoiceBankWindow::showEntryFor(int noteNum, const QString &lyric) {
        stdc_impl_t;
        const auto bank = impl.document->session()->snapshot();
        const auto sample = bank.find(noteNum, lyric);
        if (!sample) {
            return false;
        }
        const auto directory = bank.directories().at(sample->directory).path;

        // Its folder in the tree, which shows its entries
        const auto key = textOf(directory);
        for (QTreeWidgetItemIterator it(impl.tree); *it; ++it) {
            if (!(*it)->data(0, AllRole).toBool() && (*it)->data(0, PathRole).toString() == key) {
                impl.tree->setCurrentItem(*it);
                break;
            }
        }
        impl.search->clear();
        const int row = impl.model->rowOf(directory, sample->fileName, sample->alias);
        if (row < 0) {
            return false;
        }
        const auto index =
            impl.proxy->mapFromSource(impl.model->index(row, VoiceBankEntryModel::FileColumn));
        impl.table->setCurrentIndex(index);
        impl.table->scrollTo(index);
        raise();
        activateWindow();
        return true;
    }

    bool VoiceBankWindow::save() {
        stdc_impl_t;
        kit::DiagnosticList diagnostics;
        const bool saved = impl.document->save(diagnostics);
        DiagnosticBox::show(this, tr("Save"), diagnostics);
        return saved;
    }

    void VoiceBankWindow::checkDisk() {
        stdc_impl_t;
        impl.check({});
    }

    QWidget *VoiceBankWindow::changeBar() const {
        stdc_impl_t;
        return impl.bar;
    }

    bool VoiceBankWindow::reloadAll() {
        stdc_impl_t;
        VoiceBankCharsetDialog selector(this);
        selector.setRoot(impl.document->rootPath());
        kit::DiagnosticList diagnostics;
        impl.document->reloadAllFromDisk(&selector, diagnostics);
        DiagnosticBox::show(this, tr("Read All from Disk"), diagnostics);
        impl.declined.clear();
        impl.check({});
        return !kit::hasError(diagnostics);
    }

    void VoiceBankWindow::changeEvent(QEvent *event) {
        stdc_impl_t;
        QMainWindow::changeEvent(event);
        // Back from another program, which may have changed the files
        if (event->type() == QEvent::ActivationChange && isActiveWindow() && impl.scheduler) {
            impl.scheduler->requestFull();
        }
    }

    bool VoiceBankWindow::saveAs() {
        stdc_impl_t;
        const auto folder = QFileDialog::getExistingDirectory(
            this, tr("Save Voice Bank As (an empty folder)"),
            QDir::toNativeSeparators(textOf(impl.document->rootPath().parent_path())));
        if (folder.isEmpty()) {
            return false;
        }
        kit::DiagnosticList diagnostics;
        const bool saved =
            impl.document->saveAs(pathOf(folder), kit::VoiceBankSession::AllFiles, diagnostics);
        if (saved) {
            impl.editor->settings().addRecentVoiceBank(impl.document->rootPath());
        }
        DiagnosticBox::show(this, tr("Save As"), diagnostics);
        return saved;
    }

    void VoiceBankWindow::closeEvent(QCloseEvent *event) {
        stdc_impl_t;
        if (!impl.maybeSave()) {
            event->ignore();
            return;
        }
        QMainWindow::closeEvent(event);
    }

}
