#include "MainWindow.h"

#include <QtCore/QDir>
#include <QtCore/QHash>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtGui/QActionGroup>
#include <QtGui/QClipboard>
#include <QtGui/QCloseEvent>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>

#include <stdcorelib/pimpl.h>

#include <QAKCore/actionextension.h>
#include <QAKCore/actionregistry.h>
#include <QAKWidgets/widgetactioncontext.h>

#include <hellokit/Edit/ProjectDocument.h>
#include <hellokit/Edit/ProjectEdits.h>
#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Edit/TrackTimeline.h>

#include <helloutau/Theme/ThemeManager.h>
#include <helloutau/Widgets/CommandPalette.h>
#include <helloutau/Widgets/PianoKeyboard.h>

#include "AppSettings.h"
#include "CommandEntries_p.h"
#include "DiagnosticBox_p.h"
#include "Editor.h"
#include "ExportUstDialog.h"
#include "PianoRoll.h"
#include "Playback.h"
#include "PasteParametersDialog.h"
#include "ScalePitchDialog.h"
#include "VibratoDialog.h"
#include "VoiceBankCharsetDialog.h"
#include "VoiceBankWindow.h"

namespace hello::daw {

    namespace {

        // How often the playhead follows playback, in milliseconds
        constexpr int PlayheadInterval = 30;

        // How long a message stays in the status bar, in milliseconds
        constexpr int StatusMessageTimeout = 8000;

        // A preview restarts from a moved playhead this many milliseconds later, once for the
        // moves of a drag within them; so does the rendering in the background.
        constexpr int PreviewRestartDelay = 50;

        // How often the status bar counts the notes rendered in the background, in milliseconds
        constexpr int RenderStatusInterval = 250;

        QString textOf(const std::filesystem::path &path) {
            return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
        }

        std::filesystem::path pathOf(const QString &text) {
            return std::filesystem::path(QDir::fromNativeSeparators(text).toStdU16String());
        }

        // The file a document is saved or exported as by default: its own file, or else the file
        // it came from, with \a extension, or else a new file in the documents directory.
        std::filesystem::path proposedPath(const kit::ProjectDocument &document,
                                           const char16_t *extension) {
            auto path = document.sourcePath();
            if (path.empty()) {
                path = pathOf(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
                path /= u"Untitled";
            }
            return path.replace_extension(extension);
        }

    }

    class MainWindow::Impl {
    public:
        using Decl = MainWindow;

        Impl(Decl *decl, Editor *editor) : _decl(decl), editor(editor) {
        }

        Decl *_decl;
        Editor *editor;
        std::unique_ptr<kit::ProjectDocument> document;
        PianoRoll *roll = nullptr;
        QAK::WidgetActionContext *context = nullptr;
        QHash<QString, QAction *> actions;
        QActionGroup *tools = nullptr;
        CommandPalette *palette = nullptr;
        QMenu *recentMenu = nullptr;
        // What Paste Parameters pasted last
        PianoRoll::Parameters pastedParameters = PianoRoll::AllParameters;

        Playback *playback = nullptr;
        QLabel *renderLabel = nullptr;
        QProgressBar *renderProgress = nullptr;
        QPushButton *renderCancel = nullptr;
        QTimer playheadTimer;
        // The notes still to render in the background, in the realtime mode
        QTimer statusTimer;
        // Whether the playback is a preview, and whether it restarts from the playhead soon
        bool previewing = false;
        bool restartPending = false;

