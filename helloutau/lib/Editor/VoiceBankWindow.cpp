#include "VoiceBankWindow.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <map>

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QEventLoop>
#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QSignalBlocker>
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QThread>
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
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTreeWidgetItemIterator>
#include <QtWidgets/QVBoxLayout>

#include <stdcorelib/path.h>
#include <stdcorelib/pimpl.h>

#include <QAKCore/actionregistry.h>
#include <QAKWidgets/widgetactioncontext.h>

#include <hellokit/AudioFile/AudioFileProcessor.h>
#include <hellokit/AudioFile/Spectrogram.h>
#include <hellokit/AudioFile/WaveAudio.h>
#include <hellokit/Edit/VoiceBankDocument.h>
#include <hellokit/Edit/VoiceBankEdits.h>
#include <hellokit/Edit/VoiceBankRefs.h>
#include <hellokit/Support/FileManager.h>
#include <hellokit/Support/TextCodec.h>
#include <hellokit/Synth/FrequencyMapGenerator.h>
#include <hellokit/Synth/SynthDiagnosers.h>
#include <hellokit/Synth/SynthToolProcess.h>
#include <hellokit/VoiceBank/FrequencyFormats.h>
#include <hellokit/VoiceBank/FrequencyMapConverter.h>
#include <hellokit/VoiceBank/VoiceAliasRule.h>
#include <hellokit/VoiceBank/VoiceBankCheckScheduler.h>

#include <helloutau/Theme/ThemeManager.h>
#include <helloutau/Widgets/CommandPalette.h>
#include <helloutau/Widgets/FindBar.h>
#include <helloutau/Widgets/ToolBarPalette.h>

#include "AboutDialog.h"
#include "VoiceAliasRuleDialog.h"
#include "AppSettings.h"
#include "SoftwareBuildChannel.h"
#include "CommandEntries_p.h"
#include "ConvertFrequencyMapsDialog.h"
#include "DiagnosticBox.h"
#include "EditFrequencyMapsDialog.h"
#include "Editor.h"
#include "FindSupport_p.h"
#include "GenerateFrequencyMapsDialog.h"
#include "OtoWaveformKeyboardModifiers.h"
#include "OtoWaveformView.h"
#include "OutputLogDialog.h"
#include "ProcessAudioFilesDialog.h"
#include "RemoveFrequencyMapsDialog.h"
#include "RepairFrequencyMapsDialog.h"
#include "SamplePreview.h"
#include "VoiceBankDecodingDialog.h"
#include "VoiceBankEntryModel.h"
#include "VoiceBankInfoPanel.h"

namespace hello::daw {

    namespace {

