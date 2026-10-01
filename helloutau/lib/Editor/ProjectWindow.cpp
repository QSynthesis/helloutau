#include "ProjectWindow.h"

#include <QtCore/QDir>
#include <QtCore/QHash>
#include <QtCore/QMetaObject>
#include <QtCore/QMimeData>
#include <QtCore/QSignalBlocker>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtGui/QActionGroup>
#include <QtGui/QClipboard>
#include <QtGui/QCloseEvent>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>

#include <stdcorelib/pimpl.h>

#include <QAKCore/actionextension.h>
#include <QAKCore/actionregistry.h>
#include <QAKWidgets/widgetactioncontext.h>

#include <hellokit/Edit/ProjectDocument.h>
#include <hellokit/Edit/ProjectEdits.h>
#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Edit/TrackTimeline.h>
#include <hellokit/Synth/ClassicSynthRunner.h>
#include <hellokit/Synth/ThreadedSynthRunner.h>

#include <helloutau/Theme/ThemeManager.h>
#include <helloutau/Widgets/CommandPalette.h>
#include <helloutau/Widgets/FindBar.h>
#include <helloutau/Widgets/PianoKeyboard.h>
#include <helloutau/Widgets/SceneView.h>

#include "ActionRegistrations_p.h"
#include "AppSettings.h"
#include "CommandEntries_p.h"
#include "DiagnosticBox_p.h"
#include "Editor.h"
#include "ExportUstDialog.h"
#include "FindSupport_p.h"
#include "PianoRoll.h"
#include "NotePropertiesDialog.h"
#include "ProjectPropertiesDialog.h"
#include "Playback.h"
#include "PasteParametersDialog.h"
#include "ScalePitchDialog.h"
#include "ToolBarPalette_p.h"
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

        // The render states on the ruler follow a change this many milliseconds later, once for
        // the changes within them, and a script that renders as often.
        constexpr int RenderStateDelay = 300;

        PianoRoll::RenderState renderStateOf(kit::RealtimeSynth::NoteState state) {
            switch (state) {
                case kit::RealtimeSynth::Waiting:
                    return PianoRoll::RenderWaiting;
                case kit::RealtimeSynth::Running:
                    return PianoRoll::RenderRunning;
                case kit::RealtimeSynth::Ready:
                    return PianoRoll::RenderReady;
                case kit::RealtimeSynth::Failed:
                    return PianoRoll::RenderFailed;
                default:
                    return PianoRoll::RenderSilent;
            }
        }

        QString textOf(const std::filesystem::path &path) {
            return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
        }

        std::filesystem::path pathOf(const QString &text) {
            return std::filesystem::path(QDir::fromNativeSeparators(text).toStdU16String());
        }

        // The local files of a drag, in the order dragged
        QList<std::filesystem::path> localFilesOf(const QMimeData *data) {
            QList<std::filesystem::path> files;
            for (const auto &url : data->urls()) {
                if (url.isLocalFile()) {
                    files.push_back(pathOf(url.toLocalFile()));
                }
            }
            return files;
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

    class ProjectWindow::Impl {
    public:
        using Decl = ProjectWindow;

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
        FindBar *findBar = nullptr;
        QToolBar *toolBar = nullptr;
        // The boxes of the quantization in the tool bars, which follow the piano roll
        QList<QPointer<QComboBox>> quantizationBoxes;
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
        int lastPending = -1;
        // The notes that sound, from the last render states, as the total of the background
        // render in the status bar
        int soundingNotes = 0;
        // The render states on the ruler, updated after a change
        QTimer renderStateTimer;
        // Whether the playback is a preview, and whether it restarts from the playhead soon
        bool previewing = false;
        // The voice bank root used by the last load attempt. A project undo can restore the
        // recorded path without changing the loaded voice bank, so the two paths are tracked
        // separately.
        std::filesystem::path voiceBankRoot;
        bool voiceBankReloadPending = false;
        // The kind of the runner of the playback, and its thread count, see updateRunner()
        std::optional<bool> runnerClassic;
        int runnerThreads = 0;
        // The notes last rendered, which Replay renders again
        std::optional<std::pair<int, int>> lastRange;
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
                                 scheduleRenderStates();
                                 updateSaveLastPlayed();
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
                                     // Paused, the line stays where playback was.
                                     const auto at = playback->position();
                                     roll->setPlayheadPosition(
                                         state == Playback::Paused && at
                                             ? std::optional(
                                                   roll->timeline()->tempoMap().tickOf(*at))
                                             : std::nullopt);
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
                renderLabel->setText(tr("Rendering..."));
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
            QObject::connect(&statusTimer, &QTimer::timeout, &decl, [this] {
                updatePreviewStatus();
                // While notes are rendered, and once more when they are
                const int pending = playback->pendingNotes();
                if (pending > 0 || pending != lastPending) {
                    updateRenderStates();
                }
                lastPending = pending;
            });
            renderStateTimer.setSingleShot(true);
            renderStateTimer.setInterval(RenderStateDelay);
            QObject::connect(playback, &Playback::noteStatesChanged, &decl,
                             [this] { updateRenderStates(); });
            // The status of a plan made in the background, in notes as UTAU counts them
            QObject::connect(playback, &Playback::planProgressed, &decl,
                             [this](int done, int total) {
                                 if (playback->state() == Playback::Rendering) {
                                     renderLabel->setText(preparingText(done, total));
                                     renderProgress->setRange(0, total);
                                     renderProgress->setValue(done);
                                 } else {
                                     updatePreviewStatus();
                                 }
                             });
            QObject::connect(&renderStateTimer, &QTimer::timeout, &decl, [this] {
                playback->refreshNoteStates(*document);
                if (playback->state() == Playback::Rendering) {
                    scheduleRenderStates();
                }
            });
        }

        // The tempo of note index in its dialog: a value, or following the tempo before
        void editTempo(int index) {
            stdc_decl_t;
            const auto notes = kit::ProjectRef(document->session()).tracks().at(0).notes();
            if (index < 0 || index >= notes.size()) {
                return;
            }
            TempoDialog dialog(notes.at(index).tempo(), roll->timeline()->tempoMap().tempo(index),
                               &decl);
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }
            kit::NotePropertyChanges changes;
            changes.tempo = dialog.tempo();
            kit::DiagnosticList diagnostics;
            kit::ProjectEdits::setNoteProperties({notes.at(index)}, changes, diagnostics);
            DiagnosticBox::show(&decl, tr("Tempo"), diagnostics);
        }

        // The properties of the selected notes in their dialog, changed in one step
        void editNoteProperties() {
            stdc_decl_t;
            const auto indices = roll->selectedIndices();
            if (indices.isEmpty()) {
                return;
            }
            const auto project = document->session()->snapshot();
            const auto refs = kit::ProjectRef(document->session()).tracks().at(0).notes();
            QList<kit::Note> notes;
            QList<kit::NoteRef> selected;
            for (const int index : indices) {
                notes.push_back(project.tracks[0].notes[index]);
                selected.push_back(refs.at(index));
            }
            NotePropertiesDialog dialog(notes, &decl);
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }
            kit::DiagnosticList diagnostics;
            kit::ProjectEdits::setNoteProperties(selected, dialog.changes(), diagnostics);
            DiagnosticBox::show(&decl, tr("Note Properties"), diagnostics);
        }

        // The properties of the project in their dialog, changed in one step; a new voice
        // folder is read at once.
        void editProperties() {
            stdc_decl_t;
            ProjectPropertiesDialog dialog(document->session()->snapshot(), &decl);
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }
            const auto changes = dialog.changes();
            bool newFolder = false;
            if (!changes.isEmpty()) {
                kit::DiagnosticList diagnostics;
                const bool changed = kit::ProjectEdits::setProperties(
                    kit::ProjectRef(document->session()), changes, diagnostics);
                DiagnosticBox::show(&decl, tr("Project Properties"), diagnostics);
                newFolder = changed && changes.voiceDir.has_value();
            }
            // A folder that failed to load before is read again once the dialog confirms it.
            const auto root = voiceRoot();
            if (newFolder || (!document->voiceBank() && !root.empty() && root == voiceBankRoot)) {
                decl.loadVoiceBank();
            }
        }

        // Returns the voice bank folder that the project names, resolved against the UTAU
        // folder of the settings. Reads the field alone rather than a snapshot of the project.
        std::filesystem::path voiceRoot() const {
            const auto tracks = kit::ProjectRef(document->session()).tracks();
            if (tracks.size() == 0) {
                return {};
            }
            kit::Track track;
            track.voiceDir = tracks.at(0).voiceDir();
            return track.voiceDirectory(editor->settings().utauDirectory());
        }

        // Deletes the render cache of the project; the realtime mode renders it anew.
        void clearCache() {
            stdc_decl_t;
            kit::DiagnosticList diagnostics;
            const auto deleted = playback->clearCache(*document, diagnostics);
            DiagnosticBox::show(&decl, tr("Clear Render Cache"), diagnostics);
            if (!deleted) {
                return;
            }
            decl.statusBar()->showMessage(
                ProjectWindow::tr("%n file(s) deleted from the render cache.", nullptr, *deleted),
                StatusMessageTimeout);
            updateBackground();
        }

        // How far each note is rendered, on the ruler: as the background renders them, or by
        // the fragments in the cache (the render states in docs/Widgets.md)
        // The status of a plan made in the background, which counts notes as UTAU does while
        // it renders
        static QString preparingText(int done, int total) {
            return ProjectWindow::tr("Preparing notes (%1/%2)").arg(done).arg(total);
        }

        void updateRenderStates() {
            QList<PianoRoll::RenderState> states;
            soundingNotes = 0;
            for (const auto state : playback->noteStates()) {
                states.push_back(renderStateOf(state));
                if (state != kit::RealtimeSynth::Silent) {
                    ++soundingNotes;
                }
            }
            roll->setRenderStates(states);
        }

        void scheduleRenderStates() {
            renderStateTimer.start();
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
        //
        // In the prerender mode, as in UTAU, the key stops what plays, and at rest it renders
        // the current selection again. A paused render counts as at rest. The realtime mode
        // pauses and resumes instead.
        void togglePlayback() {
            stdc_decl_t;
            switch (playback->state()) {
                case Playback::Rendering:
                    playback->stop();
                    return;
                case Playback::Playing:
                    if (realtime()) {
                        playback->pause();
                    } else {
                        playback->stop();
                    }
                    return;
                case Playback::Paused:
                    if (realtime()) {
                        resumePlayback();
                        return;
                    }
                    playback->stop();
                    break;
                default:
                    break;
            }
            if (realtime()) {
                startPreview();
                return;
            }
            const auto selected = roll->selectedIndices();
            if (selected.isEmpty()) {
                decl.statusBar()->showMessage(ProjectWindow::tr("Select the notes to render first."),
                                              StatusMessageTimeout);
                return;
            }
            playRange(std::make_pair(selected.first(), selected.last()));
        }

        // Renders notes range and plays them, or plays the last render again if they sound the
        // same.
        void playRange(std::pair<int, int> range) {
            stdc_decl_t;
            lastRange = range;
            kit::DiagnosticList diagnostics;
            if (!playback->play(*document, range, engines(), diagnostics)) {
                DiagnosticBox::show(&decl, tr("Play"), diagnostics);
                return;
            }
            waitForRender(tr("Play"));
        }

        // Shows a modal dialog while a prerender runs, as UTAU does while its script runs, so
        // that the project is not edited meanwhile. Cancel stops the render. The dialog closes
        // once the render ends and playback starts, or fails.
        void waitForRender(const QString &title) {
            stdc_decl_t;
            if (playback->state() != Playback::Rendering) {
                return;
            }
            QProgressDialog dialog(tr("Rendering..."), tr("Cancel"), 0, 0, &decl);
            dialog.setWindowTitle(title);
            dialog.setWindowModality(Qt::WindowModal);
            dialog.setMinimumDuration(0);
            dialog.setAutoClose(false);
            dialog.setAutoReset(false);
            QObject::connect(playback, &Playback::planProgressed, &dialog,
                             [&dialog](int done, int total) {
                                 dialog.setLabelText(preparingText(done, total));
                                 dialog.setMaximum(total);
                                 dialog.setValue(done);
                             });
            QObject::connect(playback, &Playback::progressed, &dialog,
                             [&dialog](int done, int total) {
                                 dialog.setLabelText(tr("Rendering..."));
                                 dialog.setMaximum(total);
                                 dialog.setValue(done);
                             });
            QObject::connect(playback, &Playback::stateChanged, &dialog,
                             [&dialog](Playback::State state) {
                                 if (state != Playback::Rendering) {
                                     dialog.accept();
                                 }
                             });
            QObject::connect(&dialog, &QProgressDialog::canceled, playback, &Playback::stop);
            dialog.exec();
        }

        // Saves a copy of the track file of the last prerender, the temp.wav of UTAU, where the
        // user chooses. The output file of the project is not involved.
        void saveLastPlayed() {
            stdc_decl_t;
            const auto source = playback->lastRenderFile();
            std::error_code error;
            if (source.empty() || !std::filesystem::is_regular_file(source, error)) {
                QMessageBox::information(&decl, tr("Save Last Played"),
                                         tr("Nothing has been rendered since the render cache "
                                            "was last cleared."));
                return;
            }
            auto proposed = document->sourcePath();
            proposed = proposed.empty() ? std::filesystem::path(u"untitled.wav")
                                        : proposed.replace_extension(u".wav");
            const auto chosen = QFileDialog::getSaveFileName(
                &decl, tr("Save Last Played"), QString::fromStdU16String(proposed.u16string()),
                tr("WAV files (*.wav)"));
            if (chosen.isEmpty()) {
                return;
            }
            const auto target = std::filesystem::path(chosen.toStdU16String());
            // The file dialog has confirmed the replacement.
            std::filesystem::copy_file(source, target,
                                       std::filesystem::copy_options::overwrite_existing, error);
            if (error) {
                QMessageBox::critical(
                    &decl, tr("Save Last Played"),
                    tr("%1 could not be written.").arg(QDir::toNativeSeparators(chosen)));
            }
        }

        // The runner of the playback mode: temp.bat in a console for the classic prerender, and
        // several threads otherwise, which also render a whole track in the realtime mode. The
        // runner is replaced only when the mode or the thread count changed, because a new
        // runner discards the kept render.
        void updateRunner() {
            const auto &settings = editor->settings();
            const bool classic = settings.playbackMode() == AppSettings::Prerender;
            const int threads = settings.renderThreadCount();
            playback->setThreadCount(threads);
            if (runnerClassic == classic && (classic || runnerThreads == threads)) {
                return;
            }
            if (classic) {
                playback->setRunner(std::make_shared<kit::ClassicSynthRunner>());
            } else {
                auto runner = std::make_shared<kit::ThreadedSynthRunner>();
                runner->threadCount = threads;
                playback->setRunner(runner);
            }
            runnerClassic = classic;
            runnerThreads = threads;
        }

        // Renders the whole track into a WAV file that the user chooses, by default the output
        // file of the project, as File > Render WAV of UTAU. The render of the last playback
        // stays as it is. A message box reports the written file once the progress dialog has
        // closed.
        void renderTrack() {
            stdc_decl_t;
            playback->stop();
            const auto file = QFileDialog::getSaveFileName(
                &decl, tr("Render Track"),
                QString::fromStdU16String(defaultTrackFile().u16string()), tr("WAV files (*.wav)"));
            if (file.isEmpty()) {
                return;
            }
            std::optional<std::filesystem::path> written;
            const auto connection =
                QObject::connect(playback, &Playback::trackRendered, &decl,
                                 [&written](const std::filesystem::path &path) { written = path; });
            kit::DiagnosticList diagnostics;
            const bool started = playback->renderTrack(
                *document, std::filesystem::path(file.toStdU16String()), engines(), diagnostics);
            if (started) {
                waitForRender(tr("Render Track"));
            }
            QObject::disconnect(connection);
            if (!started) {
                DiagnosticBox::show(&decl, tr("Render Track"), diagnostics);
                return;
            }
            if (written) {
                const auto path = QString::fromStdU16String(written->u16string());
                QMessageBox::information(
                    &decl, tr("Render Track"),
                    tr("The track has been saved to %1.").arg(QDir::toNativeSeparators(path)));
            }
        }

        // The output file of the project, resolved against the folder of the project file, or
        // against the music folder of the user for a project without a file. A project without
        // an output file proposes its name with the extension .wav.
        std::filesystem::path defaultTrackFile() const {
            const auto source = document->sourcePath();
            const auto folder =
                source.empty() ? std::filesystem::path(
                                     QStandardPaths::writableLocation(QStandardPaths::MusicLocation)
                                         .toStdU16String())
                               : source.parent_path();
            auto output = std::filesystem::path(
                QDir::fromNativeSeparators(
                    kit::ProjectRef(document->session()).settings().outputFile())
                    .toStdU16String());
            if (output.empty()) {
                output = source.empty() ? std::filesystem::path(u"untitled.wav")
                                        : source.filename().replace_extension(u".wav");
            }
            return output.is_absolute() ? output : folder / output;
        }

        // Save Last Played applies to the prerender mode, after a render.
        void updateSaveLastPlayed() {
            actions.value(QStringLiteral("helloutau.playback.saveLastPlayed"))
                ->setEnabled(!realtime() && playback->state() != Playback::Rendering &&
                             !playback->lastRenderFile().empty());
        }

        // Pauses what plays, or goes on with what was paused.
        void pauseOrResume() {
            if (playback->state() == Playback::Playing) {
                playback->pause();
            } else if (playback->state() == Playback::Paused) {
                resumePlayback();
            }
        }

        // A paused render goes on from where it was, a paused preview previews from there.
        void resumePlayback() {
            stdc_decl_t;
            if (!playback->isPreviewPaused()) {
                playback->resume();
                return;
            }
            const auto at = playback->position();
            kit::DiagnosticList diagnostics;
            previewing = playback->preview(*document, at, engines(), diagnostics);
            if (!previewing) {
                DiagnosticBox::show(&decl, tr("Play"), diagnostics);
            }
        }

        // Plays the last playback again from its start: from the playhead at rest in the
        // realtime mode, the notes last rendered in the prerender mode.
        void replay() {
            playback->stop();
            if (realtime()) {
                startPreview();
            } else if (lastRange) {
                playRange(*lastRange);
            } else {
                togglePlayback();
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
        // in the prerender mode, nothing, and the playhead shows only where playback is. What
        // prevents rendering, such as a missing voice bank, is reported once the user plays.
        void updateBackground() {
            updateRunner();
            roll->setCursorEnabled(realtime());
            scheduleRenderStates();
            updateSaveLastPlayed();
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
                if (playback->state() == Playback::Paused) {
                    // Resumed from the playhead instead of where playback was
                    playback->stop();
                    updateBackground();
                } else if (previewing) {
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
                    ProjectWindow::tr("%n note(s) could not be rendered, and were silent.", nullptr,
                                   int(failed.size())),
                    StatusMessageTimeout);
            }
        }

        // The state of a preview in the status bar: the plan while it is made, the notes still
        // to render, and whether playback waits for them
        void updatePreviewStatus() {
            const auto planning = playback->planProgress();
            const int pending = playback->pendingNotes();
            const bool buffering = playback->isBuffering();
            // The status of a render, and of a preview that waits for its plan, follows the
            // state of the playback.
            if (playback->state() == Playback::Rendering) {
                return;
            }
            renderLabel->setVisible(planning || pending > 0 || buffering);
            renderProgress->setVisible(planning || pending > 0);
            if (planning) {
                renderLabel->setText(preparingText(planning->first, planning->second));
                renderProgress->setRange(0, planning->second);
                renderProgress->setValue(planning->first);
                return;
            }
            const int total = std::max(soundingNotes, pending);
            renderProgress->setRange(0, total);
            renderProgress->setValue(total - pending);
            if (buffering) {
                renderLabel->setText(
                    ProjectWindow::tr("Buffering, %n note(s) to render", nullptr, pending));
            } else if (pending > 0) {
                renderLabel->setText(ProjectWindow::tr("%n note(s) to render", nullptr, pending));
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

        // Creates the tool bar, which QActionKit fills from the layout of helloutau.mainToolBar,
        // with the box of the quantization that each tool bar creates for itself.
        void initToolBar() {
            stdc_decl_t;
            toolBar = new QToolBar(tr("Main Toolbar"));
            toolBar->setObjectName(QStringLiteral("mainToolBar"));
            toolBar->setMovable(false);
            followToolBarPalette(toolBar);
            toolBar->setVisible(editor->settings().isToolBarVisible());
            decl.addToolBar(toolBar);
            context->addToolBar(QStringLiteral("helloutau.mainToolBar"), toolBar);
            const auto id = QStringLiteral("helloutau.edit.quantization");
            context->addWidgetFactory(id, [this](QWidget *parent) -> QWidget * {
                    auto box = new QComboBox(parent);
                    box->setObjectName(QStringLiteral("quantization"));
                    box->setToolTip(tr("Quantization"));
                    // Wider than its longest choice, which looks cramped in the tool bar
                    box->setMinimumContentsLength(8);
                    box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
                    for (const int ticks : PianoRoll::quantizations()) {
                        box->addItem(PianoRoll::quantizationName(ticks), ticks);
                    }
                    if (roll) {
                        box->setCurrentIndex(box->findData(roll->quantization()));
                    }
                    QObject::connect(box, &QComboBox::currentIndexChanged, box, [this, box] {
                        if (roll) {
                            roll->setQuantization(box->currentData().toInt());
                        }
                    });
                    quantizationBoxes.push_back(box);
                    return box;
                });
        }

        // Shows ticks in the boxes of the quantization, as the piano roll has it.
        void showQuantization(int ticks) {
            quantizationBoxes.removeAll(nullptr);
            for (const auto &box : std::as_const(quantizationBoxes)) {
                const QSignalBlocker blocker(box.data());
                box->setCurrentIndex(box->findData(ticks));
            }
        }

        // Selects the next finer quantization if finer is true, else the next coarser one. Off
        // counts as the finest.
        void stepQuantization(bool finer) {
            const auto choices = PianoRoll::quantizations();
            const auto index = choices.indexOf(roll->quantization());
            const auto last = choices.size() - 1;
            const auto next = std::clamp(index + (finer ? 1 : -1), qsizetype(0), last);
            roll->setQuantization(choices[next]);
        }

        void initActions() {
            stdc_decl_t;
            context = new QAK::WidgetActionContext(&decl);
            context->addMenuBar(QStringLiteral("helloutau.mainMenu"), decl.menuBar());
            initToolBar();
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
            addCommand(QStringLiteral("helloutau.tools.editVoiceBank"), [this] {
                if (roll->selectedIndices().isEmpty()) {
                    editVoiceBank();
                } else {
                    showEntry();
                }
            });
            // An external action: its menu is ours to fill, each time it opens.
            recentMenu = new QMenu(&decl);
            QObject::connect(recentMenu, &QMenu::aboutToShow, &decl, [this] { fillRecentMenu(); });
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
            addCommand(QStringLiteral("helloutau.file.exportUst"), [this] {
                stdc_decl_t;
                decl.exportUst();
            });
            addCommand(QStringLiteral("helloutau.file.properties"), [this] { editProperties(); });
            addCommand(QStringLiteral("helloutau.edit.setTempo"), [this] {
                const auto selected = roll->selectedIndices();
                if (!selected.isEmpty()) {
                    editTempo(selected.first());
                }
            });
            addCommand(QStringLiteral("helloutau.edit.noteProperties"),
                       [this] { editNoteProperties(); });
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
            addCommand(QStringLiteral("helloutau.edit.find"), [this] {
                findBar->showFind();
                updateFindResult();
            });
            addCommand(QStringLiteral("helloutau.edit.replace"), [this] {
                findBar->showReplace();
                updateFindResult();
            });
            addCommand(QStringLiteral("helloutau.edit.findNext"), [this] { findAgain(true); });
            addCommand(QStringLiteral("helloutau.edit.findPrevious"),
                       [this] { findAgain(false); });
            addCommand(QStringLiteral("helloutau.edit.insertNote"), [this] {
                edit(tr("Insert Note"), [this](kit::DiagnosticList &diagnostics) {
                    return roll->insertNote(diagnostics);
                });
            });
            addCommand(QStringLiteral("helloutau.edit.insertRest"), [this] {
                edit(tr("Insert Rest"), [this](kit::DiagnosticList &diagnostics) {
                    return roll->insertRest(diagnostics);
                });
            });
            addCommand(QStringLiteral("helloutau.edit.mergeNotes"), [this] {
                edit(tr("Merge Notes"), [this](kit::DiagnosticList &diagnostics) {
                    return roll->mergeSelected(diagnostics);
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
            addCommand(QStringLiteral("helloutau.edit.convertPitchToMode1"), [this] {
                edit(tr("Convert Mode2 Pitch to Mode1"), [this](kit::DiagnosticList &diagnostics) {
                    return roll->convertPitchToMode1(diagnostics);
                });
            });
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
            // The display toggles start from the settings and are stored when changed.
            const auto showPitch = addCommand(QStringLiteral("helloutau.view.showPitch"), [this] {
                const bool visible =
                    actions.value(QStringLiteral("helloutau.view.showPitch"))->isChecked();
                editor->settings().setPitchVisible(visible);
                roll->setPitchVisible(visible);
                updatePitchActions();
            });
            showPitch->setCheckable(true);
            showPitch->setChecked(editor->settings().isPitchVisible());
            const auto showRenderedPitch =
                addCommand(QStringLiteral("helloutau.view.showRenderedPitch"), [this] {
                    const bool visible =
                        actions.value(QStringLiteral("helloutau.view.showRenderedPitch"))
                            ->isChecked();
                    editor->settings().setRenderedPitchVisible(visible);
                    roll->setRenderedPitchVisible(visible);
                });
            showRenderedPitch->setCheckable(true);
            showRenderedPitch->setChecked(editor->settings().isRenderedPitchVisible());
            const auto showEnvelopes =
                addCommand(QStringLiteral("helloutau.view.showEnvelopes"), [this] {
                    const bool visible =
                        actions.value(QStringLiteral("helloutau.view.showEnvelopes"))->isChecked();
                    editor->settings().setEnvelopesVisible(visible);
                    roll->setEnvelopesVisible(visible);
                });
            showEnvelopes->setCheckable(true);
            showEnvelopes->setChecked(editor->settings().areEnvelopesVisible());
            const auto showParameters =
                addCommand(QStringLiteral("helloutau.view.showParameters"), [this] {
                    const bool visible =
                        actions.value(QStringLiteral("helloutau.view.showParameters"))->isChecked();
                    editor->settings().setParametersVisible(visible);
                    roll->setParametersVisible(visible);
                });
            showParameters->setCheckable(true);
            showParameters->setChecked(editor->settings().areParametersVisible());
            const auto showToolBar =
                addCommand(QStringLiteral("helloutau.view.showToolBar"), [this] {
                    const bool visible =
                        actions.value(QStringLiteral("helloutau.view.showToolBar"))->isChecked();
                    editor->settings().setToolBarVisible(visible);
                    toolBar->setVisible(visible);
                });
            showToolBar->setCheckable(true);
            showToolBar->setChecked(editor->settings().isToolBarVisible());
            addCommand(QStringLiteral("helloutau.edit.finerQuantization"),
                       [this] { stepQuantization(true); });
            addCommand(QStringLiteral("helloutau.edit.coarserQuantization"),
                       [this] { stepQuantization(false); });
            addCommand(QStringLiteral("helloutau.view.commandPalette"), [this] {
                palette->setCommands(commandEntries());
                palette->setRecentIds(editor->settings().recentCommands());
                palette->popup();
            });
            addCommand(QStringLiteral("helloutau.playback.play"), [this] { togglePlayback(); });
            addCommand(QStringLiteral("helloutau.playback.pause"), [this] { pauseOrResume(); });
            addCommand(QStringLiteral("helloutau.playback.stop"), [this] { playback->stop(); });
            addCommand(QStringLiteral("helloutau.playback.replay"), [this] { replay(); });
            addCommand(QStringLiteral("helloutau.playback.saveLastPlayed"),
                       [this] { saveLastPlayed(); });
            addCommand(QStringLiteral("helloutau.playback.renderTrack"), [this] { renderTrack(); });
            addCommand(QStringLiteral("helloutau.tools.clearCache"), [this] { clearCache(); });
            addCommand(QStringLiteral("helloutau.tools.settings"), [this] {
                stdc_decl_t;
                editor->showSettings(&decl);
            });
            ActionRegistrations::instance().addActions(&decl, context);

            const auto registry = editor->actionRegistry(Editor::ProjectWindowKind);
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

            findBar = new FindBar(&decl);
            QObject::connect(findBar, &FindBar::queryChanged, &decl,
                             [this] { findLyric(true, true); });
            QObject::connect(findBar, &FindBar::findNextRequested, &decl,
                             [this] { findLyric(true, false); });
            QObject::connect(findBar, &FindBar::findPreviousRequested, &decl,
                             [this] { findLyric(false, false); });
            QObject::connect(findBar, &FindBar::replaceRequested, &decl,
                             [this] { replaceLyric(); });
            QObject::connect(findBar, &FindBar::replaceAllRequested, &decl,
                             [this] { replaceAllLyrics(); });
        }

        // Returns the notes whose lyric search matches, rests included, in track order.
        QList<int> lyricMatches(const kit::TextSearch &search) const {
            QList<int> matches;
            const auto timeline = roll->timeline();
            for (int i = 0; i < timeline->noteCount(); ++i) {
                if (search.matches(timeline->note(i).lyric)) {
                    matches.push_back(i);
                }
            }
            return matches;
        }

        // Counts the matches of the find bar and shows whether the selected note is one of them.
        void updateFindResult() {
            if (findBar->isHidden()) {
                return;
            }
            const auto search = FindSupport::searchOf(findBar);
            roll->setLyricSearch(search);
            const auto selected = roll->selectedIndices();
            FindSupport::showResult(findBar, search, lyricMatches(search),
                                    selected.size() == 1 ? selected.first() : -1);
        }

        // Selects the next or the previous note whose lyric matches, from the first selected
        // note, as the find widget of VS Code moves from the cursor. With inclusive, as for typing
        // in the find field, the first selected note is itself a candidate.
        void findLyric(bool forward, bool inclusive) {
            const auto search = FindSupport::searchOf(findBar);
            const auto matches = lyricMatches(search);
            const auto selected = roll->selectedIndices();
            const auto at = FindSupport::adjacentMatch(
                matches, selected.isEmpty() ? -1 : selected.first(), forward, inclusive);
            if (at) {
                const int index = matches[*at];
                roll->setSelectedIndices({index});
                roll->showNote(index);
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
            findLyric(forward, false);
        }

        // Replaces the matches in the lyric of the selected note if it matches, and selects the
        // next match, as Replace of VS Code does. Without a matching note selected, only the next
        // match is selected.
        void replaceLyric() {
            const auto search = FindSupport::searchOf(findBar);
            const auto selected = roll->selectedIndices();
            if (search.isValid() && selected.size() == 1) {
                const int index = selected.first();
                const auto lyric = roll->timeline()->note(index).lyric;
                if (search.matches(lyric) &&
                    !setLyrics({{index, search.replaced(lyric, findBar->replacement())}},
                               tr("Replace"))) {
                    return;
                }
            }
            findLyric(true, false);
        }

        // Replaces the matches in every lyric as one undo step and selects the changed notes.
        void replaceAllLyrics() {
            stdc_decl_t;
            const auto search = FindSupport::searchOf(findBar);
            QList<std::pair<int, QString>> lyrics;
            for (const int index : lyricMatches(search)) {
                const auto lyric = roll->timeline()->note(index).lyric;
                const auto replaced = search.replaced(lyric, findBar->replacement());
                if (replaced != lyric) {
                    lyrics.push_back({index, replaced});
                }
            }
            if (lyrics.isEmpty() || !setLyrics(lyrics, tr("Replace All"))) {
                return;
            }
            QList<int> changed;
            for (const auto &lyric : std::as_const(lyrics)) {
                changed.push_back(lyric.first);
            }
            roll->setSelectedIndices(changed);
            decl.statusBar()->showMessage(
                ProjectWindow::tr("%n lyric(s) replaced.", nullptr, int(changed.size())),
                StatusMessageTimeout);
        }

        // Sets the lyric of each note index to its text as one undo step named title. Returns
        // whether the step is committed.
        bool setLyrics(const QList<std::pair<int, QString>> &lyrics, const QString &title) {
            stdc_decl_t;
            if (roll->lyricEditor()->isVisible()) {
                return false;
            }
            const auto notes = kit::ProjectRef(document->session()).tracks().at(0).notes();
            auto transaction = document->session()->transaction(title);
            kit::DiagnosticList diagnostics;
            for (const auto &[index, lyric] : lyrics) {
                kit::NotePropertyChanges changes;
                changes.lyric = lyric;
                if (!kit::ProjectEdits::setNoteProperties({notes.at(index)}, changes,
                                                          diagnostics)) {
                    DiagnosticBox::show(&decl, title, diagnostics);
                    return false;
                }
            }
            const bool committed = transaction.commit(diagnostics);
            DiagnosticBox::show(&decl, title, diagnostics);
            return committed;
        }

        QList<CommandEntry> commandEntries() const {
            return commandEntriesOf(editor->actionRegistry(Editor::ProjectWindowKind), context);
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
            showQuantization(roll->quantization());
            QObject::connect(roll, &PianoRoll::quantizationChanged, &decl,
                             [this](int ticks) { showQuantization(ticks); });
            if (actions.value(QStringLiteral("helloutau.edit.penTool"))->isChecked()) {
                roll->setTool(PianoRoll::PenTool);
            } else if (actions.value(QStringLiteral("helloutau.edit.pitchTool"))->isChecked()) {
                roll->setTool(PianoRoll::PitchTool);
            }
            roll->setPitchVisible(
                actions.value(QStringLiteral("helloutau.view.showPitch"))->isChecked());
            roll->setRenderedPitchVisible(
                actions.value(QStringLiteral("helloutau.view.showRenderedPitch"))->isChecked());
            roll->setEnvelopesVisible(
                actions.value(QStringLiteral("helloutau.view.showEnvelopes"))->isChecked());
            roll->setParametersVisible(
                actions.value(QStringLiteral("helloutau.view.showParameters"))->isChecked());
            decl.setCentralWidget(roll);

            QObject::connect(document.get(), &kit::ProjectDocument::voiceBankChanged, roll, [this] {
                roll->setVoiceBank(document->voiceBank());
                updateBackground();
            });
            QObject::connect(roll, &PianoRoll::selectionChanged, &decl, [this] {
                updateEditActions();
                updateFindResult();
            });
            findBar->setAnchor(roll->view());
            QObject::connect(findBar, &FindBar::closed, roll,
                             [this] { roll->setLyricSearch(kit::TextSearch()); });
            // In the status bar, so that a refused drag does not stop the work with a dialog.
            // A dialog remains an alternative, see the open questions in docs/Tuning.md.
            QObject::connect(roll, &PianoRoll::cursorMoved, &decl, [this] { cursorMoved(); });
            QObject::connect(roll, &PianoRoll::tempoRequested, &decl,
                             [this](int index) { editTempo(index); });
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
                scheduleRenderStates();
            });
            QObject::connect(document->session(), &kit::ProjectSession::stepChanged, &decl, [this] {
                updateUndoActions();
                updatePitchActions();
                updateFindResult();
                // An undo or redo that changes the voice folder reads the folder again. The read
                // is queued so that it does not run inside the notification of the step.
                if (!voiceBankReloadPending && voiceRoot() != voiceBankRoot) {
                    voiceBankReloadPending = true;
                    QMetaObject::invokeMethod(
                        _decl,
                        [this] {
                            voiceBankReloadPending = false;
                            if (voiceRoot() != voiceBankRoot) {
                                _decl->loadVoiceBank();
                            }
                        },
                        Qt::QueuedConnection);
                }
                // A preview plays, and the background renders, the notes as they now are.
                playback->updatePlan(*document);
                scheduleRenderStates();
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
            for (const auto id :
                 {"helloutau.edit.delete", "helloutau.edit.editLyric",
                  "helloutau.edit.togglePortamento", "helloutau.edit.toggleVibrato",
                  "helloutau.edit.editVibrato", "helloutau.edit.scalePitch",
                  "helloutau.edit.convertPitchToMode1", "helloutau.edit.crossfadeP2P3",
                  "helloutau.edit.crossfadeP1P4", "helloutau.edit.copy",
                  "helloutau.edit.transposeUp", "helloutau.edit.transposeDown",
                  "helloutau.edit.octaveUp", "helloutau.edit.octaveDown", "helloutau.edit.setTempo",
                  "helloutau.edit.noteProperties"}) {
                actions.value(QLatin1String(id))->setEnabled(selected > 0);
            }
            actions.value(QStringLiteral("helloutau.edit.splitNote"))->setEnabled(selected == 1);
            actions.value(QStringLiteral("helloutau.edit.mergeNotes"))->setEnabled(selected > 1);
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

        // Opens the voice bank at the entry that the first selected note uses. For a rest, or a
        // note without an entry, the voice bank opens and the status bar reports the reason.
        void showEntry() {
            stdc_decl_t;
            const auto indices = roll->selectedIndices();
            if (indices.isEmpty()) {
                return;
            }
            const auto &note = roll->timeline()->note(indices.first());
            const auto window = editVoiceBank();
            if (window && note.rest) {
                decl.statusBar()->showMessage(tr("A rest has no entry."), StatusMessageTimeout);
            } else if (window && !window->showEntryFor(note.key, note.lyric)) {
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
            return ProjectWindow::tr(text);
        }
    };

    ProjectWindow::ProjectWindow(Editor *editor, std::unique_ptr<kit::ProjectDocument> document)
        : _impl(std::make_unique<Impl>(this, editor)) {
        stdc_impl_t;
        impl.initActions();
        impl.document = std::move(document);
        impl.bindDocument();
        editor->themeManager()->install(this, {QStringLiteral("ProjectWindow")});
        // Files dropped on the window open, see dropEvent(). QMainWindow accepts drops already,
        // for its dock widgets, which the window does not rely on.
        setAcceptDrops(true);
        resize(960, 640);
    }

    ProjectWindow::~ProjectWindow() {
        stdc_impl_t;
        // Stopping updates the piano roll, which refers to the session of the document, which
        // goes with _impl.
        impl.playback->stop();
        delete impl.roll;
    }

    Editor *ProjectWindow::editor() const {
        stdc_impl_t;
        return impl.editor;
    }

    kit::ProjectDocument *ProjectWindow::document() const {
        stdc_impl_t;
        return impl.document.get();
    }

    QAK::WidgetActionContext *ProjectWindow::actionContext() const {
        stdc_impl_t;
        return impl.context;
    }

    PianoRoll *ProjectWindow::pianoRoll() const {
        stdc_impl_t;
        return impl.roll;
    }

    QComboBox *ProjectWindow::quantizationBox() const {
        stdc_impl_t;
        // The box created last, by the latest rebuild of the tool bar
        for (auto it = impl.quantizationBoxes.crbegin(); it != impl.quantizationBoxes.crend();
             ++it) {
            if (*it) {
                return *it;
            }
        }
        return nullptr;
    }

    void ProjectWindow::setDocument(std::unique_ptr<kit::ProjectDocument> document) {
        stdc_impl_t;
        impl.playback->stop();
        impl.voiceBankRoot.clear();
        impl.voiceBankReloadPending = false;
        auto previous = std::move(impl.document);
        impl.document = std::move(document);
        impl.bindDocument();
        Q_EMIT documentChanged();
    }

    void ProjectWindow::applySettings() {
        stdc_impl_t;
        // What plays in the other mode stops.
        const auto state = impl.playback->state();
        if (impl.realtime() ? state == Playback::Rendering : impl.previewing) {
            impl.playback->stop();
        }
        impl.updateBackground();
    }

    bool ProjectWindow::isUnused() const {
        stdc_impl_t;
        return impl.document->sourcePath().empty() && !impl.document->isModified();
    }

    bool ProjectWindow::loadVoiceBank() {
        stdc_impl_t;
        const auto document = impl.document.get();
        const auto utau = impl.editor->settings().utauDirectory();
        impl.voiceBankRoot = impl.voiceRoot();
        VoiceBankCharsetDialog selector(this);
        selector.setRoot(impl.voiceBankRoot);
        kit::DiagnosticList diagnostics;
        const bool loaded = document->loadVoiceBank(utau, &selector, diagnostics);
        DiagnosticBox::show(this, tr("Voice Bank"), diagnostics);
        return loaded;
    }

    bool ProjectWindow::save() {
        stdc_impl_t;
        if (impl.document->filePath().empty()) {
            return saveAs();
        }
        kit::DiagnosticList diagnostics;
        const bool saved = impl.document->save(diagnostics);
        DiagnosticBox::show(this, tr("Save"), diagnostics);
        return saved;
    }

    bool ProjectWindow::saveAs() {
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

    bool ProjectWindow::exportUst() {
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

    void ProjectWindow::closeEvent(QCloseEvent *event) {
        stdc_impl_t;
        if (!impl.maybeSave()) {
            event->ignore();
            return;
        }
        QMainWindow::closeEvent(event);
    }

    void ProjectWindow::dragEnterEvent(QDragEnterEvent *event) {
        if (!localFilesOf(event->mimeData()).isEmpty()) {
            event->acceptProposedAction();
        }
    }

    void ProjectWindow::dropEvent(QDropEvent *event) {
        const auto files = localFilesOf(event->mimeData());
        if (files.isEmpty()) {
            return;
        }
        event->acceptProposedAction();
        // Later, so that the dialogs that opening may show run after the drag has ended
        QTimer::singleShot(0, this, [this, files] {
            stdc_impl_t;
            for (const auto &file : files) {
                impl.editor->openFile(file, this);
            }
        });
    }

}