        // The render progress in the status bar, and the playhead that follows playback
        void initPlayback() {
            stdc_decl_t;
            playback = new Playback(&decl);
            renderLabel = new QLabel();
            renderProgress = new QProgressBar();
            renderProgress->setMaximumWidth(200);
            renderProgress->setTextVisible(false);
            renderCancel = new QPushButton(tr("Cancel"));
            for (const auto widget :
                 std::initializer_list<QWidget *>{renderLabel, renderProgress, renderCancel}) {
                decl.statusBar()->addPermanentWidget(widget);
                widget->hide();
            }
            QObject::connect(renderCancel, &QPushButton::clicked, playback, &Playback::stop);

            QObject::connect(playback, &Playback::stateChanged, &decl,
                             [this](Playback::State state) {
                                 const bool rendering = state == Playback::Rendering;
                                 renderLabel->setVisible(rendering);
                                 renderProgress->setVisible(rendering);
                                 renderCancel->setVisible(rendering);
                                 if (rendering) {
                                     renderLabel->setText(tr("Rendering..."));
                                     renderProgress->setRange(0, 0);
                                 }
                                 if (state == Playback::Playing) {
                                     playheadTimer.start();
                                 } else {
                                     playheadTimer.stop();
                                     roll->setPlayheadPosition(std::nullopt);
                                 }
                                 if (state == Playback::Stopped) {
                                     previewing = false;
                                     reportPreviewFailures();
                                     // Later, since a preview stops before it restarts, and
                                     // a window stops as it closes.
                                     QMetaObject::invokeMethod(
                                         _decl,
                                         [this] {
                                             if (realtime() &&
                                                 playback->state() == Playback::Stopped) {
                                                 updateBackground();
                                             }
                                         },
                                         Qt::QueuedConnection);
                                 }
                             });
            QObject::connect(playback, &Playback::progressed, &decl, [this](int done, int total) {
                renderLabel->setText(tr("Rendering %1 of %2 notes").arg(done).arg(total));
                renderProgress->setRange(0, total);
                renderProgress->setValue(done);
            });
            QObject::connect(playback, &Playback::failed, &decl,
                             [this](const kit::DiagnosticList &diagnostics) {
                                 stdc_decl_t;
                                 DiagnosticBox::show(&decl, tr("Play"), diagnostics);
                             });

            playheadTimer.setInterval(PlayheadInterval);
            QObject::connect(&playheadTimer, &QTimer::timeout, &decl, [this] {
                if (const auto position = playback->position()) {
                    roll->setPlayheadPosition(roll->timeline()->tempoMap().tickOf(*position));
                }
                updatePreviewStatus();
            });
            statusTimer.setInterval(RenderStatusInterval);
            QObject::connect(&statusTimer, &QTimer::timeout, &decl,
                             [this] { updatePreviewStatus(); });
        }

        bool realtime() const {
            return editor->settings().playbackMode() == AppSettings::Realtime;
        }

        kit::SynthEngines engines() const {
            kit::SynthEngines engines;
            engines.resampler = pathOf(editor->settings().resampler());
            engines.wavtool = pathOf(editor->settings().wavtool());
            return engines;
        }

        // The time of the playhead at rest, in milliseconds
        double cursorTime() const {
            return roll->timeline()->tempoMap().timeOf(roll->cursorPosition());
        }

        // Plays in the playback mode of the settings (docs/Widgets.md), or stops what is playing
        // or rendering: renders the selected notes, from the first to the last, and plays
        // them; or plays from the playhead as the track is rendered.
        void togglePlayback() {
            stdc_decl_t;
            if (playback->state() != Playback::Stopped) {
                playback->stop();
                return;
            }
            if (realtime()) {
                startPreview();
                return;
            }
            const auto selected = roll->selectedIndices();
            if (selected.isEmpty()) {
                decl.statusBar()->showMessage(MainWindow::tr("Select the notes to render first."),
                                              StatusMessageTimeout);
                return;
            }
            kit::DiagnosticList diagnostics;
            if (!playback->play(*document, std::make_pair(selected.first(), selected.last()),
                                engines(), diagnostics)) {
                DiagnosticBox::show(&decl, tr("Play"), diagnostics);
            }
        }

        // Plays from the playhead at rest as the track is rendered.
        void startPreview() {
            stdc_decl_t;
            kit::DiagnosticList diagnostics;
            previewing = playback->preview(*document, cursorTime(), engines(), diagnostics);
            if (!previewing) {
                DiagnosticBox::show(&decl, tr("Play"), diagnostics);
            }
        }

        // In the realtime mode, renders the track in the background, from the playhead first;
        // in the prerender mode, nothing. What prevents rendering, such as a missing voice
        // bank, is reported once the user plays.
        void updateBackground() {
            if (!realtime()) {
                playback->release();
                statusTimer.stop();
                updatePreviewStatus();
                return;
            }
            kit::DiagnosticList diagnostics;
            playback->prepare(*document, cursorTime(), engines(), diagnostics);
            statusTimer.start();
        }