        QString textOf(const std::filesystem::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

        // Returns the names of the analysis caches of format, such as LLSM.
        QStringList cacheNamesOf(const kit::FrequencyFormat &format) {
            QStringList names;
            for (const auto &cache : format.analysisCaches()) {
                names.push_back(cache.name);
            }
            return names;
        }

        // Returns folder if nothing exists there, otherwise the first of folder (1), folder (2)
        // and so on at which nothing exists, so that a backup never writes into an existing
        // folder
        std::filesystem::path freeFolderOf(std::filesystem::path folder) {
            if (!folder.has_filename()) {
                folder = folder.parent_path();
            }
            std::error_code error;
            auto candidate = folder;
            for (int i = 1; std::filesystem::exists(candidate, error); ++i) {
                candidate = folder;
                candidate += QStringLiteral(" (%1)").arg(i).toStdU16String();
            }
            return candidate;
        }

        std::filesystem::path pathOf(const QString &text) {
            return std::filesystem::path(QDir::fromNativeSeparators(text).toStdU16String());
        }

        // The data of a folder item: its path relative to the root, which the item of all
        // folders lacks
        constexpr int PathRole = Qt::UserRole;
        constexpr int AllRole = Qt::UserRole + 1;

        // The scopes of the find bar: the aliases, which can be replaced, and the file names
        enum FindScope {
            AliasScope,
            FileScope,
        };

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

        // Draws the matches of the find bar over the text of a cell after the style has drawn
        // the cell.
        class MatchDelegate : public QStyledItemDelegate {
        public:
            using QStyledItemDelegate::QStyledItemDelegate;

            // Returns the matches in the text of a cell
            std::function<QList<kit::TextSearch::Match>(const QModelIndex &)> matchesOf;

            void paint(QPainter *painter, const QStyleOptionViewItem &option,
                       const QModelIndex &index) const override {
                QStyledItemDelegate::paint(painter, option, index);
                const auto matches = matchesOf(index);
                if (matches.isEmpty()) {
                    return;
                }
                auto item = option;
                initStyleOption(&item, index);
                FindMatches::drawItemMatches(painter, item, FindSupport::rangesOf(matches),
                                             FindMatches::matchColor());
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
        FindBar *findBar = nullptr;
        // The search whose matches the table highlights, invalid while the find bar is hidden
        kit::TextSearch highlightedSearch;
        QMenu *recentMenu = nullptr;

        // Follows the disk while the window is open. See checkDisk().
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
        QPushButton *determineNames = nullptr;

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
        QToolBar *sampleToolBar = nullptr;
        // The boxes of the sample tool bar, which QActionKit creates again as it rebuilds the
        // tool bar, and the values that they show
        QPointer<QComboBox> pitchBox;
        QPointer<QSpinBox> lengthBox;
        QPointer<QComboBox> frequencyBox;
        int synthesizedPitch = 60;
        int synthesizedLength = 480;
        // The id of the format of the frequency map chosen in its box, empty for none, or
        // std::nullopt until the user chooses one
        std::optional<QString> frequencyFormat;
        // The audio file and the format of the frequency map shown, and the spectrogram of
        // the audio it was computed from
        std::pair<std::filesystem::path, QString> frequencyShown;
        std::shared_ptr<const kit::Spectrogram> spectrum;
        std::shared_ptr<const kit::WaveAudio> spectrumOf;
        // The output of the resampler during the generations of frequency maps, which the
        // process of the generator writes
        std::shared_ptr<kit::SynthToolOutputLog> generationLog =
            std::make_shared<kit::SynthToolOutputLog>();
        kit::FrequencyMapGenerator generator;
        QTimer *playheadTimer = nullptr;
        // The folder and alias whose pitch the synthesized note has by default
        std::optional<std::pair<std::filesystem::path, QString>> pitchOf;
        // The audio files read for the waveform, by path, with the size and time they had
        struct ReadAudio {
            QString stamp;
            std::shared_ptr<const kit::WaveAudio> audio;
        };
        std::map<std::filesystem::path, ReadAudio> readAudio;

        // The folders the tree shows and their logical paths, to rebuild it only when they change
        QList<std::filesystem::path> shownDirectories;
        QList<std::filesystem::path> shownExcluded;
        QList<std::filesystem::path> shownLogical;

        static QString tr(const char *text) {
            return VoiceBankWindow::tr(text);
        }

        static QString tr(const char *text, const char *disambiguation, int n) {
            return VoiceBankWindow::tr(text, disambiguation, n);
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
            initSampleToolBar();

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
            addCommand(QStringLiteral("helloutau.file.clearRecent"), [this] {
                stdc_decl_t;
                editor->clearRecent(&decl);
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
            addCommand(QStringLiteral("helloutau.edit.find"), [this] {
                findBar->showFind();
                updateFindResult();
            });
            addCommand(QStringLiteral("helloutau.edit.replace"), [this] {
                findBar->showReplace();
                updateFindResult();
            });
            addCommand(QStringLiteral("helloutau.edit.findNext"), [this] { findAgain(true); });
            addCommand(QStringLiteral("helloutau.edit.findPrevious"), [this] { findAgain(false); });
            addCommand(QStringLiteral("helloutau.view.commandPalette"), [this] {
                palette->setCommands(
                    commandEntriesOf(editor->actionRegistry(Editor::VoiceBankWindowKind), context));
                palette->setRecentIds(editor->settings().commandPalette().recentCommands());
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
            addCommand(QStringLiteral("helloutau.voiceBank.openInFileManager"),
                       [this] { openInFileManager(); });
            addCommand(QStringLiteral("helloutau.select.selectAll"),
                       [this] { table->selectAll(); });
            // The current row is cleared as well, so that no entry is shown in the waveform
            // without a selection.
            addCommand(QStringLiteral("helloutau.select.deselect"), [this] {
                table->selectionModel()->clearCurrentIndex();
                table->clearSelection();
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
                       [this] { promptForCharset(false); });
            addCommand(QStringLiteral("helloutau.voiceBank.rereadCharset"),
                       [this] { promptForCharset(true); });
            addCommand(QStringLiteral("helloutau.voiceBank.rereadCharsets"), [this] {
                stdc_decl_t;
                decl.rereadCharsets();
            });
            addCommand(QStringLiteral("helloutau.voiceBank.convertEncodings"), [this] {
                stdc_decl_t;
                decl.convertEncodings();
            });
            addCommand(QStringLiteral("helloutau.voiceBank.editMaps"), [this] {
                stdc_decl_t;
                decl.editFrequencyMaps();
            });
            addCommand(QStringLiteral("helloutau.voiceBank.generateMaps"), [this] {
                stdc_decl_t;
                decl.generateFrequencyMaps();
            });
            addCommand(QStringLiteral("helloutau.voiceBank.removeMaps"), [this] {
                stdc_decl_t;
                decl.removeFrequencyMaps();
            });
            addCommand(QStringLiteral("helloutau.voiceBank.repairMaps"), [this] {
                stdc_decl_t;
                decl.repairFrequencyMaps();
            });
            addCommand(QStringLiteral("helloutau.voiceBank.convertMaps"), [this] {
                stdc_decl_t;
                decl.convertFrequencyMaps();
            });
            addCommand(QStringLiteral("helloutau.voiceBank.viewFrequencyMapLog"), [this] {
                stdc_decl_t;
                OutputLogDialog dialog(generationLog, tr("Frequency Map Log"), &decl);
                dialog.exec();
            });
            addCommand(QStringLiteral("helloutau.voiceBank.processAudio"), [this] {
                stdc_decl_t;
                decl.processAudioFiles();
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
            addCommand(QStringLiteral("helloutau.help.checkForUpdates"), [this] {
                stdc_decl_t;
                editor->checkForUpdates(&decl);
            });
            addCommand(QStringLiteral("helloutau.help.about"), [this] {
                stdc_decl_t;
                AboutDialog dialog(&decl);
                dialog.exec();
            });
            addCommand(QStringLiteral("helloutau.help.aboutQt"), [this] {
                stdc_decl_t;
                QMessageBox::aboutQt(&decl);
            });
            editor->addContributedActions(&decl, context);

            const auto registry = editor->actionRegistry(Editor::VoiceBankWindowKind);
            registry->addContext(context);
            for (const auto element :
                 {QAK::AE_Layouts, QAK::AE_Texts, QAK::AE_Keymap, QAK::AE_Icons}) {
                registry->updateContext(element);
            }

            palette = new CommandPalette(&decl);
            QObject::connect(palette, &CommandPalette::commandActivated, &decl,
                             [this](const QString &id) {
                                 stdc_decl_t;
                                 editor->settings().commandPalette().addRecentCommand(id);
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
            model->setFrequencyFormat(editor->frequencyFormats().format(currentFrequencyFormat()));
            proxy = new EntryFilter(&decl);
            proxy->setSourceModel(model);
            proxy->setSortRole(VoiceBankEntryModel::SortRole);
            proxy->setSortCaseSensitivity(Qt::CaseInsensitive);

            table = new QTableView();
            table->setModel(proxy);
            // The width of an inserted cache column is fitted to its name.
            QObject::connect(proxy, &QAbstractItemModel::columnsInserted, table,
                             [this](const QModelIndex &, int first, int last) {
                                 for (int column = first; column <= last; ++column) {
                                     table->resizeColumnToContents(column);
                                 }
                             });
            table->setSortingEnabled(true);
            table->sortByColumn(-1, Qt::AscendingOrder);
            table->setSelectionBehavior(QAbstractItemView::SelectRows);
            // A row has one line, so that a long name is elided and the matches of the find bar
            // are highlighted up to the ellipsis.
            table->setWordWrap(false);
            table->verticalHeader()->hide();
            table->horizontalHeader()->setStretchLastSection(false);
            table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            table->setMinimumWidth(0);
            table->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
            // Once the window is shown, so that the rows in view are measured
            QTimer::singleShot(0, table, [this] { fitColumns(); });

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
            determineNames = new QPushButton(tr("&Determine File Names..."));
            auto barLayout = new QHBoxLayout(bar);
            barLayout->addWidget(barText, 1);
            barLayout->addWidget(readChanged);
            barLayout->addWidget(readAdded);
            barLayout->addWidget(determineNames);
            bar->hide();
            QObject::connect(determineNames, &QPushButton::clicked, &decl, [this] {
                stdc_decl_t;
                decl.determineFileNames();
            });
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
            const auto updateModifiers = [waveform = waveform, editor = editor] {
                waveform->setKeyboardModifierBindings(editor->keyboardModifierBindings(
                    Editor::VoiceBankWindowKind, OtoWaveformKeyboardModifiers::scheme()));
            };
            updateModifiers();
            QObject::connect(editor, &Editor::keyboardModifierBindingsChanged, waveform,
                             [updateModifiers](Editor::WindowKind kind) {
                                 if (kind == Editor::VoiceBankWindowKind) {
                                     updateModifiers();
                                 }
                             });
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
            initPreview();
            lowerLayout->addWidget(sampleToolBar);
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
                                     DiagnosticBox::report(&decl, tr("Voice Bank Info"),
                                                           diagnostics);
                                 });
                             });

            tree->setContextMenuPolicy(Qt::CustomContextMenu);
            QObject::connect(
                tree, &QWidget::customContextMenuRequested, &decl, [this](const QPoint &position) {
                    stdc_decl_t;
                    const auto item = tree->itemAt(position);
                    if (!item) {
                        return;
                    }
                    if (item->data(0, AllRole).toBool()) {
                        QMenu menu(&decl);
                        menu.addAction(
                            actions.value(QStringLiteral("helloutau.voiceBank.rereadCharsets")));
                        menu.exec(tree->viewport()->mapToGlobal(position));
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

        // The name of folder directory of the voice bank in a message, its logical path
        QString folderName(const std::filesystem::path &directory) const {
            return directory.empty()
                       ? QString::fromStdU16String(document->rootPath().filename().u16string())
                       : QDir::toNativeSeparators(QString::fromStdU16String(
                             document->session()->logicalPathOf(directory).u16string()));
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
            VoiceBankDecodingDialog selector(&decl);
            selector.setRoot(document->rootPath());
            kit::DiagnosticList diagnostics;
            document->reloadFromDisk(changes, &selector, diagnostics);
            DiagnosticBox::report(&decl, tr("Read from Disk"), diagnostics);
            for (const auto &directory : changes.changed) {
                declined.erase(directory);
            }
            for (const auto &directory : changes.removed) {
                declined.erase(directory);
            }
            check({});
        }

        // Handles the difference between the disk and the voice bank, in places or everywhere.
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
            }
            // Not an edit of the tree, which the model follows by itself. The maps may have been
            // written or removed by another program, such as a resampler that renders.
            model->refreshFrequencyMaps();

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
                       "Read them again? Unsaved changes in them are replaced, and Undo restores "
                       "them.")
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
            stdc_decl_t;
            const int names = decl.fileNamesToDetermine();
            if (names > 0) {
                lines.push_back(
                    VoiceBankWindow::tr("%n file name(s) are to be determined.", nullptr, names));
            }
            barText->setText(lines.join(QLatin1Char('\n')));
            readChanged->setVisible(!changed.isEmpty());
            readAdded->setVisible(!pending.added.isEmpty());
            determineNames->setVisible(names > 0);
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
                            DiagnosticBox::report(&decl, title, diagnostics);
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
                    DiagnosticBox::report(&decl, title, diagnostics);
                    return false;
                }
            }
            const bool committed = transaction.commit(diagnostics);
            DiagnosticBox::report(&decl, title, diagnostics);
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

        void initFind() {
            stdc_decl_t;
            findBar = new FindBar(&decl);
            findBar->setScopes({tr("Aliases"), tr("File Names")});
            findBar->setAnchor(table);
            QObject::connect(findBar, &FindBar::queryChanged, &decl, [this] {
                findBar->setReplaceEnabled(findBar->scope() == AliasScope);
                findEntry(true, true);
            });
            QObject::connect(findBar, &FindBar::findNextRequested, &decl,
                             [this] { findEntry(true, false); });
            QObject::connect(findBar, &FindBar::findPreviousRequested, &decl,
                             [this] { findEntry(false, false); });
            QObject::connect(findBar, &FindBar::replaceRequested, &decl,
                             [this] { replaceAlias(); });
            QObject::connect(findBar, &FindBar::replaceAllRequested, &decl,
                             [this] { replaceAllAliases(); });
            // The count is updated when the rows of the table or the current row change.
            QObject::connect(proxy, &QAbstractItemModel::layoutChanged, &decl,
                             [this] { updateFindResult(); });
            QObject::connect(proxy, &QAbstractItemModel::modelReset, &decl,
                             [this] { updateFindResult(); });
            QObject::connect(proxy, &QAbstractItemModel::rowsInserted, &decl,
                             [this] { updateFindResult(); });
            QObject::connect(proxy, &QAbstractItemModel::rowsRemoved, &decl,
                             [this] { updateFindResult(); });
            QObject::connect(proxy, &QAbstractItemModel::dataChanged, &decl,
                             [this] { updateFindResult(); });
            QObject::connect(table->selectionModel(), &QItemSelectionModel::currentRowChanged,
                             &decl, [this] { updateFindResult(); });

            const auto delegate = new MatchDelegate(table);
            delegate->matchesOf = [this](const QModelIndex &index) { return cellMatches(index); };
            table->setItemDelegate(delegate);
            QObject::connect(findBar, &FindBar::closed, &decl,
                             [this] { highlight(kit::TextSearch()); });
        }

        void highlight(const kit::TextSearch &search) {
            highlightedSearch = search;
            table->viewport()->update();
        }

        // Returns the matches of the highlighted search in the text of the cell at index of the
        // table. An empty alias denotes the stem of the file name, whose matches are therefore
        // drawn in the file name.
        QList<kit::TextSearch::Match> cellMatches(const QModelIndex &index) const {
            if (!highlightedSearch.isValid()) {
                return {};
            }
            const int row = proxy->mapToSource(index).row();
            const auto entry = model->entryOf(row);
            const int column = index.column();
            if (findBar->scope() == FileScope) {
                return column == VoiceBankEntryModel::FileColumn
                           ? highlightedSearch.matchesIn(entry.fileName)
                           : QList<kit::TextSearch::Match>();
            }
            const auto name = findTextOf(row);
            if (!name) {
                return {};
            }
            if (column == VoiceBankEntryModel::AliasColumn && !entry.alias.isEmpty()) {
                return highlightedSearch.matchesIn(entry.alias);
            }
            if (column == VoiceBankEntryModel::FileColumn && entry.alias.isEmpty() &&
                entry.fileName.startsWith(*name)) {
                return highlightedSearch.matchesIn(*name);
            }
            return {};
        }

        // Returns the text of row of the model that the find bar searches: the file name, or the
        // name that the entry denotes, the stem of the file name for an empty alias. An audio
        // file without an entry has no alias, which is returned as std::nullopt.
        std::optional<QString> findTextOf(int row) const {
            const auto entry = model->entryOf(row);
            if (findBar->scope() == FileScope) {
                return entry.fileName;
            }
            if (model->index(row, 0).data(VoiceBankEntryModel::RowKindRole).toInt() ==
                VoiceBankEntryModel::UnlistedAudioRow) {
                return std::nullopt;
            }
            return kit::VoiceAliasRule::nameOf(entry.fileName, entry.alias);
        }

        // Returns the row of the model that row of the table shows.
        int sourceRowOf(int shown) const {
            return proxy->mapToSource(proxy->index(shown, 0)).row();
        }

        // Returns the rows of the table, in the order shown, whose text search matches. The rows
        // that the folder and the search box hide are not searched.
        QList<int> entryMatches(const kit::TextSearch &search) const {
            QList<int> matches;
            if (!search.isValid()) {
                return matches;
            }
            for (int shown = 0; shown < proxy->rowCount(); ++shown) {
                const auto text = findTextOf(sourceRowOf(shown));
                if (text && search.matches(*text)) {
                    matches.push_back(shown);
                }
            }
            return matches;
        }

        // Returns the current row of the table, or -1 if there is none.
        int currentShownRow() const {
            const auto index = table->currentIndex();
            return index.isValid() ? index.row() : -1;
        }

        void updateFindResult() {
            if (!findBar || findBar->isHidden()) {
                return;
            }
            const auto search = FindSupport::searchOf(findBar);
            highlight(search);
            FindSupport::showResult(findBar, search, entryMatches(search), currentShownRow());
        }

        // Selects the next or the previous row whose text matches, from the current row. With
        // inclusive, as for typing in the find field, the current row is itself a candidate.
        void findEntry(bool forward, bool inclusive) {
            const auto search = FindSupport::searchOf(findBar);
            const auto matches = entryMatches(search);
            if (const auto at =
                    FindMatches::adjacentMatch(matches, currentShownRow(), forward, inclusive)) {
                selectRows({sourceRowOf(matches[*at])});
            }
            updateFindResult();
        }

        // Selects the next or the previous match for F3 and Shift+F3, or shows the find bar if it
        // has no query.
        void findAgain(bool forward) {
            if (findBar->isHidden() || findBar->text().isEmpty()) {
                findBar->showFind();
                updateFindResult();
                return;
            }
            findEntry(forward, false);
        }

        // Returns the planned aliases of the entries at rows of the model, which the search
        // matches, by folder. Each change is checked against the entries of its folder, as the
        // renaming of aliases checks them. Unchanged aliases are left out.
        std::map<std::filesystem::path, QList<kit::VoiceAliasRule::Change>>
            aliasChanges(const QList<int> &rows, const kit::TextSearch &search) const {
            std::map<std::filesystem::path, QList<kit::VoiceAliasRule::Change>> changes;
            for (const int row : rows) {
                const int index = model->entryIndexOf(row);
                if (index < 0) {
                    continue;
                }
                const auto entry = model->entryOf(row);
                const auto name = kit::VoiceAliasRule::nameOf(entry.fileName, entry.alias);
                kit::VoiceAliasRule::Change change;
                change.index = index;
                change.to = search.replaced(name, findBar->replacement());
                if (change.to != name) {
                    changes[model->directoryOf(row)].push_back(change);
                }
            }
            for (auto &[path, list] : changes) {
                QList<kit::VoiceAliasRule::Entry> entries;
                if (const auto directory = directoryRef(path)) {
                    const auto oto = directory->otoEntries();
                    for (int i = 0; i < oto.size(); ++i) {
                        entries.push_back({oto.at(i).fileName(), oto.at(i).alias()});
                    }
                }
                list = kit::VoiceAliasRule::check(entries, list, false);
            }
            return changes;
        }

        // Writes the planned aliases as one undo step named title and selects the entries.
        // If a change has a problem, the problems are reported and nothing is written. Returns
        // whether the step is committed.
        bool applyAliasChanges(
            const std::map<std::filesystem::path, QList<kit::VoiceAliasRule::Change>> &changes,
            const QString &title) {
            stdc_decl_t;
            kit::DiagnosticList problems;
            for (const auto &[path, list] : changes) {
                for (const auto &change : list) {
                    if (!change.problem.isEmpty()) {
                        kit::Diagnostic diagnostic;
                        diagnostic.severity = kit::Diagnostic::Error;
                        diagnostic.message =
                            tr("\"%1\" to \"%2\": %3").arg(change.from, change.to, change.problem);
                        problems.push_back(diagnostic);
                    }
                }
            }
            if (!problems.isEmpty()) {
                DiagnosticBox::report(&decl, title, problems);
                return false;
            }
            auto transaction = document->session()->transaction(title);
            kit::DiagnosticList diagnostics;
            QList<RowKey> made;
            for (const auto &[path, list] : changes) {
                const auto directory = directoryRef(path);
                if (!directory) {
                    continue;
                }
                const auto entries = directory->otoEntries();
                for (const auto &change : list) {
                    auto value = entries.at(change.index).toVoiceOtoEntry();
                    value.alias = change.to;
                    if (!kit::VoiceBankEdits::setEntry(entries.at(change.index), value,
                                                       diagnostics)) {
                        DiagnosticBox::report(&decl, title, diagnostics);
                        return false;
                    }
                    made.push_back({path, value.fileName, value.alias});
                }
            }
            const bool committed = transaction.commit(diagnostics);
            DiagnosticBox::report(&decl, title, diagnostics);
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

        // Replaces the matches in the alias of the current entry if it matches, and selects the
        // next match, as Replace of VS Code does. Without a matching entry current, only the next
        // match is selected.
        void replaceAlias() {
            if (findBar->scope() != AliasScope) {
                return;
            }
            const auto search = FindSupport::searchOf(findBar);
            const int shown = currentShownRow();
            if (search.isValid() && shown >= 0) {
                const int row = sourceRowOf(shown);
                const auto text = findTextOf(row);
                if (text && search.matches(*text)) {
                    const auto changes = aliasChanges({row}, search);
                    if (!changes.empty() && !applyAliasChanges(changes, tr("Replace"))) {
                        return;
                    }
                }
            }
            findEntry(true, false);
        }

        // Replaces the matches in the aliases of every row that the table shows as one undo
        // step, and selects the changed entries.
        void replaceAllAliases() {
            stdc_decl_t;
            if (findBar->scope() != AliasScope) {
                return;
            }
            const auto search = FindSupport::searchOf(findBar);
            QList<int> rows;
            for (const int shown : entryMatches(search)) {
                rows.push_back(sourceRowOf(shown));
            }
            const auto changes = aliasChanges(rows, search);
            int count = 0;
            for (const auto &[path, list] : changes) {
                count += int(list.size());
            }
            if (count == 0 || !applyAliasChanges(changes, tr("Replace All"))) {
                return;
            }
            decl.statusBar()->showMessage(
                VoiceBankWindow::tr("%n alias(es) replaced.", nullptr, count));
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
            stdc_decl_t;
            actions.value(QStringLiteral("helloutau.voiceBank.openInFileManager"))
                ->setEnabled(decl.currentRow() >= 0);
        }

        // Selects the audio file of the current entry in the file manager of the system.
        void openInFileManager() {
            stdc_decl_t;
            const int row = decl.currentRow();
            if (row < 0) {
                return;
            }
            const auto path = audioPathOf(row);
            if (!kit::FileManager::reveal(path)) {
                // The title is the name of the command, which names the file manager of the
                // platform.
                const auto info =
                    editor->actionRegistry(Editor::VoiceBankWindowKind)
                        ->actionInfo(QStringLiteral("helloutau.voiceBank.openInFileManager"));
                QMessageBox::warning(&decl, info ? info->text().withoutMnemonic() : QString(),
                                     tr("%1 cannot be shown.")
                                         .arg(QDir::toNativeSeparators(
                                             QString::fromStdU16String(path.u16string()))));
            }
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
                                     DiagnosticBox::report(&decl, tr("Edit Entry"), diagnostics);
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
                    menu.addSeparator();
                    menu.addAction(
                        actions.value(QStringLiteral("helloutau.voiceBank.openInFileManager")));
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

        // Shows a list in which the user chooses an encoding for the folder of the context menu of
        // the tree, or else for the folder chosen in the tree, and converts the folder to the
        // encoding or reads the folder again in it.
        void promptForCharset(bool reread) {
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
            return document->session()->audioPathOf(model->directoryOf(row),
                                                    model->entryOf(row).fileName);
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
                waveform->setFrequencyMap(std::nullopt);
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

        // The spectrogram of the audio shown, while the command of the View menu is checked
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

        // Returns the id of the format of the frequency map in effect, or an empty string for
        // none. The format chosen stays while it is registered. At first, and in place of a
        // format that is no longer registered, the format of the resampler of the settings is
        // in effect.
        QString currentFrequencyFormat() const {
            const auto &formats = editor->frequencyFormats();
            if (frequencyFormat &&
                (frequencyFormat->isEmpty() || formats.format(*frequencyFormat))) {
                return *frequencyFormat;
            }
            const auto format =
                formats.formatForResampler(pathOf(editor->settings().rendering().resampler()));
            return format ? format->id() : QString();
        }

        // Fills the box of the format with the registered formats, as formats are registered and
        // removed.
        void fillFrequencyBox() {
            if (!frequencyBox) {
                return;
            }
            const QSignalBlocker blocker(frequencyBox.data());
            frequencyBox->clear();
            frequencyBox->addItem(tr("None"), QString());
            for (const auto format : editor->frequencyFormats().formats()) {
                frequencyBox->addItem(format->name(), format->id());
            }
            frequencyBox->setCurrentIndex(
                std::max(0, frequencyBox->findData(currentFrequencyFormat())));
        }

        // The audio files of the voice bank, those of the folders not read included, by folder
        std::map<std::filesystem::path, QList<std::filesystem::path>> audioByFolder() const {
            std::map<std::filesystem::path, QList<std::filesystem::path>> audio;
            const auto root = document->rootPath();
            for (const auto &listing : document->session()->nameListings()) {
                auto &names = audio[root / listing.directory];
                for (const auto &name : listing.audioFiles) {
                    names.push_back(name.toStdU16String());
                }
            }
            return audio;
        }

        // Returns the names of the audio files of the selected rows by folder, each name once
        std::map<std::filesystem::path, QList<std::filesystem::path>>
            selectedAudioByFolder() const {
            std::map<std::filesystem::path, QList<std::filesystem::path>> audio;
            for (const int row : selectedRows()) {
                const auto path = audioPathOf(row);
                auto &names = audio[path.parent_path()];
                if (!names.contains(path.filename())) {
                    names.push_back(path.filename());
                }
            }
            return audio;
        }

        // Returns the paths of the audio files of the selected rows, in normal form, as are the
        // paths that a dialog lists. The paths are compared whole, because the folder of the root
        // in audioByFolder() ends with a separator and the folder of a selected file does not.
        QList<std::filesystem::path> selectedAudioPaths() const {
            QList<std::filesystem::path> paths;
            for (const auto &[folder, names] : selectedAudioByFolder()) {
                for (const auto &name : names) {
                    paths.push_back((folder / name).lexically_normal());
                }
            }
            return paths;
        }

        // Returns the audio files of the voice bank, those of the folders not read included,
        // each with whether it has a map of format, whether it has each analysis cache of
        // format, and whether it is selected, as GenerateFrequencyMapsDialog::File and
        // RemoveFrequencyMapsDialog::File hold them
        template <class File>
        QList<File> mapListingOf(const kit::FrequencyFormat &format) const {
            const auto selected = selectedAudioPaths();
            const auto caches = format.analysisCaches();
            QList<File> files;
            for (const auto &[folder, names] : audioByFolder()) {
                const auto present = format.mapsExist(folder, names);
                for (qsizetype i = 0; i < names.size(); ++i) {
                    File file;
                    file.path = (folder / names.at(i)).lexically_normal();
                    file.hasMap = present.value(i);
                    for (const auto &cache : caches) {
                        std::error_code error;
                        file.caches.push_back(std::filesystem::is_regular_file(
                            folder / cache.fileNameOf(names.at(i)), error));
                    }
                    file.selected = selected.contains(file.path);
                    file.shownPath = shownPathOf(file.path);
                    files.push_back(std::move(file));
                }
            }
            return files;
        }

        // Returns the path of the file at path for the lists of the dialogs, relative to the root
        // and with misdecoded names restored as the window shows them, or an empty path for a
        // file outside the root.
        std::filesystem::path shownPathOf(const std::filesystem::path &path) const {
            const auto relative = stdc::path::lexically_relative(path, document->rootPath());
            if (relative.empty() || *relative.begin() == "..") {
                return {};
            }
            return document->session()->logicalPathOf(relative);
        }

        // Returns the resampler of the settings and the format of its frequency maps, or shows
        // a message box with title and returns std::nullopt if either is absent
        std::optional<std::pair<std::filesystem::path, kit::FrequencyFormat *>>
            generationTools(const QString &title) {
            stdc_decl_t;
            const auto resampler = pathOf(editor->settings().rendering().resampler());
            if (resampler.empty()) {
                QMessageBox::information(&decl, title,
                                         tr("Set the resampler in the settings first."));
                return std::nullopt;
            }
            const auto format = editor->frequencyFormats().formatForResampler(resampler);
            if (!format) {
                QMessageBox::information(&decl, title,
                                         tr("No format of frequency maps is registered for the "
                                            "resampler."));
                return std::nullopt;
            }
            return std::pair(resampler, format);
        }

        enum Generation {
            MapsOnly,      // The missing maps
            CachesOnly,    // The analysis caches of every audio file, and the missing maps
            MapsAndCaches, // The missing maps and caches of every audio file, after a repair
        };

        // Generates the files that generation names for wavs in a progress dialog that cancels the
        // generation, and shows the map of the current row again. Returns whether the run
        // completed without a cancel and without a diagnostic.
        bool generate(const QString &title, const kit::FrequencyFormat &format,
                      const std::filesystem::path &resampler,
                      const QList<std::filesystem::path> &wavs, Generation generation) {
            stdc_decl_t;
            QProgressDialog progress(tr("Generating frequency maps with the resampler..."),
                                     tr("Cancel"), 0, int(wavs.size()), &decl);
            progress.setWindowTitle(title);
            progress.setWindowModality(Qt::WindowModal);
            progress.setMinimumDuration(0);
            progress.setValue(0);
            std::atomic<bool> canceled = false;
            QObject::connect(&progress, &QProgressDialog::canceled,
                             [&canceled] { canceled = true; });

            kit::DiagnosticList diagnostics;
            kit::FrequencyMapGenerator::Outcome outcome;
            generator.threadCount = editor->settings().frequencyMaps().threadCount();
            generator.everyFile = generation != MapsOnly;
            applyLogSettings();
            QEventLoop loop;
            const auto worker = QThread::create([&] {
                outcome = generator.generate(
                    format, resampler, wavs, diagnostics,
                    [&progress](int done) {
                        // Dropped with the dialog if it is gone
                        QMetaObject::invokeMethod(
                            &progress, [&progress, done] { progress.setValue(done); },
                            Qt::QueuedConnection);
                    },
                    [&canceled] { return canceled.load(); });
            });
            QObject::connect(worker, &QThread::finished, &loop, &QEventLoop::quit);
            worker->start();
            loop.exec();
            worker->wait();
            delete worker;
            progress.reset();

            model->refreshFrequencyMaps();
            frequencyShown = {};
            showFrequency();
            const auto done =
                generation == CachesOnly
                    ? tr("The analysis caches or the frequency maps of %n audio file(s) were "
                         "generated.",
                         nullptr, outcome.generated)
                : generation == MapsAndCaches
                    ? tr("The frequency maps and analysis caches of %n audio file(s) were "
                         "generated again.",
                         nullptr, outcome.generated)
                    : tr("%n frequency map(s) were generated.", nullptr, outcome.generated);
            decl.statusBar()->showMessage(done);
            reportGeneration(title, format, resampler, diagnostics, outcome.failures);
            return !canceled.load() && diagnostics.isEmpty();
        }

        // Sets the runs held in the log and its size limit, those of the render log unless the
        // settings of the frequency maps specify their own. Applied as a generation starts, so
        // that the first output of the run empties a log of the latest run only.
        void applyLogSettings() {
            const auto &maps = editor->settings().frequencyMaps();
            const auto &rendering = editor->settings().rendering();
            const bool follows = maps.followsRenderLog();
            const bool accumulated =
                follows ? rendering.isLogAccumulated() : maps.isLogAccumulated();
            generationLog->setMode(accumulated ? kit::SynthToolOutputLog::Accumulated
                                               : kit::SynthToolOutputLog::Latest);
            generationLog->setLimit(follows ? rendering.logLimit() : maps.logLimit());
        }

        // Shows the probable causes of the failed resampler calls of a generation above the
        // audio files without maps, which the details list one per line, as the project window
        // reports a render.
        void reportGeneration(const QString &title, const kit::FrequencyFormat &format,
                              const std::filesystem::path &resampler,
                              const kit::DiagnosticList &diagnostics,
                              const QList<kit::SynthFailure> &failures) {
            stdc_decl_t;
            QList<kit::SynthDiagnosis> diagnoses;
            if (!failures.isEmpty()) {
                kit::SynthDiagnosisContext context;
                context.operation = kit::SynthDiagnosisContext::FrequencyMapGeneration;
                context.frequencyFormat = format.name();
                context.synthTools.resampler = resampler;
                context.failures = failures;
                diagnoses = editor->synthDiagnosers().diagnose(context);
            }
            const auto missing = std::count_if(
                diagnostics.cbegin(), diagnostics.cend(), [](const kit::Diagnostic &diagnostic) {
                    return diagnostic.severity == kit::Diagnostic::Warning;
                });
            const auto summary =
                missing > 0 ? tr("%n audio file(s) have no frequency map.", nullptr, int(missing))
                            : tr("The frequency maps could not be generated.");
            DiagnosticBox::report(&decl, title, summary, diagnostics, diagnoses,
                                  DiagnosticBox::OrderedList);
        }

        // Fits each column to its header and its cells in view, a column of names up to a width
        // beyond which the names are elided, so that the columns fit a window that is not
        // maximized.
        void fitColumns() {
            table->resizeColumnsToContents();
            const int widest = table->fontMetrics().horizontalAdvance(QLatin1Char('M')) * 20;
            for (const int column :
                 {VoiceBankEntryModel::DirectoryColumn, VoiceBankEntryModel::FileColumn,
                  VoiceBankEntryModel::AliasColumn}) {
                if (table->columnWidth(column) > widest) {
                    table->setColumnWidth(column, widest);
                }
            }
        }

        static bool hasMap(const kit::FrequencyFormat &format, const std::filesystem::path &wav) {
            return format.mapsExist(wav.parent_path(), {wav.filename()}).value(0);
        }

        // Shows the frequency map of the audio of the current row in the format in effect, of
        // which the table shows the presence. The box marks the formats without a map for the
        // audio file.
        void showFrequency() {
            stdc_decl_t;
            const int row = decl.currentRow();
            const auto path = row >= 0 ? audioPathOf(row) : std::filesystem::path();
            const auto id = currentFrequencyFormat();
            const auto &formats = editor->frequencyFormats();
            model->setFrequencyFormat(formats.format(id));
            for (int i = 1; frequencyBox && i < frequencyBox->count(); ++i) {
                const auto format = formats.format(frequencyBox->itemData(i).toString());
                const bool present = !path.empty() && format && hasMap(*format, path);
                frequencyBox->setItemText(i, present ? format->name()
                                                     : tr("%1 (none)").arg(format->name()));
            }
            const auto audio = waveform->audio();
            if (frequencyShown == std::pair{path, id}) {
                return;
            }
            frequencyShown = {path, id};
            const auto format = formats.format(id);
            std::optional<kit::FrequencyMap> table;
            if (format && audio && hasMap(*format, path)) {
                kit::DiagnosticList diagnostics;
                table = format->read(path, audio->sampleRate, diagnostics);
                if (!table) {
                    decl.statusBar()->showMessage(diagnostics.value(0).message);
                }
            }
            waveform->setFrequencyMap(table);
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
                                     DiagnosticBox::report(&decl, tr("Edit Entry"), diagnostics);
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

        // Returns a widget of the sample tool bar that holds box after its label.
        static QWidget *labelled(QWidget *parent, const QString &label, QWidget *box) {
            auto widget = new QWidget(parent);
            auto layout = new QHBoxLayout(widget);
            layout->setContentsMargins(4, 0, 4, 0);
            auto text = new QLabel(label);
            text->setBuddy(box);
            layout->addWidget(text);
            layout->addWidget(box);
            return widget;
        }

        // Creates the tool bar above the waveform, which QActionKit fills from the layout of
        // helloutau.voiceBank.sampleToolBar: the buttons of the preview, the pitch and the length
        // of the synthesized note, and the format of the frequency map. QActionKit creates the
        // boxes again whenever it rebuilds the tool bar, so the window keeps their values.
        void initSampleToolBar() {
            stdc_decl_t;
            sampleToolBar = new QToolBar();
            sampleToolBar->setObjectName(QStringLiteral("sampleToolBar"));
            followToolBarPalette(sampleToolBar);
            context->addToolBar(QStringLiteral("helloutau.voiceBank.sampleToolBar"), sampleToolBar);
            context->addWidgetFactory(
                QStringLiteral("helloutau.voiceBank.pitch"), [this](QWidget *parent) -> QWidget * {
                    auto box = new QComboBox();
                    for (int noteNum = kit::VoicePrefix::minimumKey;
                         noteNum <= kit::VoicePrefix::maximumKey; ++noteNum) {
                        box->addItem(noteName(noteNum), noteNum);
                    }
                    box->setCurrentIndex(box->findData(synthesizedPitch));
                    QObject::connect(box, &QComboBox::currentIndexChanged, box, [this, box] {
                        synthesizedPitch = box->currentData().toInt();
                    });
                    pitchBox = box;
                    return labelled(parent, tr("&Pitch:"), box);
                });
            context->addWidgetFactory(
                QStringLiteral("helloutau.voiceBank.length"), [this](QWidget *parent) -> QWidget * {
                    auto box = new QSpinBox();
                    box->setRange(15, 7680);
                    box->setSingleStep(15);
                    box->setSuffix(tr(" ticks"));
                    box->setValue(synthesizedLength);
                    QObject::connect(box, &QSpinBox::valueChanged, box,
                                     [this](int value) { synthesizedLength = value; });
                    lengthBox = box;
                    return labelled(parent, tr("&Length:"), box);
                });
            // The frequency map drawn over the audio, that of the resampler of the settings at
            // first (docs/FrequencyMaps.md)
            context->addWidgetFactory(
                QStringLiteral("helloutau.voiceBank.frequencyFormat"),
                [this](QWidget *parent) -> QWidget * {
                    auto box = new QComboBox();
                    box->setObjectName(QStringLiteral("frequencyFormat"));
                    frequencyBox = box;
                    fillFrequencyBox();
                    QObject::connect(box, &QComboBox::currentIndexChanged, box, [this, box] {
                        frequencyFormat = box->currentData().toString();
                        showFrequency();
                    });
                    return labelled(parent, tr("&F0:"), box);
                });
            QObject::connect(&editor->frequencyFormats(), &kit::FrequencyFormats::formatsChanged,
                             &decl, [this] {
                                 frequencyFormat = currentFrequencyFormat();
                                 fillFrequencyBox();
                                 frequencyShown = {};
                                 showFrequency();
                             });
        }

        // Creates the preview of the audio and of the synthesized note, with the playhead on the
        // waveform while it plays.
        void initPreview() {
            stdc_decl_t;
            preview = new SamplePreview(&decl);
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
                                 DiagnosticBox::report(&decl, tr("Preview"), diagnostics);
                             });
            actions.value(QStringLiteral("helloutau.playback.stop"))->setEnabled(false);
        }

        void playAudio(double from, std::optional<double> to) {
            stdc_decl_t;
            if (!waveform->audio()) {
                return;
            }
            kit::DiagnosticList diagnostics;
            preview->play(waveform->audio(), from, to, diagnostics);
            DiagnosticBox::report(&decl, tr("Play"), diagnostics);
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
            preview->synthesize(sample, synthesizedPitch, synthesizedLength,
                                pathOf(editor->settings().rendering().resampler()), diagnostics);
            DiagnosticBox::report(&decl, tr("Synthesize"), diagnostics);
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
            // The prefix map names folders by their logical paths, as it is read in an encoding.
            synthesizedPitch = SamplePreview::noteNumFor(
                prefixMap, document->session()->logicalPathOf(key.first), key.second);
            if (pitchBox) {
                pitchBox->setCurrentIndex(pitchBox->findData(synthesizedPitch));
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
            // A folder is shown under its logical name, which a record of its parent changes.
            QList<std::filesystem::path> logical;
            for (const auto &path : directories + excluded) {
                logical.push_back(document->session()->logicalPathOf(path));
            }
            if (directories == shownDirectories && excluded == shownExcluded &&
                logical == shownLogical && tree->topLevelItemCount() > 0) {
                return;
            }
            shownDirectories = directories;
            shownExcluded = excluded;
            shownLogical = logical;

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
                    label = QString::fromStdU16String(
                        document->session()->logicalPathOf(path).filename().u16string());
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
            decl.setWindowTitle(
                QStringLiteral("%1[*] - %2")
                    .arg(document->displayName(), SoftwareBuildChannel::applicationTitle()));
            decl.setWindowModified(document->isModified());
        }

        void updateUndoActions() {
            const auto session = document->session();
            actions.value(QStringLiteral("helloutau.edit.undo"))->setEnabled(session->canUndo());
            actions.value(QStringLiteral("helloutau.edit.redo"))->setEnabled(session->canRedo());
        }

        // Shows a message box in which the user saves or discards a modified voice bank before it
        // is closed. Returns whether closing may proceed.
        bool maybeSave() {
            stdc_decl_t;
            info->commit();
            if (!document->isModified()) {
                return true;
            }
            QMessageBox box(QMessageBox::Warning, tr("HelloUtau"),
                            tr("Save the changes to %1?").arg(document->displayName()),
                            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, &decl);
            box.button(QMessageBox::Discard)->setText(tr("Do not save"));
            const auto answer = box.exec();
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
        impl.generator.process = std::make_shared<kit::SynthToolProcess>(impl.generationLog);
        impl.initActions();
        impl.initWidgets();
        impl.initTableEditing();
        impl.initFind();
        impl.initWaveform();
        impl.refreshTree();
        impl.initScheduler();
        // File names left to determine when the voice bank was opened
        impl.updateBar();

        const auto session = impl.document->session();
        connect(session, &kit::VoiceBankSession::stepChanged, this, [this] {
            stdc_impl_t;
            impl.updateUndoActions();
            impl.refreshTree();
            impl.updateTreeTips();
            impl.updateBar();
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
        resize(1200, 800);
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

    QComboBox *VoiceBankWindow::frequencyFormatBox() const {
        stdc_impl_t;
        return impl.frequencyBox;
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
        DiagnosticBox::report(this, tr("Convert Encoding"), diagnostics);
        return converted;
    }

    bool VoiceBankWindow::rereadCharset(const std::filesystem::path &directory,
                                        const QString &charset) {
        stdc_impl_t;
        kit::DiagnosticList diagnostics;
        const bool read = impl.document->session()->reread(directory, charset, diagnostics);
        DiagnosticBox::report(this, tr("Read Again in Encoding"), diagnostics);
        if (read) {
            impl.model->refresh();
        }
        return read;
    }

    bool VoiceBankWindow::rereadCharsets() {
        stdc_impl_t;
        // The dialog shows the bytes on disk, which reading again decodes.
        kit::DiagnosticList diagnostics;
        std::vector<kit::VoiceBankDirectorySource> sources;
        QStringList charsets;
        const auto list = kit::VoiceBankRef(impl.document->session()).directories();
        for (int i = 0; i < list.size(); ++i) {
            const auto directory = list.at(i);
            auto source = kit::VoiceBankSource::readDirectory(impl.document->rootPath(),
                                                              directory.path(), diagnostics);
            if (!source || source->textFiles().empty() || source->otoDeclaresUtf8()) {
                continue;
            }
            sources.push_back(std::move(*source));
            charsets.push_back(directory.charset());
        }
        DiagnosticBox::report(this, tr("Read Folders Again in Encodings"), diagnostics);
        if (sources.empty()) {
            QMessageBox::information(this, tr("Read Folders Again in Encodings"),
                                     tr("No folder of the voice bank can be read in another "
                                        "encoding. Each folder has either no text files or an "
                                        "oto.ini that declares UTF-8."));
            return false;
        }

        QList<const kit::VoiceBankDirectorySource *> pointers;
        for (const auto &source : sources) {
            pointers.push_back(&source);
        }
        VoiceBankDecodingDialog dialog(this);
        dialog.setRoot(impl.document->rootPath());
        dialog.setDirectories(pointers);
        dialog.setCurrentCharsets(charsets);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }
        QList<std::pair<std::filesystem::path, QString>> changed;
        for (const int index : dialog.changedFolders()) {
            changed.push_back({sources[index].path, dialog.charsetOf(index)});
        }
        return !changed.isEmpty() && rereadCharsets(changed);
    }

    bool VoiceBankWindow::rereadCharsets(
        const QList<std::pair<std::filesystem::path, QString>> &charsets) {
        stdc_impl_t;
        kit::DiagnosticList diagnostics;
        const bool read = impl.document->session()->reread(charsets, diagnostics);
        DiagnosticBox::report(this, tr("Read Folders Again in Encodings"), diagnostics);
        impl.model->refresh();
        return read;
    }

    int VoiceBankWindow::fileNamesToDetermine() const {
        stdc_impl_t;
        const auto session = impl.document->session();
        const auto list = kit::VoiceBankRef(session).directories();
        int count = 0;
        for (int i = 0; i < list.size(); ++i) {
            const auto directory = list.at(i);
            if (const auto record = directory.fileNameEncoding()) {
                for (const auto &name : session->undeterminedNames(directory.path())) {
                    count += record->restore(name).has_value();
                }
            }
        }
        return count;
    }

    bool VoiceBankWindow::determineFileNames() {
        stdc_impl_t;
        // The dialog lists the names on disk under the records of the tree, which saving writes,
        // rather than under the configurations on disk.
        const auto session = impl.document->session();
        kit::DiagnosticList diagnostics;
        std::vector<kit::VoiceBankDirectorySource> sources;
        QStringList charsets;
        const auto list = kit::VoiceBankRef(session).directories();
        for (int i = 0; i < list.size(); ++i) {
            const auto directory = list.at(i);
            const auto record = directory.fileNameEncoding();
            if (!record || session->undeterminedNames(directory.path()).isEmpty()) {
                continue;
            }
            auto source = kit::VoiceBankSource::readDirectory(impl.document->rootPath(),
                                                              directory.path(), diagnostics);
            if (!source) {
                continue;
            }
            if (!source->config) {
                source->config = kit::VoiceBankConfiguration();
            }
            source->config->fileNameEncoding = record;
            if (!kit::MisdecodedNameQuestion::of(*source, directory.charset())) {
                continue;
            }
            sources.push_back(std::move(*source));
            charsets.push_back(directory.charset());
        }
        DiagnosticBox::report(this, tr("Determine File Names"), diagnostics);
        if (sources.empty()) {
            QMessageBox::information(this, tr("Determine File Names"),
                                     tr("No file name of the voice bank is to be determined."));
            return false;
        }

        QList<kit::VoiceBankCharsetSelector::Question> questions;
        for (std::size_t i = 0; i < sources.size(); ++i) {
            questions.push_back({&sources[i], charsets.at(qsizetype(i))});
        }
        VoiceBankDecodingDialog dialog(this);
        dialog.setRoot(impl.document->rootPath());
        dialog.setQuestions(questions);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }
        QList<std::pair<std::filesystem::path, kit::VoiceBankConfiguration::FileNameEncoding>>
            records;
        const auto answers = dialog.answers();
        for (qsizetype i = 0; i < answers.size(); ++i) {
            if (answers.at(i).fileNameEncoding) {
                records.push_back({sources[std::size_t(i)].path, *answers.at(i).fileNameEncoding});
            }
        }
        return !records.isEmpty() && determineFileNames(records);
    }

    bool VoiceBankWindow::determineFileNames(
        const QList<std::pair<std::filesystem::path, kit::VoiceBankConfiguration::FileNameEncoding>>
            &records) {
        stdc_impl_t;
        kit::DiagnosticList diagnostics;
        bool done = false;
        {
            auto transaction = impl.document->session()->transaction(tr("Determine File Names"));
            bool set = true;
            for (const auto &[path, record] : records) {
                const auto directory = impl.directoryRef(path);
                set = set && directory &&
                      kit::VoiceBankEdits::setFileNameEncoding(*directory, record, diagnostics);
            }
            done = set && transaction.commit(diagnostics);
        }
        DiagnosticBox::report(this, tr("Determine File Names"), diagnostics);
        impl.updateBar();
        return done;
    }

    bool VoiceBankWindow::convertEncodings() {
        stdc_impl_t;
        const auto session = impl.document->session();
        QList<VoiceBankTranscodingDialog::Folder> folders;
        for (const auto &listing : session->nameListings()) {
            const auto conversion = kit::MisdecodedNameConversion::of(listing);
            const bool excluded = !impl.directoryRef(listing.directory).has_value();
            const bool hasText = !excluded && !listing.charset.isEmpty();
            // Every name that is not ASCII either restores or does not.
            if (!hasText && conversion.choices().isEmpty() && conversion.unrestorable().isEmpty()) {
                continue;
            }
            folders.push_back({listing, impl.folderName(listing.directory), excluded});
        }
        if (folders.isEmpty()) {
            QMessageBox::information(this, tr("Convert Encodings"),
                                     tr("No folder of the voice bank has text files or a file "
                                        "name that is not ASCII."));
            return false;
        }

        VoiceBankTranscodingDialog dialog(this);
        dialog.setFolders(impl.document->rootPath(), folders);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }
        const auto conversion = dialog.conversion();
        int renamed = 0;
        if (conversion.method == VoiceBankTranscodingDialog::Rename) {
            for (const auto &[names, checked] : conversion.names) {
                renamed += int(checked.size());
            }
        }
        if (renamed > 0 &&
            QMessageBox::warning(
                this, tr("Convert Encodings"),
                tr("Rename %n file(s) and folder(s) on disk? The voice bank is saved first. The "
                   "renaming cannot be undone, and the window opens again without the undo "
                   "history. UTAU on a system in the encoding in which the names were read no "
                   "longer finds the renamed files.",
                   nullptr, renamed),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
            return false;
        }
        return convertEncodings(conversion);
    }

    bool VoiceBankWindow::convertEncodings(
        const VoiceBankTranscodingDialog::Conversion &conversion) {
        stdc_impl_t;
        const auto title = tr("Convert Encodings");
        const bool rename = conversion.method == VoiceBankTranscodingDialog::Rename;
        kit::DiagnosticList diagnostics;

        // The text files and the names read as misdecoded, as one undo step
        if (!conversion.charsets.isEmpty() || (!rename && !conversion.names.isEmpty())) {
            auto transaction = impl.document->session()->transaction(title);
            bool set = true;
            for (const auto &[path, charset] : conversion.charsets) {
                const auto directory = impl.directoryRef(path);
                set = set && directory &&
                      kit::VoiceBankEdits::convertCharset(*directory, charset, diagnostics);
            }
            if (conversion.method == VoiceBankTranscodingDialog::ReadAsMisdecoded) {
                for (const auto &[names, checked] : conversion.names) {
                    const auto directory = impl.directoryRef(names.directory());
                    set = set && directory &&
                          kit::VoiceBankEdits::setFileNameEncoding(
                              *directory, names.record(checked), diagnostics);
                }
            }
            if (!set || !transaction.commit(diagnostics)) {
                DiagnosticBox::report(this, title, diagnostics);
                impl.model->refresh();
                return false;
            }
            impl.model->refresh();
            impl.updateBar();
        }

        QList<kit::VoiceBankSession::NameConversion> renames;
        if (rename) {
            for (const auto &[names, checked] : conversion.names) {
                if (!checked.isEmpty()) {
                    renames.push_back({names.directory(), names.keptRecord(), checked});
                }
            }
        }
        if (renames.isEmpty()) {
            DiagnosticBox::report(this, title, diagnostics);
            return true;
        }
        if (!impl.document->renameOnDisk(renames, &impl.editor->frequencyFormats(), diagnostics)) {
            DiagnosticBox::report(this, title, diagnostics);
            return false;
        }
        // The tree and the undo history no longer hold the disk.
        const auto reopened = impl.editor->reopenVoiceBank(this);
        DiagnosticBox::report(reopened ? static_cast<QWidget *>(reopened) : this, title,
                              diagnostics);
        return true;
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
        DiagnosticBox::report(this, tr("Insert Entry"), diagnostics);
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
                DiagnosticBox::report(this, tr("Duplicate Entries"), diagnostics);
                return false;
            }
        }
        const bool committed = transaction.commit(diagnostics);
        DiagnosticBox::report(this, tr("Duplicate Entries"), diagnostics);
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
                DiagnosticBox::report(this, tr("Include Audio Files"), diagnostics);
                return false;
            }
        }
        const bool committed = transaction.commit(diagnostics);
        DiagnosticBox::report(this, tr("Include Audio Files"), diagnostics);
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
                DiagnosticBox::report(this, tr("Remove Entries"), diagnostics);
                return false;
            }
        }
        const bool committed = transaction.commit(diagnostics);
        DiagnosticBox::report(this, tr("Remove Entries"), diagnostics);
        if (committed) {
            impl.model->refresh();
        }
        return committed;
    }

    bool VoiceBankWindow::editFrequencyMaps() {
        stdc_impl_t;
        const auto title = tr("Edit Frequency Maps");
        const auto tools = impl.generationTools(title);
        if (!tools) {
            return false;
        }
        const auto format = tools->second;
        if (!format->canWrite()) {
            QMessageBox::information(
                this, title,
                tr("HelloUtau cannot write frequency maps in the format %1.").arg(format->name()));
            return false;
        }
        auto files = impl.mapListingOf<EditFrequencyMapsDialog::File>(*format);
        if (files.isEmpty()) {
            QMessageBox::information(this, title, tr("The voice bank has no audio files."));
            return false;
        }
        // The audio file of the current row is current in the dialog, rather than the selection.
        const int row = currentRow();
        const auto current =
            row >= 0 ? impl.audioPathOf(row).lexically_normal() : std::filesystem::path();
        for (auto &file : files) {
            file.selected = !current.empty() && file.path == current;
        }
        EditFrequencyMapsDialog dialog(impl.editor, *format, std::move(files),
                                       impl.document->rootPath(), this);
        dialog.setVoiceBankName(impl.document->displayName());
        dialog.exec();
        impl.model->refreshFrequencyMaps();
        impl.frequencyShown = {};
        impl.showFrequency();
        return true;
    }

    bool VoiceBankWindow::generateFrequencyMaps() {
        stdc_impl_t;
        const auto title = tr("Generate Frequency Maps");
        const auto tools = impl.generationTools(title);
        if (!tools) {
            return false;
        }
        const auto format = tools->second;

        auto files = impl.mapListingOf<GenerateFrequencyMapsDialog::File>(*format);
        if (files.isEmpty()) {
            QMessageBox::information(this, title, tr("The voice bank has no audio files."));
            return false;
        }
        GenerateFrequencyMapsDialog dialog(
            std::move(files), impl.document->rootPath(), format->name(), cacheNamesOf(*format),
            impl.editor->settings().frequencyMaps().regeneration(), this);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }
        return generateFrequencyMaps(dialog.checkedFiles(), dialog.regeneration());
    }

    bool VoiceBankWindow::generateFrequencyMaps(const QList<std::filesystem::path> &wavs) {
        stdc_impl_t;
        return generateFrequencyMaps(wavs, impl.editor->settings().frequencyMaps().regeneration());
    }

    bool VoiceBankWindow::generateFrequencyMaps(const QList<std::filesystem::path> &wavs,
                                                FrequencyMapSettings::Regeneration regeneration) {
        stdc_impl_t;
        const auto title = tr("Generate Frequency Maps");
        const auto tools = impl.generationTools(title);
        if (!tools) {
            return false;
        }
        const auto &[resampler, format] = *tools;

        std::map<std::filesystem::path, QList<std::filesystem::path>> byFolder;
        for (const auto &wav : wavs) {
            byFolder[wav.parent_path()].push_back(wav.filename());
        }
        // The files that have a map, by folder
        std::map<std::filesystem::path, QList<std::filesystem::path>> present;
        for (const auto &[folder, names] : byFolder) {
            const auto exist = format->mapsExist(folder, names);
            for (qsizetype i = 0; i < names.size(); ++i) {
                if (exist.value(i)) {
                    present[folder].push_back(names.at(i));
                }
            }
        }

        // Only the analysis caches of an existing map are removed and generated again, which
        // requires a format with caches.
        const bool cachesOnly = !present.empty() && !format->analysisCaches().isEmpty() &&
                                regeneration == FrequencyMapSettings::CachesOnly;
        kit::DiagnosticList diagnostics;
        bool removed = true;
        for (const auto &[folder, names] : present) {
            removed =
                (cachesOnly
                     ? kit::FrequencyFormats::invalidateCaches(*format, folder, names, diagnostics)
                     : kit::FrequencyFormats::removeMaps(*format, folder, names, diagnostics)) &&
                removed;
        }
        if (!removed) {
            DiagnosticBox::report(this, title, diagnostics);
            return false;
        }
        return impl.generate(title, *format, resampler, wavs,
                             cachesOnly ? Impl::CachesOnly : Impl::MapsOnly);
    }

    bool VoiceBankWindow::repairFrequencyMaps() {
        stdc_impl_t;
        const auto title = tr("Repair Frequency Maps");
        const auto tools = impl.generationTools(title);
        if (!tools) {
            return false;
        }
        const auto &[resampler, format] = *tools;
        RepairFrequencyMapsDialog::Files files;
        for (const auto &[folder, names] : impl.audioByFolder()) {
            auto found = kit::FrequencyFormats::mapFilesOf(*format, folder, names);
            if (!found.isEmpty()) {
                files[folder] = std::move(found);
            }
        }
        if (files.empty()) {
            QMessageBox::information(this, title,
                                     tr("The voice bank has no frequency maps or analysis caches "
                                        "of the format %1.")
                                         .arg(format->name()));
            return false;
        }
        RepairFrequencyMapsDialog dialog(
            *format, impl.document->rootPath(), std::move(files),
            [this](const std::filesystem::path &path) {
                stdc_impl_t;
                return impl.shownPathOf(path);
            },
            this);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }

        kit::DiagnosticList diagnostics;
        QList<std::filesystem::path> wavs;
        for (const auto &[folder, checked] : dialog.checkedFiles()) {
            for (const auto &name :
                 kit::FrequencyFormats::repairMapFiles(*format, folder, checked, diagnostics)) {
                wavs.push_back(folder / name);
            }
        }
        if (!diagnostics.isEmpty()) {
            DiagnosticBox::report(this, title, diagnostics);
        }
        if (wavs.isEmpty()) {
            impl.model->refreshFrequencyMaps();
            impl.frequencyShown = {};
            impl.showFrequency();
            return diagnostics.isEmpty();
        }
        return impl.generate(title, *format, resampler, wavs, Impl::MapsAndCaches) &&
               diagnostics.isEmpty();
    }

    bool VoiceBankWindow::convertFrequencyMaps() {
        stdc_impl_t;
        const auto title = tr("Convert Frequency Maps");
        const auto &formats = impl.editor->frequencyFormats();
        const auto all = formats.formats();
        const auto selected = impl.selectedAudioPaths();
        QList<ConvertFrequencyMapsDialog::File> files;
        for (const auto &[folder, names] : impl.audioByFolder()) {
            QList<QList<bool>> present;
            for (const auto format : all) {
                present.push_back(format->mapsExist(folder, names));
            }
            for (qsizetype i = 0; i < names.size(); ++i) {
                ConvertFrequencyMapsDialog::File file;
                file.path = (folder / names.at(i)).lexically_normal();
                for (const auto &maps : std::as_const(present)) {
                    file.maps.push_back(maps.value(i));
                }
                file.selected = selected.contains(file.path);
                file.shownPath = impl.shownPathOf(file.path);
                files.push_back(std::move(file));
            }
        }
        if (files.isEmpty()) {
            QMessageBox::information(this, title, tr("The voice bank has no audio files."));
            return false;
        }

        auto source = formats.format(impl.currentFrequencyFormat());
        if (!source) {
            source =
                formats.formatForResampler(pathOf(impl.editor->settings().rendering().resampler()));
        }
        ConvertFrequencyMapsDialog dialog(std::move(files), impl.document->rootPath(), all, source,
                                          this);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }
        return convertFrequencyMaps(dialog.checkedFiles(), *dialog.sourceFormat(),
                                    *dialog.targetFormat());
    }

    bool VoiceBankWindow::convertFrequencyMaps(const QList<std::filesystem::path> &wavs,
                                               const kit::FrequencyFormat &from,
                                               const kit::FrequencyFormat &to) {
        stdc_impl_t;
        const auto title = tr("Convert Frequency Maps");
        std::map<std::filesystem::path, QList<std::filesystem::path>> byFolder;
        for (const auto &wav : wavs) {
            byFolder[wav.parent_path()].push_back(wav.filename());
        }

        kit::DiagnosticList diagnostics;
        kit::FrequencyMapConverter::Outcome outcome;
        QApplication::setOverrideCursor(Qt::WaitCursor);
        for (const auto &[folder, names] : byFolder) {
            const auto done =
                kit::FrequencyMapConverter::convert(from, to, folder, names, diagnostics);
            outcome.converted += done.converted;
            outcome.resampled += done.resampled;
        }
        QApplication::restoreOverrideCursor();

        impl.model->refreshFrequencyMaps();
        impl.frequencyShown = {};
        impl.showFrequency();
        if (outcome.converted == 0 && diagnostics.isEmpty()) {
            QMessageBox::information(
                this, title,
                tr("No audio file has a frequency map of the format %1.").arg(from.name()));
            return false;
        }
        auto message = tr("%n frequency map(s) were converted.", nullptr, outcome.converted);
        if (outcome.resampled > 0) {
            message +=
                QLatin1Char(' ') + tr("%n of them were resampled to the hop of the format %1.",
                                      nullptr, outcome.resampled)
                                       .arg(to.name());
        }
        statusBar()->showMessage(message);
        DiagnosticBox::report(this, title, diagnostics);
        return diagnostics.isEmpty();
    }

    bool VoiceBankWindow::removeFrequencyMaps() {
        stdc_impl_t;
        const auto title = tr("Remove Frequency Maps");
        const auto resampler = pathOf(impl.editor->settings().rendering().resampler());
        const auto format = impl.editor->frequencyFormats().formatForResampler(resampler);
        if (!format) {
            QMessageBox::information(this, title,
                                     tr("No format of frequency maps is registered for the "
                                        "resampler."));
            return false;
        }
        auto files = impl.mapListingOf<RemoveFrequencyMapsDialog::File>(*format);
        if (files.isEmpty()) {
            QMessageBox::information(this, title, tr("The voice bank has no audio files."));
            return false;
        }
        RemoveFrequencyMapsDialog dialog(std::move(files), impl.document->rootPath(),
                                         format->name(), cacheNamesOf(*format), this);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }
        return removeFrequencyMaps(dialog.checkedFiles());
    }

    bool VoiceBankWindow::removeFrequencyMaps(const QList<std::filesystem::path> &wavs) {
        stdc_impl_t;
        const auto title = tr("Remove Frequency Maps");
        const auto resampler = pathOf(impl.editor->settings().rendering().resampler());
        const auto format = impl.editor->frequencyFormats().formatForResampler(resampler);
        if (!format) {
            QMessageBox::information(this, title,
                                     tr("No format of frequency maps is registered for the "
                                        "resampler."));
            return false;
        }
        std::map<std::filesystem::path, QList<std::filesystem::path>> byFolder;
        for (const auto &wav : wavs) {
            byFolder[wav.parent_path()].push_back(wav.filename());
        }
        kit::DiagnosticList diagnostics;
        bool removed = true;
        for (const auto &[folder, names] : byFolder) {
            removed =
                kit::FrequencyFormats::removeMaps(*format, folder, names, diagnostics) && removed;
        }
        impl.model->refreshFrequencyMaps();
        impl.frequencyShown = {};
        impl.showFrequency();
        DiagnosticBox::report(this, title, diagnostics);
        return removed;
    }

    kit::FrequencyMapGenerator &VoiceBankWindow::frequencyMapGenerator() {
        stdc_impl_t;
        return impl.generator;
    }

    std::shared_ptr<kit::SynthToolOutputLog> VoiceBankWindow::frequencyMapLog() const {
        stdc_impl_t;
        return impl.generationLog;
    }

    bool VoiceBankWindow::processAudioFiles() {
        stdc_impl_t;
        const auto title = tr("Process Audio Files");
        QList<std::filesystem::path> paths;
        for (const auto &[folder, names] : impl.audioByFolder()) {
            for (const auto &name : names) {
                paths.push_back((folder / name).lexically_normal());
            }
        }
        if (paths.isEmpty()) {
            QMessageBox::information(this, title, tr("The voice bank has no audio files."));
            return false;
        }

        // The properties of the files are read in the background, with the threads of
        // TaskSettings::threadCount(), while a progress dialog cancels the reading.
        kit::DiagnosticList diagnostics;
        QList<std::optional<kit::AudioFileProcessor::Inspection>> inspections;
        {
            QProgressDialog reading(tr("Reading the audio files..."), tr("Cancel"), 0,
                                    int(paths.size()), this);
            reading.setWindowTitle(title);
            reading.setWindowModality(Qt::WindowModal);
            reading.setMinimumDuration(0);
            reading.setValue(0);
            reading.show();
            std::atomic<bool> canceled = false;
            connect(&reading, &QProgressDialog::canceled, this, [&canceled] { canceled = true; });
            const int threads = impl.editor->settings().tasks().threadCount();
            QEventLoop loop;
            QThread *worker = nullptr;
            // Started once the event loop runs, so that no file is read if a cancellation comes
            // first, such as a click on the dialog just shown.
            QTimer::singleShot(0, &loop, [&] {
                worker = QThread::create([&] {
                    inspections = kit::AudioFileProcessor::inspectFiles(
                        paths, threads, diagnostics,
                        [&reading](int done) {
                            // Dropped with the dialog if it is gone
                            QMetaObject::invokeMethod(
                                &reading, [&reading, done] { reading.setValue(done); },
                                Qt::QueuedConnection);
                        },
                        [&canceled] { return canceled.load(); });
                });
                connect(worker, &QThread::finished, &loop, &QEventLoop::quit);
                worker->start();
            });
            loop.exec();
            worker->wait();
            delete worker;
            reading.reset();
            if (canceled.load()) {
                QMessageBox::warning(this, title,
                                     tr("The reading of the audio files was canceled, and no "
                                        "file is processed."));
                return false;
            }
        }

        const auto selected = impl.selectedAudioPaths();
        QList<ProcessAudioFilesDialog::File> files;
        for (qsizetype i = 0; i < paths.size(); ++i) {
            if (const auto &inspection = inspections.at(i)) {
                files.push_back({paths.at(i), *inspection, selected.contains(paths.at(i)),
                                 impl.shownPathOf(paths.at(i))});
            }
        }
        if (files.isEmpty()) {
            DiagnosticBox::report(this, title, diagnostics);
            return false;
        }

        // Beside the voice bank, so that the backup is not read as a folder of the voice bank
        const auto root = impl.document->rootPath();
        auto backup = root.parent_path() / root.filename();
        backup += QStringLiteral("-backup-%1")
                      .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")))
                      .toStdU16String();
        ProcessAudioFilesDialog dialog(std::move(files), root, backup, this);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }
        const auto checked = dialog.checkedFiles();
        const auto options = dialog.options();
        auto folder = dialog.backupFolder();
        if (folder) {
            folder = freeFolderOf(*folder);
        }

        QProgressDialog progress(tr("Processing audio files..."), tr("Cancel"), 0,
                                 int(checked.size()), this);
        progress.setWindowTitle(title);
        progress.setWindowModality(Qt::WindowModal);
        progress.setMinimumDuration(0);
        std::map<std::filesystem::path, QList<std::filesystem::path>> processed;
        int count = 0;
        for (qsizetype i = 0; i < checked.size() && !progress.wasCanceled(); ++i) {
            const auto &file = checked.at(i);
            progress.setValue(int(i));
            progress.setLabelText(QDir::toNativeSeparators(textOf(file)));
            if (folder) {
                // A file outside the voice bank keeps only its name.
                auto relative = stdc::path::lexically_relative(file, root);
                if (relative.empty() || *relative.begin() == "..") {
                    relative = file.filename();
                }
                const auto target = *folder / relative;
                std::error_code error;
                std::filesystem::create_directories(target.parent_path(), error);
                if (error || !QFile::copy(textOf(file), textOf(target))) {
                    diagnostics.push_back({kit::Diagnostic::Error,
                                           tr("\"%1\" could not be backed up to \"%2\", and is "
                                              "not processed.")
                                               .arg(QDir::toNativeSeparators(textOf(file)),
                                                    QDir::toNativeSeparators(textOf(target))),
                                           std::nullopt});
                    continue;
                }
            }
            if (kit::AudioFileProcessor::process(file, options, diagnostics)) {
                processed[file.parent_path()].push_back(file.filename());
                ++count;
            }
        }
        progress.setValue(int(checked.size()));

        if (dialog.removesFrequencyMaps()) {
            for (const auto &[path, names] : processed) {
                for (const auto format : impl.editor->frequencyFormats().formats()) {
                    kit::FrequencyFormats::removeMaps(*format, path, names, diagnostics);
                }
            }
        }
        impl.readAudio.clear();
        impl.waveform->setAudio(nullptr);
        impl.model->refreshFrequencyMaps();
        impl.frequencyShown = {};
        impl.showCurrentEntry();
        statusBar()->showMessage(tr("%n audio file(s) were processed.", nullptr, count));
        DiagnosticBox::report(this, title, diagnostics);
        return count == checked.size();
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
        impl.table->scrollTo(index, QAbstractItemView::PositionAtCenter);
        // Scrolled again once the event loop runs: a window opened just now has no geometry
        // yet, and the change of folder may rebuild the rows. The row is found again by its
        // folder, file and alias.
        QTimer::singleShot(0, this,
                           [this, directory, fileName = sample->fileName, alias = sample->alias] {
                               stdc_impl_t;
                               const int row = impl.model->rowOf(directory, fileName, alias);
                               if (row < 0) {
                                   return;
                               }
                               const auto index = impl.proxy->mapFromSource(
                                   impl.model->index(row, VoiceBankEntryModel::FileColumn));
                               impl.table->scrollTo(index, QAbstractItemView::PositionAtCenter);
                           });
        raise();
        activateWindow();
        return true;
    }

    bool VoiceBankWindow::save() {
        stdc_impl_t;
        kit::DiagnosticList diagnostics;
        impl.info->commit();
        const bool saved = impl.document->save(diagnostics);
        DiagnosticBox::report(this, tr("Save"), diagnostics);
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
        VoiceBankDecodingDialog selector(this);
        selector.setRoot(impl.document->rootPath());
        kit::DiagnosticList diagnostics;
        impl.document->reloadAllFromDisk(&selector, diagnostics);
        DiagnosticBox::report(this, tr("Read All from Disk"), diagnostics);
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
            impl.editor->settings().files().addRecentVoiceBank(impl.document->rootPath());
        }
        DiagnosticBox::report(this, tr("Save As"), diagnostics);
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
