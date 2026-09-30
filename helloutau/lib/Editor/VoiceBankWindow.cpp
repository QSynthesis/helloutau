#include "VoiceBankWindow.h"

#include <algorithm>
#include <functional>
#include <map>

#include <QtCore/QDir>
#include <QtCore/QRegularExpression>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QSignalBlocker>
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtGui/QCloseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTreeWidgetItemIterator>
#include <QtWidgets/QVBoxLayout>

#include <stdcorelib/pimpl.h>

#include <QAKCore/actionregistry.h>
#include <QAKWidgets/widgetactioncontext.h>

#include <hellokit/Edit/VoiceBankDocument.h>
#include <hellokit/Edit/VoiceBankEdits.h>
#include <hellokit/Edit/VoiceBankRefs.h>
#include <hellokit/Support/TextCodec.h>
#include <hellokit/Synth/Spectrogram.h>
#include <hellokit/Synth/WaveAudio.h>
#include <hellokit/VoiceBank/FrequencyFormatRegistry.h>
#include <hellokit/VoiceBank/VoiceBankCheckScheduler.h>
#include <hellokit/VoiceBank/WaveMetadata.h>

#include <helloutau/Theme/ThemeManager.h>
#include <helloutau/Widgets/CommandPalette.h>

#include "ActionRegistrations_p.h"
#include "VoiceAliasRuleDialog.h"
#include "AppSettings.h"
#include "CommandEntries_p.h"
#include "DiagnosticBox_p.h"
#include "Editor.h"
#include "OtoWaveformView.h"
#include "SamplePreview.h"
#include "VoiceBankCharsetDialog.h"
#include "VoiceBankEntryModel.h"
#include "VoiceBankInfoPanel.h"

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
        OtoWaveformView *waveform = nullptr;
        QVBoxLayout *lowerLayout = nullptr;
        VoiceBankInfoPanel *info = nullptr;
        // The folder of the context menu of the tree while it is open
        std::optional<std::filesystem::path> menuFolder;
        SamplePreview *preview = nullptr;
        QComboBox *pitchBox = nullptr;
        QComboBox *frequencyBox = nullptr;
        // The audio file and the format of the frequency table shown, and the spectrogram of
        // the audio it was computed from
        std::pair<std::filesystem::path, QString> frequencyShown;
        std::shared_ptr<const kit::Spectrogram> spectrum;
        std::shared_ptr<const kit::WaveAudio> spectrumOf;
        QSpinBox *lengthBox = nullptr;
        QTimer *playheadTimer = nullptr;
        // The folder and alias whose pitch pitchBox shows by default
        std::optional<std::pair<std::filesystem::path, QString>> pitchOf;
        // The audio files read for the waveform, by path, with the size and time they had
        struct ReadAudio {
            QString stamp;
            std::shared_ptr<const kit::WaveAudio> audio;
        };
        std::map<std::filesystem::path, ReadAudio> readAudio;

        // The folders the tree shows, to rebuild it only when they change
        QList<std::filesystem::path> shownDirectories;
        QList<std::filesystem::path> shownExcluded;

        static QString tr(const char *text) {
            return VoiceBankWindow::tr(text);
        }

        // The commands that set a value at the pointer, by the key 1 to 5 over the waveform
        static QList<std::pair<QString, OtoWaveformView::Value>> valueCommands() {
            return {
                {QStringLiteral("helloutau.voiceBank.setOffset"),       OtoWaveformView::Offset   },
                {QStringLiteral("helloutau.voiceBank.setOverlap"),      OtoWaveformView::Overlap  },
                {QStringLiteral("helloutau.voiceBank.setPreUtterance"),
                 OtoWaveformView::PreUtterance                                                    },
                {QStringLiteral("helloutau.voiceBank.setConsonant"),    OtoWaveformView::Consonant},
                {QStringLiteral("helloutau.voiceBank.setCutoff"),       OtoWaveformView::Cutoff   },
            };
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
            addCommand(QStringLiteral("helloutau.file.openRecentProject"), [this] {
                stdc_decl_t;
                editor->showRecent(Editor::RecentProjects, &decl);
            });
            addCommand(QStringLiteral("helloutau.file.openRecentVoiceBank"), [this] {
                stdc_decl_t;
                editor->showRecent(Editor::RecentVoiceBanks, &decl);
            });
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
                palette->setCommands(commandEntriesOf(editor->actionRegistry(), context));
                palette->setRecentIds(editor->settings().recentCommands());
                palette->popup();
            });
            addCommand(QStringLiteral("helloutau.voiceBank.reloadAll"), [this] {
                stdc_decl_t;
                decl.reloadAll();
            });
            addCommand(QStringLiteral("helloutau.voiceBank.insertEntry"), [this] {
                stdc_decl_t;
                decl.insertEntry();
            });
            addCommand(QStringLiteral("helloutau.voiceBank.duplicateEntries"), [this] {
                stdc_decl_t;
                decl.duplicateEntries();
            });
            addCommand(QStringLiteral("helloutau.voiceBank.duplicateWithRule"), [this] {
                stdc_decl_t;
                decl.duplicateWithRule();
            });
            addCommand(QStringLiteral("helloutau.voiceBank.renameAliases"), [this] {
                stdc_decl_t;
                decl.renameAliases();
            });
            addCommand(QStringLiteral("helloutau.voiceBank.includeAudio"), [this] {
                stdc_decl_t;
                decl.includeAudio();
            });
            addCommand(QStringLiteral("helloutau.edit.delete"), [this] {
                stdc_decl_t;
                decl.removeEntries();
            });
            addCommand(QStringLiteral("helloutau.voiceBank.playAudio"), [this] {
                if (preview->state() != SamplePreview::Stopped) {
                    preview->stop();
                    return;
                }
                playAudio(0, std::nullopt);
            });
            addCommand(QStringLiteral("helloutau.voiceBank.playSpan"), [this] {
                if (const auto entry = waveform->entry()) {
                    const double length = waveform->duration();
                    playAudio(OtoWaveformView::positionOf(*entry, OtoWaveformView::Offset, length),
                              OtoWaveformView::positionOf(*entry, OtoWaveformView::Cutoff, length));
                }
            });
            addCommand(QStringLiteral("helloutau.voiceBank.playFromPointer"), [this] {
                if (const auto time = waveform->pointerTime()) {
                    playAudio(*time, std::nullopt);
                }
            });
            addCommand(QStringLiteral("helloutau.voiceBank.showSpectrogram"), [this] {
                showSpectrum();
            })->setCheckable(true);
            addCommand(QStringLiteral("helloutau.voiceBank.showInfo"), [this] {
                info->setVisible(
                    actions.value(QStringLiteral("helloutau.voiceBank.showInfo"))->isChecked());
            })->setCheckable(true);
            addCommand(QStringLiteral("helloutau.voiceBank.convertCharset"),
                       [this] { askCharset(false); });
            addCommand(QStringLiteral("helloutau.voiceBank.rereadCharset"),
                       [this] { askCharset(true); });
            addCommand(QStringLiteral("helloutau.voiceBank.removeMetadata"), [this] {
                stdc_decl_t;
                decl.removeAudioMetadata();
            });
            addCommand(QStringLiteral("helloutau.voiceBank.synthesize"), [this] { synthesize(); });
            addCommand(QStringLiteral("helloutau.playback.stop"), [this] { preview->stop(); });
            for (const auto &[id, value] : valueCommands()) {
                addCommand(id, [this, value = value] {
                    if (const auto time = waveform->pointerTime()) {
                        waveform->setValueAt(value, *time);
                    }
                });
            }
            addCommand(QStringLiteral("helloutau.tools.settings"), [this] {
                stdc_decl_t;
                editor->showSettings(&decl);
            });
            ActionRegistrations::instance().addActions(&decl, context);

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
            table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            table->setMinimumWidth(0);
            table->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);

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
            right->setMinimumWidth(0);
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

            // The waveform of the current entry below, across the window
            waveform = new OtoWaveformView();
            // Keys of the waveform alone, which the table and the search box type otherwise
            QStringList waveformIds{QStringLiteral("helloutau.voiceBank.playFromPointer")};
            for (const auto &command : valueCommands()) {
                waveformIds.push_back(command.first);
            }
            for (const auto &id : std::as_const(waveformIds)) {
                const auto action = actions.value(id);
                action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
                waveform->addAction(action);
            }
            auto lower = new QWidget();
            lowerLayout = new QVBoxLayout(lower);
            lowerLayout->setContentsMargins(0, 0, 0, 0);
            lowerLayout->addLayout(previewControls());
            lowerLayout->addWidget(waveform, 1);

            auto vertical = new QSplitter(Qt::Vertical);
            vertical->addWidget(splitter);
            vertical->addWidget(lower);
            vertical->setStretchFactor(0, 1);
            vertical->setSizes({380, 260});

            // The information of the voice bank stays in the window and does not float.
            info = new VoiceBankInfoPanel(document->session());
            info->setRoot(document->rootPath());
            auto main = new QSplitter(Qt::Horizontal);
            main->addWidget(vertical);
            main->addWidget(info);
            main->setStretchFactor(0, 1);
            main->setStretchFactor(1, 0);
            // The panel keeps a readable width. It is hidden by View > Voice Bank Info rather
            // than by dragging the splitter, which would leave the command checked.
            main->setCollapsible(1, false);
            info->setMinimumWidth(180);
            info->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
            main->setSizes({760, 280});
            decl.setCentralWidget(main);

            const auto showInfo = actions.value(QStringLiteral("helloutau.voiceBank.showInfo"));
            showInfo->setChecked(true);
            QObject::connect(info, &VoiceBankInfoPanel::editRejected, &decl,
                             [this](const kit::DiagnosticList &diagnostics) {
                                 stdc_decl_t;
                                 // Once the box that lost the focus has settled
                                 QTimer::singleShot(0, &decl, [this, diagnostics] {
                                     stdc_decl_t;
                                     DiagnosticBox::show(&decl, tr("Voice Bank Info"), diagnostics);
                                 });
                             });

            tree->setContextMenuPolicy(Qt::CustomContextMenu);
            QObject::connect(
                tree, &QWidget::customContextMenuRequested, &decl, [this](const QPoint &position) {
                    stdc_decl_t;
                    const auto item = tree->itemAt(position);
                    if (!item || item->data(0, AllRole).toBool()) {
                        return;
                    }
                    // The folder under the pointer, even one not read
                    menuFolder = pathOf(item->data(0, PathRole).toString());
                    QMenu menu(&decl);
                    const auto convert =
                        actions.value(QStringLiteral("helloutau.voiceBank.convertCharset"));
                    menu.addAction(convert);
                    menu.addAction(
                        actions.value(QStringLiteral("helloutau.voiceBank.rereadCharset")));
                    const bool enabled = convert->isEnabled();
                    convert->setEnabled(directoryRef(*menuFolder).has_value());
                    menu.exec(tree->viewport()->mapToGlobal(position));
                    convert->setEnabled(enabled);
                    menuFolder.reset();
                });

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

        std::optional<kit::VoiceDirectoryRef>
            directoryRef(const std::filesystem::path &path) const {
            const auto list = kit::VoiceBankRef(document->session()).directories();
            for (int i = 0; i < list.size(); ++i) {
                if (list.at(i).path() == path) {
                    return list.at(i);
                }
            }
            return std::nullopt;
        }

        static QString stemOf(const QString &fileName) {
            return QString::fromStdU16String(
                std::filesystem::path(fileName.toStdU16String()).stem().u16string());
        }

        // The names under which the entries of fileName in directory are matched: the aliases,
        // an empty one as the stem of the file name, as the edit layer counts them
        static QSet<QString> namesOf(const kit::VoiceDirectoryRef &directory,
                                     const QString &fileName) {
            QSet<QString> names;
            const auto list = directory.otoEntries();
            for (int i = 0; i < list.size(); ++i) {
                if (list.at(i).fileName() == fileName) {
                    const auto alias = list.at(i).alias();
                    names.insert(alias.isEmpty() ? stemOf(fileName) : alias);
                }
            }
            return names;
        }

        // base, or else base followed by the first number from 2 that is not among names
        static QString freeName(const QString &base, const QSet<QString> &names) {
            if (!names.contains(base)) {
                return base;
            }
            for (int n = 2;; ++n) {
                const auto name = base + QString::number(n);
                if (!names.contains(name)) {
                    return name;
                }
            }
        }

        // The selected entries by directory, each group with all entries of its directory and
        // the indices of the selected entries among them
        std::vector<std::pair<std::filesystem::path, VoiceAliasRuleDialog::Group>>
            aliasGroups() const {
            std::map<std::filesystem::path, QList<int>> rowsByDirectory;
            for (const int row : selectedRows(true)) {
                rowsByDirectory[model->directoryOf(row)].push_back(row);
            }
            std::vector<std::pair<std::filesystem::path, VoiceAliasRuleDialog::Group>> groups;
            for (const auto &[path, rows] : rowsByDirectory) {
                const auto directory = directoryRef(path);
                if (!directory) {
                    continue;
                }
                VoiceAliasRuleDialog::Group group;
                const auto list = directory->otoEntries();
                for (int i = 0; i < list.size(); ++i) {
                    group.entries.push_back({list.at(i).fileName(), list.at(i).alias()});
                }
                for (const int row : rows) {
                    const auto entry = model->entryOf(row);
                    for (int i = 0; i < group.entries.size(); ++i) {
                        if (group.entries[i].fileName == entry.fileName &&
                            group.entries[i].alias == entry.alias) {
                            group.selected.push_back(i);
                            break;
                        }
                    }
                }
                groups.emplace_back(path, group);
            }
            return groups;
        }

        // Opens VoiceAliasRuleDialog in mode for the selected entries and applies the rule in one
        // transaction: a copy of each entry for Duplicate, a new alias for Rename.
        bool applyAliasRule(VoiceAliasRuleDialog::Mode mode) {
            stdc_decl_t;
            const auto groups = aliasGroups();
            if (groups.empty()) {
                return false;
            }
            QList<VoiceAliasRuleDialog::Group> dialogGroups;
            for (const auto &group : groups) {
                dialogGroups.push_back(group.second);
            }
            VoiceAliasRuleDialog dialog(mode, dialogGroups, &decl);
            if (dialog.exec() != QDialog::Accepted) {
                return false;
            }
            const auto changes = dialog.changes();
            const auto title = mode == VoiceAliasRuleDialog::Rename
                                   ? tr("Rename Aliases")
                                   : tr("Duplicate Entries with Rule");

            auto transaction = document->session()->transaction(title);
            kit::DiagnosticList diagnostics;
            // The resulting entries, selected afterward
            QList<RowKey> made;
            for (size_t g = 0; g < groups.size(); ++g) {
                const auto &path = groups[g].first;
                const auto directory = directoryRef(path);
                if (!directory) {
                    continue;
                }
                const auto list = directory->otoEntries();
                QList<kit::VoiceOtoEntry> copies;
                for (const auto &change : changes[qsizetype(g)]) {
                    if (!change.problem.isEmpty()) {
                        return false;
                    }
                    auto value = list.at(change.index).toVoiceOtoEntry();
                    if (mode == VoiceAliasRuleDialog::Rename) {
                        if (change.to == change.from) {
                            continue;
                        }
                        value.alias = change.to;
                        if (!kit::VoiceBankEdits::setEntry(list.at(change.index), value,
                                                           diagnostics)) {
                            DiagnosticBox::show(&decl, title, diagnostics);
                            return false;
                        }
                    } else {
                        value.alias = change.to;
                        copies.push_back(value);
                    }
                    made.push_back({path, value.fileName, value.alias});
                }
                if (!copies.isEmpty() &&
                    !kit::VoiceBankEdits::insertEntries(*directory, copies, diagnostics)) {
                    DiagnosticBox::show(&decl, title, diagnostics);
                    return false;
                }
            }
            const bool committed = transaction.commit(diagnostics);
            DiagnosticBox::show(&decl, title, diagnostics);
            if (!committed) {
                return false;
            }
            model->refresh();
            QList<int> selected;
            for (const auto &key : std::as_const(made)) {
                selected.push_back(model->rowOf(key.directory, key.fileName, key.alias));
            }
            selectRows(selected);
            return true;
        }

        QList<int> selectedRows() const {
            QList<int> rows;
            for (const auto &index : table->selectionModel()->selectedRows()) {
                rows.push_back(proxy->mapToSource(index).row());
            }
            std::sort(rows.begin(), rows.end());
            return rows;
        }

        // The selected rows of kind, either entries or else audio files without one
        QList<int> selectedRows(bool entries) const {
            QList<int> rows;
            for (const int row : selectedRows()) {
                const bool unlisted =
                    model->index(row, 0).data(VoiceBankEntryModel::RowKindRole).toInt() ==
                    VoiceBankEntryModel::UnlistedAudioRow;
                if (unlisted != entries) {
                    rows.push_back(row);
                }
            }
            return rows;
        }

        // Selects rows of the model, the first of them current, clearing the search if it hides
        // one.
        void selectRows(const QList<int> &rows) {
            const auto indexOf = [this](int row) {
                return proxy->mapFromSource(model->index(row, VoiceBankEntryModel::FileColumn));
            };
            for (const int row : rows) {
                if (row >= 0 && !indexOf(row).isValid()) {
                    search->clear();
                    break;
                }
            }
            const auto selection = table->selectionModel();
            selection->clearSelection();
            for (const int row : rows) {
                const auto index = indexOf(row);
                if (index.isValid()) {
                    selection->select(index,
                                      QItemSelectionModel::Select | QItemSelectionModel::Rows);
                }
            }
            if (!rows.isEmpty() && indexOf(rows.first()).isValid()) {
                selection->setCurrentIndex(indexOf(rows.first()), QItemSelectionModel::NoUpdate);
                table->scrollTo(indexOf(rows.first()));
            }
        }

        void updateEditActions() {
            const bool entries = !selectedRows(true).isEmpty();
            actions.value(QStringLiteral("helloutau.edit.delete"))->setEnabled(entries);
            actions.value(QStringLiteral("helloutau.voiceBank.duplicateEntries"))
                ->setEnabled(entries);
            actions.value(QStringLiteral("helloutau.voiceBank.duplicateWithRule"))
                ->setEnabled(entries);
            actions.value(QStringLiteral("helloutau.voiceBank.renameAliases"))->setEnabled(entries);
            actions.value(QStringLiteral("helloutau.voiceBank.includeAudio"))
                ->setEnabled(!selectedRows(false).isEmpty());
        }

        // The current row across a rebuild of the rows, by its folder, file and alias
        struct RowKey {
            std::filesystem::path directory;
            QString fileName;
            QString alias;
        };
        std::optional<RowKey> keptRow;

        void initTableEditing() {
            stdc_decl_t;
            QObject::connect(model, &QAbstractItemModel::modelAboutToBeReset, &decl, [this] {
                stdc_decl_t;
                const int row = decl.currentRow();
                keptRow.reset();
                if (row >= 0) {
                    const auto entry = model->entryOf(row);
                    keptRow = RowKey{model->directoryOf(row), entry.fileName, entry.alias};
                }
            });
            QObject::connect(model, &QAbstractItemModel::modelReset, &decl, [this] {
                if (keptRow) {
                    const int row =
                        model->rowOf(keptRow->directory, keptRow->fileName, keptRow->alias);
                    if (row >= 0) {
                        selectRows({row});
                    }
                }
                keptRow.reset();
                updateEditActions();
            });
            QObject::connect(table->selectionModel(), &QItemSelectionModel::selectionChanged, &decl,
                             [this] { updateEditActions(); });
            QObject::connect(model, &VoiceBankEntryModel::editRejected, &decl,
                             [this](const kit::DiagnosticList &diagnostics) {
                                 stdc_decl_t;
                                 // Once the editor of the cell has closed
                                 QTimer::singleShot(0, &decl, [this, diagnostics] {
                                     stdc_decl_t;
                                     DiagnosticBox::show(&decl, tr("Edit Entry"), diagnostics);
                                 });
                             });

            table->setContextMenuPolicy(Qt::CustomContextMenu);
            QObject::connect(
                table, &QWidget::customContextMenuRequested, &decl, [this](const QPoint &position) {
                    stdc_decl_t;
                    QMenu menu(&decl);
                    for (const auto id :
                         {"helloutau.voiceBank.insertEntry", "helloutau.voiceBank.duplicateEntries",
                          "helloutau.voiceBank.duplicateWithRule",
                          "helloutau.voiceBank.renameAliases", "helloutau.voiceBank.includeAudio",
                          "helloutau.edit.delete"}) {
                        menu.addAction(actions.value(QString::fromLatin1(id)));
                    }
                    menu.exec(table->viewport()->mapToGlobal(position));
                });
            updateEditActions();
        }

        // The encoding of the folder at path, in the tree or not read
        QString charsetOf(const std::filesystem::path &path) const {
            if (const auto ref = directoryRef(path)) {
                return ref->charset();
            }
            for (const auto &directory : document->session()->excludedDirectories()) {
                if (directory.path == path) {
                    return directory.charset;
                }
            }
            return {};
        }

        // Asks for an encoding of the folder of the context menu of the tree, or else the one
        // chosen there, and converts the folder to it or reads the folder again in it.
        void askCharset(bool reread) {
            stdc_decl_t;
            std::filesystem::path folder;
            if (menuFolder) {
                folder = *menuFolder;
            } else if (const auto item = tree->currentItem();
                       item && !item->data(0, AllRole).toBool()) {
                folder = pathOf(item->data(0, PathRole).toString());
            }
            const auto current = charsetOf(folder);
            auto candidates = kit::TextCodec::ustCandidates();
            if (!current.isEmpty() && !candidates.contains(current)) {
                candidates.prepend(current);
            }
            const auto label =
                reread ? tr("Read the files of %1 again in the encoding (the text changes, and "
                            "Undo brings it back):")
                       : tr("Save the files of %1 in the encoding (the text stays the same):");
            bool ok = false;
            const auto charset = QInputDialog::getItem(
                &decl, reread ? tr("Read Again in Encoding") : tr("Convert Encoding"),
                label.arg(folderName(folder)), candidates,
                std::max(0, int(candidates.indexOf(current))), false, &ok);
            if (!ok) {
                return;
            }
            if (reread) {
                decl.rereadCharset(folder, charset);
            } else {
                decl.convertCharset(folder, charset);
            }
        }

        // The encoding of each folder in its tooltip
        void updateTreeTips() {
            for (QTreeWidgetItemIterator it(tree); *it; ++it) {
                const auto item = *it;
                if (item->data(0, AllRole).toBool() || item->isDisabled()) {
                    continue;
                }
                const auto charset = charsetOf(pathOf(item->data(0, PathRole).toString()));
                item->setToolTip(0,
                                 charset.isEmpty() ? QString() : tr("Encoding: %1").arg(charset));
            }
        }

        // The audio file of row of the model
        std::filesystem::path audioPathOf(int row) const {
            return document->rootPath() / model->directoryOf(row) /
                   pathOf(model->entryOf(row).fileName);
        }

        // The audio at path, read again once its size or time changes, or null if it does not
        // read
        std::shared_ptr<const kit::WaveAudio> audioAt(const std::filesystem::path &path) {
            std::error_code error;
            const auto size = std::filesystem::file_size(path, error);
            const auto time = std::filesystem::last_write_time(path, error);
            if (error) {
                readAudio.erase(path);
                return nullptr;
            }
            const auto stamp =
                QStringLiteral("%1:%2").arg(size).arg(time.time_since_epoch().count());
            if (const auto found = readAudio.find(path);
                found != readAudio.end() && found->second.stamp == stamp) {
                return found->second.audio;
            }
            kit::DiagnosticList diagnostics;
            auto read = kit::WaveAudio::read(path, diagnostics);
            std::shared_ptr<const kit::WaveAudio> audio =
                read ? std::make_shared<const kit::WaveAudio>(std::move(*read)) : nullptr;
            // A few files, those last looked at
            if (readAudio.size() >= 16) {
                readAudio.clear();
            }
            readAudio[path] = {stamp, audio};
            return audio;
        }

        // Shows the audio and the values of the current row in the waveform.
        void showCurrentEntry() {
            stdc_decl_t;
            const int row = decl.currentRow();
            if (row < 0) {
                waveform->setAudio(nullptr);
                waveform->setEntry(std::nullopt);
                waveform->setSpectrogram(nullptr);
                waveform->setFrequencyTable(std::nullopt);
                frequencyShown = {};
                return;
            }
            const auto audio = audioAt(audioPathOf(row));
            if (audio != waveform->audio()) {
                waveform->setAudio(audio);
                showSpectrum();
            }
            showFrequency();
            const auto entry = model->entryOf(row);
            if (waveform->entry() != entry) {
                waveform->setEntry(entry);
            }
        }

        // The spectrogram of the audio shown, while the View menu asks for it
        void showSpectrum() {
            const auto audio = waveform->audio();
            const bool shown =
                actions.value(QStringLiteral("helloutau.voiceBank.showSpectrogram"))->isChecked();
            if (!shown || !audio) {
                waveform->setSpectrogram(nullptr);
                return;
            }
            if (spectrumOf != audio) {
                spectrum = std::make_shared<const kit::Spectrogram>(kit::Spectrogram::of(*audio));
                spectrumOf = audio;
            }
            waveform->setSpectrogram(spectrum);
        }

        // The formats registered, as they come and go. The format chosen stays while it is
        // registered; at first, and in place of a format that went, the one of the resampler of
        // the settings is chosen.
        void fillFrequencyBox() {
            const QSignalBlocker blocker(frequencyBox);
            const bool filled = frequencyBox->count() > 0;
            const auto chosen = frequencyBox->currentData().toString();
            frequencyBox->clear();
            frequencyBox->addItem(tr("None"), QString());
            const auto &formats = editor->frequencyFormats();
            for (const auto format : formats.formats()) {
                frequencyBox->addItem(format->name(), format->id());
            }
            int index = filled ? frequencyBox->findData(chosen) : -1;
            if (index < 0) {
                const auto format =
                    formats.formatForResampler(pathOf(editor->settings().resampler()));
                index = format ? std::max(0, frequencyBox->findData(format->id())) : 0;
            }
            frequencyBox->setCurrentIndex(index);
        }

        // The frequency table of the audio of the current row in the format of the box. The box
        // marks the formats without a table for the audio file.
        void showFrequency() {
            stdc_decl_t;
            const int row = decl.currentRow();
            const auto path = row >= 0 ? audioPathOf(row) : std::filesystem::path();
            const auto id = frequencyBox->currentData().toString();
            const auto &formats = editor->frequencyFormats();
            for (int i = 1; i < frequencyBox->count(); ++i) {
                const auto format = formats.format(frequencyBox->itemData(i).toString());
                const bool present = !path.empty() && format && format->exists(path);
                frequencyBox->setItemText(i, present ? format->name()
                                                     : tr("%1 (none)").arg(format->name()));
            }
            const auto audio = waveform->audio();
            if (frequencyShown == std::pair{path, id}) {
                return;
            }
            frequencyShown = {path, id};
            const auto format = formats.format(id);
            std::optional<kit::FrequencyTable> table;
            if (format && audio && format->exists(path)) {
                kit::DiagnosticList diagnostics;
                table = format->read(path, audio->sampleRate, diagnostics);
                if (!table) {
                    decl.statusBar()->showMessage(diagnostics.value(0).message);
                }
            }
            waveform->setFrequencyTable(table);
        }

        void showPointer() {
            stdc_decl_t;
            const auto time = waveform->pointerTime();
            if (!time || !waveform->audio()) {
                decl.statusBar()->clearMessage();
                return;
            }
            const auto value = waveform->activeValue();
            decl.statusBar()->showMessage(
                value ? tr("%1 at %2 ms")
                            .arg(OtoWaveformView::nameOf(*value))
                            .arg(OtoWaveformView::positionOf(*waveform->entry(), *value,
                                                             waveform->duration()))
                      : tr("%1 ms").arg(std::round(*time * 10) / 10));
        }

        void initWaveform() {
            stdc_decl_t;
            QObject::connect(table->selectionModel(), &QItemSelectionModel::currentRowChanged,
                             &decl, [this] {
                                 preview->stop();
                                 showCurrentEntry();
                                 updatePitch();
                             });
            QObject::connect(waveform, &OtoWaveformView::playRequested, &decl,
                             [this](double time) { playAudio(time, std::nullopt); });
            QObject::connect(model, &QAbstractItemModel::dataChanged, &decl,
                             [this] { showCurrentEntry(); });
            QObject::connect(model, &QAbstractItemModel::modelReset, &decl,
                             [this] { showCurrentEntry(); });
            QObject::connect(waveform, &OtoWaveformView::pointerMoved, &decl,
                             [this] { showPointer(); });
            QObject::connect(waveform, &OtoWaveformView::entryEdited, &decl,
                             [this](const kit::VoiceOtoEntry &entry) {
                                 stdc_decl_t;
                                 const int row = decl.currentRow();
                                 kit::DiagnosticList diagnostics;
                                 if (row < 0 || !model->setEntry(row, entry, diagnostics)) {
                                     DiagnosticBox::show(&decl, tr("Edit Entry"), diagnostics);
                                 }
                                 model->refresh();
                                 showCurrentEntry();
                             });
        }

        static QString noteName(int noteNum) {
            static const char *const names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                                "F#", "G",  "G#", "A",  "A#", "B"};
            return QString::fromLatin1(names[noteNum % 12]) + QString::number(noteNum / 12 - 1);
        }

        // The buttons of the preview above the waveform, with the pitch and the length of the
        // synthesized note
        QHBoxLayout *previewControls() {
            stdc_decl_t;
            preview = new SamplePreview(&decl);
            auto controls = new QHBoxLayout();
            for (const auto id : {"helloutau.voiceBank.playAudio", "helloutau.voiceBank.playSpan",
                                  "helloutau.voiceBank.synthesize", "helloutau.playback.stop"}) {
                auto button = new QToolButton();
                button->setDefaultAction(actions.value(QString::fromLatin1(id)));
                button->setToolButtonStyle(Qt::ToolButtonTextOnly);
                controls->addWidget(button);
            }
            controls->addSpacing(12);
            pitchBox = new QComboBox();
            for (int noteNum = kit::VoicePrefix::minimumKey;
                 noteNum <= kit::VoicePrefix::maximumKey; ++noteNum) {
                pitchBox->addItem(noteName(noteNum), noteNum);
            }
            pitchBox->setCurrentIndex(pitchBox->findData(60));
            auto pitchLabel = new QLabel(tr("&Pitch:"));
            pitchLabel->setBuddy(pitchBox);
            controls->addWidget(pitchLabel);
            controls->addWidget(pitchBox);
            lengthBox = new QSpinBox();
            lengthBox->setRange(15, 7680);
            lengthBox->setSingleStep(15);
            lengthBox->setValue(480);
            lengthBox->setSuffix(tr(" ticks"));
            auto lengthLabel = new QLabel(tr("&Length:"));
            lengthLabel->setBuddy(lengthBox);
            controls->addWidget(lengthLabel);
            controls->addWidget(lengthBox);
            controls->addSpacing(12);

            // The frequency table drawn over the audio, that of the resampler of the settings
            // at first (docs/FrequencyTables.md)
            frequencyBox = new QComboBox();
            frequencyBox->setObjectName(QStringLiteral("frequencyFormat"));
            fillFrequencyBox();
            QObject::connect(frequencyBox, &QComboBox::currentIndexChanged, &decl,
                             [this] { showFrequency(); });
            QObject::connect(&editor->frequencyFormats(),
                             &kit::FrequencyFormatRegistry::formatsChanged, &decl, [this] {
                                 fillFrequencyBox();
                                 frequencyShown = {};
                                 showFrequency();
                             });
            auto frequencyLabel = new QLabel(tr("&F0:"));
            frequencyLabel->setBuddy(frequencyBox);
            controls->addWidget(frequencyLabel);
            controls->addWidget(frequencyBox);
            controls->addStretch();

            playheadTimer = new QTimer(&decl);
            playheadTimer->setInterval(30);
            QObject::connect(playheadTimer, &QTimer::timeout, &decl,
                             [this] { waveform->setPlayhead(preview->position()); });
            QObject::connect(preview, &SamplePreview::stateChanged, &decl,
                             [this](SamplePreview::State state) {
                                 if (state == SamplePreview::Playing) {
                                     playheadTimer->start();
                                 } else {
                                     playheadTimer->stop();
                                     waveform->setPlayhead(std::nullopt);
                                 }
                                 actions.value(QStringLiteral("helloutau.playback.stop"))
                                     ->setEnabled(state != SamplePreview::Stopped);
                             });
            QObject::connect(preview, &SamplePreview::failed, &decl,
                             [this](const kit::DiagnosticList &diagnostics) {
                                 stdc_decl_t;
                                 DiagnosticBox::show(&decl, tr("Preview"), diagnostics);
                             });
            actions.value(QStringLiteral("helloutau.playback.stop"))->setEnabled(false);
            return controls;
        }

        void playAudio(double from, std::optional<double> to) {
            stdc_decl_t;
            if (!waveform->audio()) {
                return;
            }
            kit::DiagnosticList diagnostics;
            preview->play(waveform->audio(), from, to, diagnostics);
            DiagnosticBox::show(&decl, tr("Play"), diagnostics);
        }

        void synthesize() {
            stdc_decl_t;
            const int row = decl.currentRow();
            if (row < 0) {
                return;
            }
            const auto entry = model->entryOf(row);
            kit::VoiceSample sample;
            sample.path = audioPathOf(row);
            sample.fileName = entry.fileName;
            sample.alias = entry.alias;
            sample.offset = entry.offset;
            sample.consonant = entry.consonant;
            sample.cutoff = entry.cutoff;
            sample.preUtterance = entry.preUtterance;
            sample.voiceOverlap = entry.voiceOverlap;
            kit::DiagnosticList diagnostics;
            preview->synthesize(sample, pitchBox->currentData().toInt(), lengthBox->value(),
                                pathOf(editor->settings().resampler()), diagnostics);
            DiagnosticBox::show(&decl, tr("Synthesize"), diagnostics);
        }

        // Sets the pitch of the synthesized note to that of the folder of the current entry,
        // once another entry becomes current.
        void updatePitch() {
            stdc_decl_t;
            const int row = decl.currentRow();
            if (row < 0) {
                return;
            }
            const std::pair key{model->directoryOf(row), model->entryOf(row).alias};
            if (pitchOf == key) {
                return;
            }
            pitchOf = key;
            QMap<int, kit::VoicePrefix> prefixMap;
            const auto map = kit::VoiceBankRef(document->session()).prefixMap();
            if (map.isValid()) {
                for (const int noteNum : map.keys()) {
                    prefixMap.insert(noteNum, map.value(noteNum));
                }
            }
            pitchBox->setCurrentIndex(
                pitchBox->findData(SamplePreview::noteNumFor(prefixMap, key.first, key.second)));
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
            updateTreeTips();
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
            info->commit();
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
        impl.initTableEditing();
        impl.initWaveform();
        impl.refreshTree();
        impl.initScheduler();

        const auto session = impl.document->session();
        connect(session, &kit::VoiceBankSession::stepChanged, this, [this] {
            stdc_impl_t;
            impl.updateUndoActions();
            impl.refreshTree();
            impl.updateTreeTips();
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
            impl.info->setRoot(impl.document->rootPath());
        });
        impl.updateTitle();
        impl.updateUndoActions();
        editor->themeManager()->install(this, {QStringLiteral("VoiceBankWindow")});
        resize(960, 640);
    }

    VoiceBankWindow::~VoiceBankWindow() {
        stdc_impl_t;
        // The panel goes while the document is there: it writes what it holds when it loses the
        // focus.
        delete impl.info;
    }

    kit::VoiceBankDocument *VoiceBankWindow::document() const {
        stdc_impl_t;
        return impl.document.get();
    }

    QAK::WidgetActionContext *VoiceBankWindow::actionContext() const {
        stdc_impl_t;
        return impl.context;
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

    OtoWaveformView *VoiceBankWindow::waveformView() const {
        stdc_impl_t;
        return impl.waveform;
    }

    VoiceBankInfoPanel *VoiceBankWindow::infoPanel() const {
        stdc_impl_t;
        return impl.info;
    }

    bool VoiceBankWindow::convertCharset(const std::filesystem::path &directory,
                                         const QString &charset) {
        stdc_impl_t;
        const auto ref = impl.directoryRef(directory);
        kit::DiagnosticList diagnostics;
        const bool converted =
            ref && kit::VoiceBankEdits::convertCharset(*ref, charset, diagnostics);
        DiagnosticBox::show(this, tr("Convert Encoding"), diagnostics);
        return converted;
    }

    bool VoiceBankWindow::rereadCharset(const std::filesystem::path &directory,
                                        const QString &charset) {
        stdc_impl_t;
        kit::DiagnosticList diagnostics;
        const bool read = impl.document->session()->reread(directory, charset, diagnostics);
        DiagnosticBox::show(this, tr("Read Again in Encoding"), diagnostics);
        if (read) {
            impl.model->refresh();
        }
        return read;
    }

    QList<int> VoiceBankWindow::selectedRows() const {
        stdc_impl_t;
        return impl.selectedRows();
    }

    int VoiceBankWindow::currentRow() const {
        stdc_impl_t;
        const auto index = impl.table->currentIndex();
        return index.isValid() ? impl.proxy->mapToSource(index).row() : -1;
    }

    void VoiceBankWindow::setCurrentRow(int row) {
        stdc_impl_t;
        impl.selectRows({row});
    }

    bool VoiceBankWindow::insertEntry() {
        stdc_impl_t;
        const int row = currentRow();
        const auto path = row >= 0 ? impl.model->directoryOf(row)
                                   : impl.model->directory().value_or(std::filesystem::path());
        const auto directory = impl.directoryRef(path);
        if (!directory) {
            return false;
        }
        const auto files = impl.document->session()->audioFiles(path);
        const auto current = row >= 0 ? impl.model->entryOf(row).fileName : QString();
        bool ok = false;
        const auto fileName = QInputDialog::getItem(
            this, tr("Insert Entry"), tr("Audio file of the folder %1:").arg(impl.folderName(path)),
            files, std::max(0, int(files.indexOf(current))), true, &ok);
        if (!ok || fileName.trimmed().isEmpty()) {
            return false;
        }
        kit::VoiceOtoEntry entry;
        entry.fileName = fileName;
        const auto names = Impl::namesOf(*directory, fileName);
        const auto stem = Impl::stemOf(fileName);
        if (names.contains(stem)) {
            entry.alias = Impl::freeName(stem, names);
        }
        kit::DiagnosticList diagnostics;
        const bool inserted = kit::VoiceBankEdits::insertEntries(*directory, {entry}, diagnostics);
        DiagnosticBox::show(this, tr("Insert Entry"), diagnostics);
        if (!inserted) {
            return false;
        }
        impl.model->refresh();
        const int added = impl.model->rowOf(path, entry.fileName, entry.alias);
        impl.selectRows({added});
        const auto index =
            impl.proxy->mapFromSource(impl.model->index(added, VoiceBankEntryModel::AliasColumn));
        if (index.isValid()) {
            impl.table->setCurrentIndex(index);
            impl.table->edit(index);
        }
        return true;
    }

    bool VoiceBankWindow::duplicateEntries() {
        stdc_impl_t;
        const auto rows = impl.selectedRows(true);
        if (rows.isEmpty()) {
            return false;
        }
        const auto session = impl.document->session();
        auto transaction = session->transaction(tr("Duplicate Oto Entries"));
        kit::DiagnosticList diagnostics;
        // The copies by folder, and the names taken by each file meanwhile
        std::map<std::filesystem::path, QList<kit::VoiceOtoEntry>> copies;
        std::map<std::pair<std::filesystem::path, QString>, QSet<QString>> taken;
        QList<std::pair<std::filesystem::path, kit::VoiceOtoEntry>> made;
        for (const int row : rows) {
            const auto path = impl.model->directoryOf(row);
            const auto directory = impl.directoryRef(path);
            if (!directory) {
                continue;
            }
            auto entry = impl.model->entryOf(row);
            auto &names = taken[{path, entry.fileName}];
            if (names.isEmpty()) {
                names = Impl::namesOf(*directory, entry.fileName);
            }
            entry.alias = Impl::freeName(
                entry.alias.isEmpty() ? Impl::stemOf(entry.fileName) : entry.alias, names);
            names.insert(entry.alias);
            copies[path].push_back(entry);
            made.push_back({path, entry});
        }
        for (const auto &[path, entries] : copies) {
            if (!kit::VoiceBankEdits::insertEntries(*impl.directoryRef(path), entries,
                                                    diagnostics)) {
                DiagnosticBox::show(this, tr("Duplicate Entries"), diagnostics);
                return false;
            }
        }
        const bool committed = transaction.commit(diagnostics);
        DiagnosticBox::show(this, tr("Duplicate Entries"), diagnostics);
        if (!committed) {
            return false;
        }
        impl.model->refresh();
        QList<int> selected;
        for (const auto &[path, entry] : std::as_const(made)) {
            selected.push_back(impl.model->rowOf(path, entry.fileName, entry.alias));
        }
        impl.selectRows(selected);
        return true;
    }

    bool VoiceBankWindow::duplicateWithRule() {
        stdc_impl_t;
        return impl.applyAliasRule(VoiceAliasRuleDialog::Duplicate);
    }

    bool VoiceBankWindow::renameAliases() {
        stdc_impl_t;
        return impl.applyAliasRule(VoiceAliasRuleDialog::Rename);
    }

    bool VoiceBankWindow::includeAudio() {
        stdc_impl_t;
        const auto rows = impl.selectedRows(false);
        if (rows.isEmpty()) {
            return false;
        }
        std::map<std::filesystem::path, QStringList> files;
        for (const int row : rows) {
            files[impl.model->directoryOf(row)].push_back(impl.model->entryOf(row).fileName);
        }
        auto transaction = impl.document->session()->transaction(tr("Include Audio Files"));
        kit::DiagnosticList diagnostics;
        for (const auto &[path, names] : files) {
            const auto directory = impl.directoryRef(path);
            if (!directory || !kit::VoiceBankEdits::includeAudio(*directory, names, diagnostics)) {
                DiagnosticBox::show(this, tr("Include Audio Files"), diagnostics);
                return false;
            }
        }
        const bool committed = transaction.commit(diagnostics);
        DiagnosticBox::show(this, tr("Include Audio Files"), diagnostics);
        if (!committed) {
            return false;
        }
        impl.model->refresh();
        QList<int> selected;
        for (const auto &[path, names] : files) {
            for (const auto &name : names) {
                selected.push_back(impl.model->rowOf(path, name, {}));
            }
        }
        impl.selectRows(selected);
        return true;
    }

    bool VoiceBankWindow::removeEntries() {
        stdc_impl_t;
        const auto rows = impl.selectedRows(true);
        if (rows.isEmpty()) {
            return false;
        }
        std::map<std::filesystem::path, QList<int>> indices;
        for (const int row : rows) {
            indices[impl.model->directoryOf(row)].push_back(impl.model->entryIndexOf(row));
        }
        auto transaction = impl.document->session()->transaction(tr("Remove Oto Entries"));
        kit::DiagnosticList diagnostics;
        for (const auto &[path, list] : indices) {
            const auto directory = impl.directoryRef(path);
            if (!directory || !kit::VoiceBankEdits::removeEntries(*directory, list, diagnostics)) {
                DiagnosticBox::show(this, tr("Remove Entries"), diagnostics);
                return false;
            }
        }
        const bool committed = transaction.commit(diagnostics);
        DiagnosticBox::show(this, tr("Remove Entries"), diagnostics);
        if (committed) {
            impl.model->refresh();
        }
        return committed;
    }

    std::optional<int> VoiceBankWindow::removeAudioMetadata() {
        stdc_impl_t;
        // The audio files with metadata, of every folder in the tree
        QList<std::pair<std::filesystem::path, kit::WaveMetadata::Report>> found;
        QStringList lines;
        kit::DiagnosticList unreadable;
        {
            QApplication::setOverrideCursor(Qt::WaitCursor);
            const auto directories = kit::VoiceBankRef(impl.document->session()).directories();
            for (int i = 0; i < directories.size(); ++i) {
                const auto directory = directories.at(i).path();
                for (const auto &name : impl.document->session()->audioFiles(directory)) {
                    const auto path = impl.document->rootPath() / directory / pathOf(name);
                    const auto report = kit::WaveMetadata::find(path, unreadable);
                    if (!report || report->isEmpty()) {
                        continue;
                    }
                    QStringList parts;
                    for (const auto &chunk : report->chunks) {
                        parts.push_back(QString::fromLatin1(chunk).trimmed());
                    }
                    if (report->trailingBytes > 0) {
                        parts.push_back(tr("%n bytes after the last chunk", nullptr,
                                           int(report->trailingBytes)));
                    }
                    lines.push_back(QStringLiteral("%1: %2").arg(
                        QDir::toNativeSeparators(textOf(directory / pathOf(name))),
                        parts.join(QStringLiteral(", "))));
                    found.push_back({path, *report});
                }
            }
            QApplication::restoreOverrideCursor();
        }
        if (found.isEmpty()) {
            QMessageBox::information(this, tr("Remove Audio Metadata"),
                                     tr("No audio file of the voice bank carries metadata."));
            return std::nullopt;
        }
        QMessageBox box(QMessageBox::Question, tr("Remove Audio Metadata"),
                        tr("%n audio files carry chunks besides the format and the audio, such "
                           "as the tags that recording programs write.\n\nWrite them again "
                           "without these chunks? The audio stays the same. The files are "
                           "written at once, and Undo does not bring the chunks back.",
                           nullptr, int(found.size())),
                        QMessageBox::Yes | QMessageBox::No, this);
        box.setDefaultButton(QMessageBox::No);
        box.setDetailedText(lines.join(QLatin1Char('\n')));
        if (box.exec() != QMessageBox::Yes) {
            return std::nullopt;
        }
        kit::DiagnosticList diagnostics;
        int written = 0;
        for (const auto &[path, report] : std::as_const(found)) {
            written += kit::WaveMetadata::strip(path, diagnostics) ? 1 : 0;
        }
        DiagnosticBox::show(this, tr("Remove Audio Metadata"), diagnostics);
        statusBar()->showMessage(
            tr("The metadata of %n audio files was removed.", nullptr, written));
        impl.readAudio.clear();
        impl.waveform->setAudio(nullptr);
        impl.showCurrentEntry();
        return written;
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
        impl.info->commit();
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