        // The playhead moved on the ruler: once a drag has settled for a moment, a preview goes
        // on from there, and otherwise the rendering in the background starts from there.
        void cursorMoved() {
            stdc_decl_t;
            if (restartPending) {
                return;
            }
            restartPending = true;
            QTimer::singleShot(PreviewRestartDelay, &decl, [this] {
                restartPending = false;
                if (previewing) {
                    startPreview();
                } else if (playback->state() == Playback::Stopped) {
                    updateBackground();
                }
            });
        }

        // The notes a preview could not render, which played as silence, in the status bar
        void reportPreviewFailures() {
            stdc_decl_t;
            const auto failed = playback->takePreviewDiagnostics();
            if (!failed.isEmpty()) {
                decl.statusBar()->showMessage(
                    MainWindow::tr("%n note(s) could not be rendered, and were silent.", nullptr,
                                   int(failed.size())),
                    StatusMessageTimeout);
            }
        }

        // The state of a preview in the status bar: the notes still to render, and whether
        // playback waits for them
        void updatePreviewStatus() {
            const int pending = playback->pendingNotes();
            const bool buffering = playback->isBuffering();
            renderLabel->setVisible(pending > 0 || buffering);
            if (buffering) {
                renderLabel->setText(
                    MainWindow::tr("Buffering, %n note(s) to render", nullptr, pending));
            } else if (pending > 0) {
                renderLabel->setText(MainWindow::tr("%n note(s) to render", nullptr, pending));
            }
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
            context->addMenuBar(QStringLiteral("helloutau.mainMenu"), decl.menuBar());
            initPlayback();

            addCommand(QStringLiteral("helloutau.file.new"), [this] { editor->newWindow(); });
            addCommand(QStringLiteral("helloutau.file.open"), [this] { open(); });
            addCommand(QStringLiteral("helloutau.file.openVoiceBank"), [this] {
                stdc_decl_t;
                const auto folder = QFileDialog::getExistingDirectory(&decl, tr("Open Voice Bank"));
                if (!folder.isEmpty()) {
                    editor->openVoiceBank(pathOf(folder), &decl);
                }
            });
            addCommand(QStringLiteral("helloutau.tools.editVoiceBank"),
                       [this] { editVoiceBank(); });
            addCommand(QStringLiteral("helloutau.tools.showEntry"), [this] { showEntry(); });
            // An external action: its menu is ours to fill, each time it opens.
            recentMenu = new QMenu(&decl);
            QObject::connect(recentMenu, &QMenu::aboutToShow, &decl, [this] { fillRecentMenu(); });
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
            addCommand(QStringLiteral("helloutau.file.exportUst"), [this] {
                stdc_decl_t;
                decl.exportUst();
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
            addCommand(QStringLiteral("helloutau.edit.delete"), [this] {
                edit(tr("Delete"), [this](kit::DiagnosticList &diagnostics) {
                    return roll->removeSelected(diagnostics);
                });
            });
            addCommand(QStringLiteral("helloutau.edit.selectAll"), [this] { roll->selectAll(); });
            addCommand(QStringLiteral("helloutau.edit.insertNote"), [this] {
                edit(tr("Insert Note"), [this](kit::DiagnosticList &diagnostics) {
                    return roll->insertNote(diagnostics);
                });
            });
            addCommand(QStringLiteral("helloutau.edit.splitNote"), [this] { splitNote(); });
            addCommand(QStringLiteral("helloutau.edit.togglePortamento"), [this] {
                edit(tr("Portamento"), [this](kit::DiagnosticList &diagnostics) {
                    return roll->togglePortamento(diagnostics);
                });
            });
            addCommand(QStringLiteral("helloutau.edit.toggleVibrato"), [this] {
                edit(tr("Vibrato"), [this](kit::DiagnosticList &diagnostics) {
                    return roll->toggleVibrato(diagnostics);
                });
            });
            addCommand(QStringLiteral("helloutau.edit.editVibrato"), [this] { editVibrato(); });
            const std::pair<const char *, PianoRoll::Crossfade> crossfades[] = {
                {"helloutau.edit.crossfadeP2P3", PianoRoll::CrossfadeP2P3},
                {"helloutau.edit.crossfadeP1P4", PianoRoll::CrossfadeP1P4},
            };
            for (const auto &[id, crossfade] : crossfades) {
                addCommand(QLatin1String(id), [this, crossfade = crossfade] {
                    edit(tr("Envelope"), [this, crossfade](kit::DiagnosticList &diagnostics) {
                        return roll->crossfadeEnvelopes(crossfade, diagnostics);
                    });
                });
            }
            addCommand(QStringLiteral("helloutau.edit.copy"), [this] {
                roll->copySelected();
                updateEditActions();
            });
            addCommand(QStringLiteral("helloutau.edit.paste"), [this] {
                edit(tr("Paste"), [this](kit::DiagnosticList &diagnostics) {
                    return roll->pasteNotes(diagnostics);
                });
            });
            addCommand(QStringLiteral("helloutau.edit.pasteParameters"),
                       [this] { pasteParameters(); });
            addCommand(QStringLiteral("helloutau.edit.scalePitch"), [this] { scalePitch(); });
            const std::pair<const char *, PianoRoll::Parameters> resets[] = {
                {"helloutau.edit.resetPortamento", PianoRoll::PortamentoParameter},
                {"helloutau.edit.resetVibratos",   PianoRoll::VibratoParameter   },
                {"helloutau.edit.resetEnvelopes",  PianoRoll::EnvelopeParameter  },
                {"helloutau.edit.resetAll",        PianoRoll::AllParameters      },
            };
            for (const auto &[id, parameters] : resets) {
                addCommand(QLatin1String(id), [this, parameters = parameters] {
                    edit(tr("Reset"), [this, parameters](kit::DiagnosticList &diagnostics) {
                        return roll->resetParameters(parameters, diagnostics);
                    });
                });
            }
            // Whether there is something to paste changes with the clipboard.
            QObject::connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, &decl,
                             [this] { updateEditActions(); });
            addCommand(QStringLiteral("helloutau.edit.editLyric"), [this] {
                const auto indices = roll->selectedIndices();
                if (!indices.isEmpty()) {
                    roll->editLyric(indices.first());
                }
            });
            const std::pair<const char *, int> transpositions[] = {
                {"helloutau.edit.transposeUp",   1  },
                {"helloutau.edit.transposeDown", -1 },
                {"helloutau.edit.octaveUp",      12 },
                {"helloutau.edit.octaveDown",    -12},
            };
            for (const auto &[id, semitones] : transpositions) {
                addCommand(QLatin1String(id), [this, semitones = semitones] {
                    edit(tr("Transpose"), [this, semitones](kit::DiagnosticList &diagnostics) {
                        return roll->transposeSelected(semitones, diagnostics);
                    });
                });
            }

            // Checked as the project has it; a click turns it over as an edit.
            const auto mode2 = addCommand(QStringLiteral("helloutau.edit.mode2"), [this] {
                const bool on = actions.value(QStringLiteral("helloutau.edit.mode2"))->isChecked();
                edit(tr("Mode2"), [this, on](kit::DiagnosticList &diagnostics) {
                    return kit::ProjectEdits::setMode2(
                        kit::ProjectRef(document->session()).settings(), on, diagnostics);
                });
                updatePitchActions();
            });
            mode2->setCheckable(true);

            tools = new QActionGroup(&decl);
            const auto selectTool = addCommand(QStringLiteral("helloutau.edit.selectTool"),
                                               [this] { roll->setTool(PianoRoll::SelectTool); });
            const auto penTool = addCommand(QStringLiteral("helloutau.edit.penTool"),
                                            [this] { roll->setTool(PianoRoll::PenTool); });
            const auto pitchTool = addCommand(QStringLiteral("helloutau.edit.pitchTool"),
                                              [this] { roll->setTool(PianoRoll::PitchTool); });
            for (const auto action : {selectTool, penTool, pitchTool}) {
                action->setCheckable(true);
                tools->addAction(action);
            }
            selectTool->setChecked(true);
            const auto showPitch = addCommand(QStringLiteral("helloutau.view.showPitch"), [this] {
                roll->setPitchVisible(
                    actions.value(QStringLiteral("helloutau.view.showPitch"))->isChecked());
                updatePitchActions();
            });
            showPitch->setCheckable(true);
            showPitch->setChecked(true);
            addCommand(QStringLiteral("helloutau.view.commandPalette"), [this] {
                palette->setCommands(commandEntries());
                palette->setRecentIds(editor->settings().recentCommands());
                palette->popup();
            });
            addCommand(QStringLiteral("helloutau.playback.play"), [this] { togglePlayback(); });
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

        QList<CommandEntry> commandEntries() const {
            return commandEntriesOf(editor->actionRegistry(), context,
                                    QStringLiteral("helloutau.view.commandPalette"));
        }

        void bindDocument() {
            stdc_decl_t;
            // Replaces the piano roll of the previous document, which is deleted with it. The
            // tool, the quantization and whether the pitch is shown belong to the window and
            // carry over.
            const auto quantization = roll ? roll->quantization() : -1;
            roll = new PianoRoll(document->session());
            roll->setVoiceBank(document->voiceBank());
            if (quantization >= 0) {
                roll->setQuantization(quantization);
            }
            if (actions.value(QStringLiteral("helloutau.edit.penTool"))->isChecked()) {
                roll->setTool(PianoRoll::PenTool);
            } else if (actions.value(QStringLiteral("helloutau.edit.pitchTool"))->isChecked()) {
                roll->setTool(PianoRoll::PitchTool);
            }
            roll->setPitchVisible(
                actions.value(QStringLiteral("helloutau.view.showPitch"))->isChecked());
            decl.setCentralWidget(roll);

            QObject::connect(document.get(), &kit::ProjectDocument::voiceBankChanged, roll, [this] {
                roll->setVoiceBank(document->voiceBank());
                updateBackground();
            });
            QObject::connect(roll, &PianoRoll::selectionChanged, &decl,
                             [this] { updateEditActions(); });
            // In the status bar, so that a refused drag does not stop the work with a dialog.
            // A dialog remains an alternative, see the open questions in docs/Tuning.md.
            QObject::connect(roll, &PianoRoll::cursorMoved, &decl, [this] { cursorMoved(); });
            QObject::connect(roll, &PianoRoll::editRefused, &decl, [this](const QString &message) {
                stdc_decl_t;
                decl.statusBar()->showMessage(message, StatusMessageTimeout);
            });
            updateEditActions();
            QObject::connect(document.get(), &kit::ProjectDocument::modifiedChanged, &decl,
                             [this] { updateTitle(); });
            QObject::connect(document.get(), &kit::ProjectDocument::filePathChanged, &decl, [this] {
                updateTitle();
                // The render cache is beside the file.
                playback->updatePlan(*document);
            });
            QObject::connect(document->session(), &kit::ProjectSession::stepChanged, &decl, [this] {
                updateUndoActions();
                updatePitchActions();
                // A preview plays, and the background renders, the notes as they now are.
                playback->updatePlan(*document);
            });
            updateTitle();
            updateUndoActions();
            updatePitchActions();
            updateBackground();
        }

        // Checks Mode2 as the project has it, and enables the pitch tool while it draws: with
        // the pitch shown and Mode2 off. Without it, the select tool takes its place.
        void updatePitchActions() {
            const bool mode2 = kit::ProjectRef(document->session()).settings().mode2();
            actions.value(QStringLiteral("helloutau.edit.mode2"))->setChecked(mode2);
            const auto pitchTool = actions.value(QStringLiteral("helloutau.edit.pitchTool"));
            pitchTool->setEnabled(
                !mode2 && actions.value(QStringLiteral("helloutau.view.showPitch"))->isChecked());
            if (!pitchTool->isEnabled() && pitchTool->isChecked()) {
                actions.value(QStringLiteral("helloutau.edit.selectTool"))->setChecked(true);
                roll->setTool(PianoRoll::SelectTool);
            }
        }

        void updateTitle() {
            stdc_decl_t;
            const auto name = document->displayName();
            decl.setWindowTitle(
                QStringLiteral("%1[*] - HelloUtau").arg(name.isEmpty() ? tr("Untitled") : name));
            decl.setWindowModified(document->isModified());
        }

        // Enables the commands that act on the selection when there is one.
        void updateEditActions() {
            const int selected = int(roll->selectedIndices().size());
            for (const auto id : {"helloutau.edit.delete", "helloutau.edit.editLyric",
                                  "helloutau.edit.togglePortamento", "helloutau.edit.toggleVibrato",
                                  "helloutau.edit.editVibrato", "helloutau.edit.scalePitch",
                                  "helloutau.edit.crossfadeP2P3", "helloutau.edit.crossfadeP1P4",
                                  "helloutau.edit.copy", "helloutau.edit.transposeUp",
                                  "helloutau.edit.transposeDown", "helloutau.edit.octaveUp",
                                  "helloutau.edit.octaveDown", "helloutau.tools.showEntry"}) {
                actions.value(QLatin1String(id))->setEnabled(selected > 0);
            }
            actions.value(QStringLiteral("helloutau.edit.splitNote"))->setEnabled(selected == 1);
            const bool copied = !PianoRoll::copiedNotes().isEmpty();
            actions.value(QStringLiteral("helloutau.edit.paste"))->setEnabled(copied);
            actions.value(QStringLiteral("helloutau.edit.pasteParameters"))
                ->setEnabled(selected > 0 && copied);
            // Delete also removes the selected pitch points.
            if (!roll->selectedPoints().isEmpty()) {
                actions.value(QStringLiteral("helloutau.edit.delete"))->setEnabled(true);
            }
        }

        // Performs an edit of the piano roll and shows why it was refused, if it was. Nothing
        // is edited while a lyric is, since the editor has the keyboard.
        void edit(const QString &title, const std::function<bool(kit::DiagnosticList &)> &run) {
            stdc_decl_t;
            if (roll->lyricEditor()->isVisible()) {
                return;
            }
            kit::DiagnosticList diagnostics;
            run(diagnostics);
            DiagnosticBox::show(&decl, title, diagnostics);
        }

        // Pastes the parameters of the copied notes that the user chooses, those chosen last
        // time at first
        void pasteParameters() {
            stdc_decl_t;
            if (roll->lyricEditor()->isVisible()) {
                return;
            }
            PasteParametersDialog dialog(pastedParameters, &decl);
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }
            pastedParameters = dialog.parameters();
            kit::DiagnosticList diagnostics;
            roll->pasteParameters(pastedParameters, diagnostics);
            DiagnosticBox::show(&decl, tr("Paste Parameters"), diagnostics);
        }

        // Scales the pitch of the selected sung notes by the factors that the user enters.
        void scalePitch() {
            stdc_decl_t;
            if (roll->lyricEditor()->isVisible()) {
                return;
            }
            ScalePitchDialog dialog(&decl);
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }
            kit::DiagnosticList diagnostics;
            roll->scalePitch(dialog.portamento(), dialog.vibrato(), diagnostics);
            DiagnosticBox::show(&decl, tr("Scale Pitch"), diagnostics);
        }

        // Sets the vibrato of the selected sung notes to one that the user enters, starting from
        // that of the first of them, or the default.
        void editVibrato() {
            stdc_decl_t;
            if (roll->lyricEditor()->isVisible()) {
                return;
            }
            const auto refs = kit::ProjectRef(document->session()).tracks().at(0).notes();
            QList<kit::NoteRef> sung;
            for (const int index : roll->selectedIndices()) {
                if (!roll->timeline()->note(index).rest) {
                    sung.push_back(refs.at(index));
                }
            }
            if (sung.isEmpty()) {
                return;
            }
            VibratoDialog dialog(sung.first().vibrato().value_or(VibratoDialog::defaultVibrato()),
                                 &decl);
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }
            kit::DiagnosticList diagnostics;
            kit::ProjectEdits::setVibrato(sung, dialog.vibrato(), diagnostics);
            DiagnosticBox::show(&decl, tr("Vibrato"), diagnostics);
        }

        // Splits the selected note after a length that the user enters.
        void splitNote() {
            stdc_decl_t;
            const auto indices = roll->selectedIndices();
            if (indices.size() != 1) {
                return;
            }
            const int index = indices.first();
            const int length = roll->timeline()->note(index).length;
            if (length < 2) {
                QMessageBox::information(&decl, tr("Split Note"),
                                         tr("A note of one tick cannot be split."));
                return;
            }
            // Half the note, on the grid if there is one
            const int step = roll->quantization();
            int proposed = length / 2;
            if (step > 0 && step < length) {
                proposed = std::clamp((proposed + step / 2) / step * step, step, length - 1);
            }
            bool ok = false;
            const int ticks =
                QInputDialog::getInt(&decl, tr("Split Note"),
                                     tr("Length of the first part in ticks, of %1:").arg(length),
                                     proposed, 1, length - 1, 1, &ok);
            if (!ok) {
                return;
            }
            edit(tr("Split Note"), [&](kit::DiagnosticList &diagnostics) {
                return kit::ProjectEdits::splitNote(
                    kit::ProjectRef(document->session()).tracks().at(0).notes(), index, ticks,
                    diagnostics);
            });
        }

        void updateUndoActions() {
            const auto session = document->session();
            actions.value(QStringLiteral("helloutau.edit.undo"))->setEnabled(session->canUndo());
            actions.value(QStringLiteral("helloutau.edit.redo"))->setEnabled(session->canRedo());
        }

        void open() {
            stdc_decl_t;
            const auto file = QFileDialog::getOpenFileName(
                &decl, tr("Open"), {},
                tr("Projects (*.usth *.ust);;HelloUtau projects (*.usth);;UTAU projects "
                   "(*.ust);;All files (*)"));
            if (!file.isEmpty()) {
                editor->openFile(pathOf(file), &decl);
            }
        }

        // The files last opened, numbered, the latest first, and a command that forgets them.
        // A file that is gone is reported and forgotten when chosen.
        // Opens the voice bank of the project in its window, as the UTAU folder of the settings
        // resolves it, or returns null after telling the user why not.
        VoiceBankWindow *editVoiceBank() {
            stdc_decl_t;
            const auto track = document->session()->snapshot().tracks.value(0);
            const auto root = track.voiceDirectory(editor->settings().utauDirectory());
            if (track.voiceDir.isEmpty() || root.empty()) {
                QMessageBox::information(
                    &decl, tr("Edit Voice Bank"),
                    track.voiceDir.isEmpty()
                        ? tr("The project names no voice bank.")
                        : tr("The voice bank \"%1\" is in the UTAU folder, which is not set in the "
                             "settings.")
                              .arg(track.voiceDir));
                return nullptr;
            }
            return editor->openVoiceBank(root, &decl);
        }

        // Shows the entry that the first selected note uses in the window of the voice bank.
        void showEntry() {
            stdc_decl_t;
            const auto indices = roll->selectedIndices();
            if (indices.isEmpty()) {
                return;
            }
            const auto &note = roll->timeline()->note(indices.first());
            if (note.rest) {
                decl.statusBar()->showMessage(tr("A rest has no entry."), StatusMessageTimeout);
                return;
            }
            const auto window = editVoiceBank();
            if (window && !window->showEntryFor(note.key, note.lyric)) {
                decl.statusBar()->showMessage(
                    tr("The voice bank has no entry for \"%1\" at %2.")
                        .arg(note.lyric, PianoKeyboard::keyName(note.key)),
                    StatusMessageTimeout);
            }
        }

        void fillRecentMenu() {
            stdc_decl_t;
            editor->fillRecentMenu(recentMenu, &decl);
        }

        // Asks whether to save a modified project before it is closed. Returns whether closing
        // may proceed.
        bool maybeSave() {
            stdc_decl_t;
            if (!document->isModified()) {
                return true;
            }
            const auto name = document->displayName();
            const auto answer = QMessageBox::warning(
                &decl, tr("HelloUtau"),
                tr("Save the changes to %1?").arg(name.isEmpty() ? tr("Untitled") : name),
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
            if (answer == QMessageBox::Save) {
                return decl.save();
            }
            return answer == QMessageBox::Discard;
        }

        static QString tr(const char *text) {
            return MainWindow::tr(text);
        }
    };

    MainWindow::MainWindow(Editor *editor, std::unique_ptr<kit::ProjectDocument> document)
        : _impl(std::make_unique<Impl>(this, editor)) {
        stdc_impl_t;
        impl.initActions();
        impl.document = std::move(document);
        impl.bindDocument();
        editor->themeManager()->install(this, {QStringLiteral("MainWindow")});
        resize(960, 640);
    }

    MainWindow::~MainWindow() {
        stdc_impl_t;
        // Stopping updates the piano roll, which refers to the session of the document, which
        // goes with _impl.
        impl.playback->stop();
        delete impl.roll;
    }

    kit::ProjectDocument *MainWindow::document() const {
        stdc_impl_t;
        return impl.document.get();
    }

    void MainWindow::setDocument(std::unique_ptr<kit::ProjectDocument> document) {
        stdc_impl_t;
        impl.playback->stop();
        auto previous = std::move(impl.document);
        impl.document = std::move(document);
        impl.bindDocument();
    }

    void MainWindow::applySettings() {
        stdc_impl_t;
        // What plays in the other mode stops.
        const auto state = impl.playback->state();
        if (impl.realtime() ? state == Playback::Rendering : impl.previewing) {
            impl.playback->stop();
        }
        impl.updateBackground();
    }

    bool MainWindow::isUnused() const {
        stdc_impl_t;
        return impl.document->sourcePath().empty() && !impl.document->isModified();
    }

    bool MainWindow::loadVoiceBank() {
        stdc_impl_t;
        const auto document = impl.document.get();
        const auto utau = impl.editor->settings().utauDirectory();
        VoiceBankCharsetDialog selector(this);
        selector.setRoot(document->session()->snapshot().tracks.value(0).voiceDirectory(utau));
        kit::DiagnosticList diagnostics;
        const bool loaded = document->loadVoiceBank(utau, &selector, diagnostics);
        DiagnosticBox::show(this, tr("Voice Bank"), diagnostics);
        return loaded;
    }

    bool MainWindow::save() {
        stdc_impl_t;
        if (impl.document->filePath().empty()) {
            return saveAs();
        }
        kit::DiagnosticList diagnostics;
        const bool saved = impl.document->save(diagnostics);
        DiagnosticBox::show(this, tr("Save"), diagnostics);
        return saved;
    }

    bool MainWindow::saveAs() {
        stdc_impl_t;
        const auto file = QFileDialog::getSaveFileName(
            this, tr("Save As"), textOf(proposedPath(*impl.document, u".usth")),
            tr("HelloUtau projects (*.usth)"));
        if (file.isEmpty()) {
            return false;
        }
        auto path = pathOf(file);
        if (path.extension() != u".usth") {
            path += u".usth";
        }
        kit::DiagnosticList diagnostics;
        const bool saved = impl.document->saveAs(path, diagnostics);
        DiagnosticBox::show(this, tr("Save As"), diagnostics);
        if (saved) {
            impl.editor->settings().addRecentFile(path);
        }
        return saved;
    }

    bool MainWindow::exportUst() {
        stdc_impl_t;
        const auto &settings = impl.editor->settings();
        ExportUstDialog dialog(proposedPath(*impl.document, u".ust"), settings.ustExportCharset(),
                               this);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }
        const auto path = dialog.path();
        std::error_code error;
        if (std::filesystem::exists(path, error) &&
            QMessageBox::question(this, tr("Export UST"),
                                  tr("%1 already exists. Replace it?").arg(textOf(path))) !=
                QMessageBox::Yes) {
            return false;
        }

        kit::UstDocument::ExportOptions options;
        options.charset = dialog.charset();
        options.wavtool = settings.wavtool();
        options.resampler = settings.resampler();
        kit::DiagnosticList diagnostics;
        const bool exported = impl.document->exportUst(path, options, diagnostics);
        DiagnosticBox::show(this, tr("Export UST"), diagnostics);
        return exported;
    }

    void MainWindow::closeEvent(QCloseEvent *event) {
        stdc_impl_t;
        if (!impl.maybeSave()) {
            event->ignore();
            return;
        }
        QMainWindow::closeEvent(event);
    }

}
