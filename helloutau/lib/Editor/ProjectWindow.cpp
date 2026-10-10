#include "ProjectWindow.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <optional>

#include <QtCore/QDir>
#include <QtCore/QHash>
#include <QtCore/QJsonObject>
#include <QtCore/QMetaObject>
#include <QtCore/QMimeData>
#include <QtCore/QRegularExpression>
#include <QtCore/QSignalBlocker>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>
#include <QtCore/QTemporaryDir>
#include <QtGui/QAction>
#include <QtGui/QActionGroup>
#include <QtGui/QClipboard>
#include <QtGui/QCloseEvent>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include <stdcorelib/pimpl.h>

#include <stdutau/utaconst.h>

#include <QAKCore/actionextension.h>
#include <QAKCore/actionregistry.h>
#include <QAKWidgets/widgetactioncontext.h>

#include <hellokit/Document/Project.h>
#include <hellokit/Edit/ProjectDocument.h>
#include <hellokit/Edit/ProjectEdits.h>
#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Edit/TrackTimeline.h>
#include <hellokit/Synth/ClassicSynthRunner.h>
#include <hellokit/Synth/SynthToolProcess.h>
#include <hellokit/Synth/ThreadedSynthRunner.h>

#include <helloutau/Theme/ThemeManager.h>
#include <helloutau/Widgets/ActionIconToggle.h>
#include <helloutau/Widgets/CommandPalette.h>
#include <helloutau/Widgets/FindBar.h>
#include <helloutau/Widgets/PianoKeyboard.h>
#include <helloutau/Widgets/SceneView.h>
#include <helloutau/Widgets/TimelineRuler.h>
#include <helloutau/Widgets/ToolBarPalette.h>

#include "AboutDialog.h"
#include "AppSettings.h"
#include "CommandEntries_p.h"
#include "DiagnosticBox.h"
#include "Editor.h"
#include "SynthToolTrust.h"
#include "ReplaceLyricsDialog.h"
#include "ExportUstDialog.h"
#include "FindSupport_p.h"
#include "PianoRoll.h"
#include "NotePropertiesDialog.h"
#include "ProjectPropertiesDialog.h"
#include "Playback.h"
#include "PasteParametersDialog.h"
#include "RegionDialog.h"
#include "ScalePitchDialog.h"
#include "PitchControlDialog.h"
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
                                           const char16_t *extension,
                                           const QString &fallback) {
            auto path = document.sourcePath();
            if (path.empty()) {
                path = pathOf(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
                path /= std::filesystem::path(fallback.toStdU16String());
            }
            return path.replace_extension(extension);
        }

        // Returns the file name of a wav file named after baseName. The characters that a file name
        // cannot contain on Windows, the path separators among them, become underscores, so that
        // a project name such as Song v1.2 or ..\x remains a single file name. The extension is
        // appended rather than replaced, because a dot in a name does not start an extension.
        std::filesystem::path audioPathOf(const QString &baseName) {
            QString name;
            for (const auto c : baseName) {
                const bool invalid = c < QChar(u' ') || QStringView(u"\\/:*?\"<>|").contains(c);
                name += invalid ? QChar(u'_') : c;
            }
            // Windows removes trailing dots and spaces from a file name.
            while (name.endsWith(u'.') || name.endsWith(u' ')) {
                name.chop(1);
            }
            if (name.isEmpty()) {
                name = QStringLiteral("track");
            }
            if (!name.endsWith(u".wav", Qt::CaseInsensitive)) {
                name += u".wav";
            }
            return std::filesystem::path(name.toStdU16String());
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
        // Shows pause on the play command while a press of it pauses or cancels a render
        ActionIconToggle *playIconToggle = nullptr;
        QActionGroup *tools = nullptr;
        CommandPalette *palette = nullptr;
        FindBar *findBar = nullptr;
        QToolBar *toolBar = nullptr;
        // The boxes of the quantization in the tool bars, which follow the piano roll
        QList<QPointer<QComboBox>> quantizationBoxes;
        QList<QPointer<QDoubleSpinBox>> tempoBoxes;
        QList<QPointer<QToolButton>> timeSignatureButtons;
        // The piano roll that last received the time signature, and that time signature
        QPointer<PianoRoll> timeSignatureRoll;
        kit::TimeSignature rollTimeSignature;
        QMenu *recentMenu = nullptr;
        QMenu *regionMenu = nullptr;
        // What Paste Parameters pasted last
        PianoRoll::Parameters pastedParameters = PianoRoll::AllParameters;

        Playback *playback = nullptr;
        // Owns all per-window render artifacts. Playback only uses the path.
        std::optional<QTemporaryDir> temporaryDirectory;
        std::shared_ptr<kit::SynthToolOutputLog> renderLog =
            std::make_shared<kit::SynthToolOutputLog>();
        QLabel *renderLabel = nullptr;
        QProgressBar *renderProgress = nullptr;
        QPushButton *renderCancel = nullptr;
        QLabel *beatDurationLabel = nullptr;
        QLabel *selectionDurationLabel = nullptr;
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
        // The kind of the runner of the playback, its thread count and its script directory, see
        // updateRunner()
        std::optional<bool> runnerClassic;
        int runnerThreads = 0;
        std::filesystem::path runnerDirectory;
        // The notes last rendered, which Replay renders again
        std::optional<std::pair<int, int>> lastRange;
        bool restartPending = false;
        // The synth tools of the project, the wavtool and the resampler, as updateBackground() last
        // recorded them
        QStringList backgroundSynthTools;

        // Replaces the temporary directory of the window and returns its path, or an empty path
        // if it cannot be created, in which case Playback refuses to render.
        std::filesystem::path newTemporaryDirectory() {
            temporaryDirectory.emplace();
            return temporaryPath();
        }

        // Returns the path of the temporary directory of the window, or an empty path if it
        // could not be created.
        std::filesystem::path temporaryPath() const {
            if (!temporaryDirectory || !temporaryDirectory->isValid()) {
                return {};
            }
            return std::filesystem::path(temporaryDirectory->path().toStdU16String());
        }

        // The render progress in the status bar, and the playhead that follows playback
        void initPlayback() {
            stdc_decl_t;
            playback = new Playback(renderLog, newTemporaryDirectory(), &decl);
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
            beatDurationLabel = new QLabel();
            selectionDurationLabel = new QLabel();
            beatDurationLabel->setMargin(4);
            selectionDurationLabel->setMargin(4);
            beatDurationLabel->setToolTip(tr("Duration of one quarter note at the current tempo"));
            selectionDurationLabel->setToolTip(tr("Duration of the selected notes"));
            decl.statusBar()->addPermanentWidget(beatDurationLabel);
            decl.statusBar()->addPermanentWidget(selectionDurationLabel);
            QObject::connect(renderCancel, &QPushButton::clicked, playback, &Playback::stop);

            QObject::connect(
                playback, &Playback::stateChanged, &decl, [this](Playback::State state) {
                    scheduleRenderStates();
                    updateSaveLastPlayed();
                    const bool rendering = state == Playback::Rendering;
                    if (playIconToggle) {
                        playIconToggle->setToggled(state == Playback::Playing || rendering);
                    }
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
                                ? std::optional(roll->timeline()->tempoMap().tickOf(*at))
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
                                if (realtime() && playback->state() == Playback::Stopped) {
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
                                 DiagnosticBox::report(&decl, tr("Play"), diagnostics);
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
            DiagnosticBox::report(&decl, tr("Tempo"), diagnostics);
        }

        // The label of note index, entered by the user; an empty text removes it
        void editLabel(int index) {
            stdc_decl_t;
            const auto notes = kit::ProjectRef(document->session()).tracks().at(0).notes();
            if (index < 0 || index >= notes.size()) {
                return;
            }
            bool ok = false;
            const auto label =
                QInputDialog::getText(&decl, tr("Set Label"), tr("&Label:"), QLineEdit::Normal,
                                      notes.at(index).label(), &ok);
            if (!ok) {
                return;
            }
            kit::DiagnosticList diagnostics;
            kit::ProjectEdits::setLabel(notes.at(index), label, diagnostics);
            DiagnosticBox::report(&decl, tr("Set Label"), diagnostics);
        }

        void replaceLyrics() {
            stdc_decl_t;
            const auto indices = roll->selectedIndices();
            if (indices.isEmpty() || roll->lyricEditor()->isVisible()) {
                return;
            }
            const auto notes = kit::ProjectRef(document->session()).tracks().at(0).notes();
            QStringList initial;
            for (const int index : indices) {
                initial.push_back(notes.at(index).lyric());
            }
            ReplaceLyricsDialog dialog(&decl);
            dialog.setLyrics(initial.join(u' '));
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }
            QStringList lyrics;
            if (dialog.splitCharacters()) {
                for (const auto character : dialog.lyrics()) {
                    if (!character.isSpace()) {
                        lyrics.push_back(QString(character));
                    }
                }
            } else {
                lyrics = dialog.lyrics().split(QRegularExpression(QStringLiteral("\\s+")),
                                               Qt::SkipEmptyParts);
            }
            if (lyrics.isEmpty()) {
                return;
            }
            QList<std::pair<int, QString>> changes;
            for (int i = 0; i < indices.size(); ++i) {
                if (!dialog.repeat() && i >= lyrics.size()) {
                    break;
                }
                changes.push_back({indices.at(i), lyrics.at(i % lyrics.size())});
            }
            if (!changes.isEmpty() && setLyrics(changes, tr("Replace Lyrics"))) {
                roll->setSelectedIndices(indices);
            }
        }

        // A new region of the notes from first to last, named by the user
        void nameRegion(int first, int last) {
            stdc_decl_t;
            const auto notes = kit::ProjectRef(document->session()).tracks().at(0).notes();
            if (first < 0 || last < first || last >= notes.size()) {
                return;
            }
            bool ok = false;
            const auto name = QInputDialog::getText(&decl, tr("Name Region"), tr("&Name:"),
                                                    QLineEdit::Normal, QString(), &ok);
            if (!ok) {
                return;
            }
            kit::DiagnosticList diagnostics;
            kit::ProjectEdits::nameRegion(notes, first, last - first + 1, name, diagnostics);
            DiagnosticBox::report(&decl, tr("Name Region"), diagnostics);
        }

        // Renames region to the name entered by the user, or removes it if the name is empty.
        // The other regions at its notes are kept.
        void editRegion(const kit::Region &region) {
            stdc_decl_t;
            const auto notes = kit::ProjectRef(document->session()).tracks().at(0).notes();
            bool ok = false;
            const auto name = QInputDialog::getText(&decl, tr("Rename Region"), tr("&Name:"),
                                                    QLineEdit::Normal, region.name, &ok);
            if (!ok) {
                return;
            }
            kit::DiagnosticList diagnostics;
            if (name.isEmpty()) {
                kit::ProjectEdits::removeRegion(notes, region, diagnostics);
            } else {
                kit::ProjectEdits::renameRegion(notes, region, name, diagnostics);
            }
            DiagnosticBox::report(&decl, tr("Rename Region"), diagnostics);
        }

        // Lists the regions of the track in their dialog with current selected if given. Each
        // removal is one undo step.
        void editRegions(const std::optional<kit::Region> &current) {
            stdc_decl_t;
            RegionDialog dialog(&decl);
            dialog.setRegions(roll->regions());
            if (current) {
                dialog.setCurrentRegion(*current);
            }
            QObject::connect(&dialog, &RegionDialog::goToRequested, &decl,
                             [this](const kit::Region &region) {
                                 roll->loadRegion(region.first, region.last);
                             });
            QObject::connect(&dialog, &RegionDialog::removeRequested, &decl,
                             [this, &dialog](const kit::Region &region) {
                                 const auto notes =
                                     kit::ProjectRef(document->session()).tracks().at(0).notes();
                                 kit::DiagnosticList diagnostics;
                                 kit::ProjectEdits::removeRegion(notes, region, diagnostics);
                                 DiagnosticBox::report(&dialog, tr("Remove Region"), diagnostics);
                                 dialog.setRegions(roll->regions());
                             });
            dialog.exec();
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
            QList<NotePropertiesDialog::Defaults> defaults;
            const auto bank = document->voiceBank();
            const auto &tempoMap = roll->timeline()->tempoMap();
            for (int i = 0; i < indices.size(); ++i) {
                NotePropertiesDialog::Defaults item;
                item.tempo = tempoMap.tempo(indices.at(i));
                const auto sample =
                    bank ? bank->find(notes.at(i).noteNum, notes.at(i).lyric) : nullptr;
                if (sample) {
                    item.preUtterance = sample->preUtterance;
                    item.voiceOverlap = sample->voiceOverlap;
                }
                defaults.push_back(item);
            }
            NotePropertiesDialog dialog(notes, defaults, &decl);
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }
            kit::DiagnosticList diagnostics;
            kit::ProjectEdits::setNoteProperties(selected, dialog.changes(), diagnostics);
            DiagnosticBox::report(&decl, tr("Note Properties"), diagnostics);
        }

        // Edits the properties of the project in their dialog in one step. A voice folder that
        // denotes another directory is read at once. A voice folder that is only written
        // differently is not read again.
        void editProperties() {
            stdc_decl_t;
            ProjectPropertiesDialog dialog(document->session()->snapshot(), editor->settings(),
                                           &decl);
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }
            const auto changes = dialog.changes();
            bool newFolder = false;
            if (!changes.isEmpty()) {
                kit::DiagnosticList diagnostics;
                const bool changed = kit::ProjectEdits::setProperties(
                    kit::ProjectRef(document->session()), changes, diagnostics);
                DiagnosticBox::report(&decl, tr("Project Properties"), diagnostics);
                newFolder = changed && changes.voiceDir.has_value() &&
                            !sameVoiceRoot(voiceRoot(), voiceBankRoot);
            }
            // A folder that failed to load before is read again once the dialog confirms it.
            const auto root = voiceRoot();
            const auto bankNeedsCharset = [&] {
                const auto bank = document->voiceBank();
                if (!bank) {
                    return false;
                }
                return std::any_of(bank->directories().cbegin(), bank->directories().cend(),
                                   [](const auto &directory) { return directory.leftOut; });
            };
            if (newFolder || bankNeedsCharset() ||
                (!document->voiceBank() && !root.empty() && root == voiceBankRoot)) {
                decl.loadVoiceBank();
            }
            // The synth tools may have been trusted in the dialog without a change of the project,
            // which lets the background render start.
            updateBackground();
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
            return track.voiceDirectory(editor->settings().voiceLocations());
        }

        // Returns whether the voice folder of the project is empty or an existing directory.
        bool voicePathValid() const {
            const auto project = document->session()->snapshot();
            if (project.tracks.isEmpty() || project.tracks.first().voiceDir.isEmpty()) {
                return true;
            }
            const auto root =
                project.tracks.first().voiceDirectory(editor->settings().voiceLocations());
            std::error_code error;
            return !root.empty() && std::filesystem::is_directory(root, error);
        }

        // Returns whether each synth tool of the project is empty or an existing file.
        bool synthToolPathsValid() const {
            const auto project = document->session()->snapshot();
            const auto utau = editor->settings().utauDirectory();
            const auto exists = [&utau](const QString &value) {
                if (value.isEmpty()) {
                    return true;
                }
                std::error_code error;
                return std::filesystem::is_regular_file(SynthToolTrust::resolved(value, utau),
                                                        error);
            };
            return exists(project.settings.wavtool) && exists(project.settings.resampler);
        }

        // Returns whether two voice bank directories are the same directory.
        static bool sameVoiceRoot(const std::filesystem::path &first,
                                  const std::filesystem::path &second) {
            if (first.empty() || second.empty()) {
                return first.empty() && second.empty();
            }
            std::error_code firstError;
            std::error_code secondError;
            const auto firstCanonical = std::filesystem::weakly_canonical(first, firstError);
            const auto secondCanonical = std::filesystem::weakly_canonical(second, secondError);
            if (firstError || secondError) {
                return first == second;
            }
            return firstCanonical == secondCanonical;
        }

        // Deletes the render cache of the project; the realtime mode renders it anew.
        void clearCache() {
            stdc_decl_t;
            kit::DiagnosticList diagnostics;
            const auto deleted = playback->clearCache(*document, diagnostics);
            DiagnosticBox::report(&decl, tr("Clear Render Cache"), diagnostics);
            if (!deleted) {
                return;
            }
            decl.statusBar()->showMessage(
                ProjectWindow::tr("%n file(s) deleted from the render cache.", nullptr, *deleted),
                StatusMessageTimeout);
            updateBackground();
        }

        void showRenderLog() {
            stdc_decl_t;
            QDialog dialog(&decl);
            dialog.setWindowTitle(tr("Render Log"));
            dialog.resize(720, 480);
            auto layout = new QVBoxLayout(&dialog);
            auto text = new QPlainTextEdit(&dialog);
            text->setReadOnly(true);
            text->setLineWrapMode(QPlainTextEdit::NoWrap);
            text->setPlainText(renderLog->text());
            text->moveCursor(QTextCursor::End);
            text->verticalScrollBar()->setValue(text->verticalScrollBar()->maximum());
            layout->addWidget(text);
            auto buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
            auto clear = buttons->addButton(tr("Clear"), QDialogButtonBox::DestructiveRole);
            QObject::connect(clear, &QPushButton::clicked, &dialog, [this, text] {
                renderLog->clear();
                text->clear();
            });
            QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            layout->addWidget(buttons);
            dialog.exec();
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

        // Returns the texts of the synth tools of the project, Tool1 and Tool2.
        QStringList synthToolTexts() const {
            const auto project = kit::ProjectRef(document->session()).settings();
            return {project.wavtool(), project.resampler()};
        }

        // Returns the synth tools of the project, the wavtool and the resampler, if both exist and
        // may run, in every playback mode: the realtime preview runs no wavtool, but Render
        // Track does. Returns std::nullopt otherwise. If title is given, reports a synth tool that
        // does not exist in a message box with that title and asks the user to trust the
        // synth tools that are not trusted. Without a title, asks and reports nothing, as for the
        // render in the background.
        std::optional<kit::SynthTools> projectSynthTools(const QString &title = {}) {
            stdc_decl_t;
            auto &settings = editor->settings();
            const auto project = kit::ProjectRef(document->session()).settings();
            const auto utau = settings.utauDirectory();
            const auto values = synthToolTexts();
            const bool exist =
                std::all_of(values.cbegin(), values.cend(), [&](const QString &value) {
                    return SynthToolTrust::exists(value, utau);
                });
            if (!exist) {
                if (!title.isEmpty()) {
                    kit::DiagnosticList diagnostics;
                    diagnostics.push_back({kit::DiagnosticSeverity::Error,
                                           ProjectWindow::tr("Set an existing wavtool and "
                                                             "resampler in the project properties "
                                                             "first."),
                                           std::nullopt});
                    DiagnosticBox::report(&decl, title, diagnostics);
                }
                return std::nullopt;
            }
            const bool allowed =
                title.isEmpty()
                    ? std::all_of(values.cbegin(), values.cend(),
                                  [&](const QString &value) {
                                      return SynthToolTrust::isAllowed(settings, value, utau);
                                  })
                    : SynthToolTrust::ask(&decl, settings, values, utau);
            if (!allowed) {
                return std::nullopt;
            }
            kit::SynthTools result;
            result.resampler = SynthToolTrust::resolved(project.resampler(), utau);
            result.wavtool = SynthToolTrust::resolved(project.wavtool(), utau);
            return result;
        }

        // The time of the playhead at rest, in milliseconds
        double cursorTime() const {
            return roll->timeline()->tempoMap().timeOf(roll->cursorPosition());
        }

        void updateTimeStatus() {
            if (!beatDurationLabel || !selectionDurationLabel || !roll || !document) {
                return;
            }
            const auto timeline = roll->timeline();
            const auto &map = timeline->tempoMap();
            const auto projectTempo = kit::ProjectRef(document->session()).settings().tempo();
            const auto selected = roll->selectedIndices();
            const double tempo = !selected.isEmpty() ? map.tempo(selected.first()) : projectTempo;
            const auto seconds = [](double milliseconds) {
                auto value = QString::number(milliseconds / 1000.0, 'f', 6);
                while (value.endsWith(QLatin1Char('0'))) {
                    value.chop(1);
                }
                if (value.endsWith(QLatin1Char('.'))) {
                    value.chop(1);
                }
                return ProjectWindow::tr("%1 sec").arg(value);
            };
            const double beat = tempo > 0 ? 60000.0 / tempo : 0;
            beatDurationLabel->setText(seconds(beat));

            double selection = 0;
            if (!selected.isEmpty()) {
                const auto first = selected.first();
                const auto last = selected.last();
                const auto start = timeline->note(first).start;
                const auto end = timeline->note(last).start + timeline->note(last).length;
                selection = map.timeOf(end) - map.timeOf(start);
            }
            selectionDurationLabel->setText(seconds(selection));
        }

        // Plays in the playback mode of the settings (docs/Widgets.md), pauses what plays,
        // resumes what is paused, or cancels a render. At rest, renders the selected notes, from
        // the first to the last, and plays them, or plays from the playhead as the track is
        // rendered.
        void togglePlayback() {
            stdc_decl_t;
            switch (playback->state()) {
                case Playback::Rendering:
                    playback->stop();
                    return;
                case Playback::Playing:
                    playback->pause();
                    return;
                case Playback::Paused:
                    resumePlayback();
                    return;
                default:
                    break;
            }
            if (realtime()) {
                startPreview();
                return;
            }
            const auto selected = roll->selectedIndices();
            if (selected.isEmpty()) {
                decl.statusBar()->showMessage(
                    ProjectWindow::tr("Select the notes to render first."), StatusMessageTimeout);
                return;
            }
            playRange(std::make_pair(selected.first(), selected.last()));
        }

        // Renders notes range and plays them, or plays the last render again if they sound the
        // same.
        void playRange(std::pair<int, int> range) {
            stdc_decl_t;
            if (!editor->settings().isRenderLogAccumulated()) {
                renderLog->clear();
            }
            lastRange = range;
            const auto synthTools = projectSynthTools(tr("Play"));
            if (!synthTools) {
                return;
            }
            kit::DiagnosticList diagnostics;
            if (!playback->play(*document, range, *synthTools, diagnostics)) {
                DiagnosticBox::report(&decl, tr("Play"), diagnostics);
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
            const auto proposed = audioFolder() / audioPathOf(QStringLiteral("temp"));
            const auto chosen = QFileDialog::getSaveFileName(
                &decl, tr("Save Last Played"), textOf(proposed), tr("WAV files (*.wav)"));
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
        // scripts are written into the temporary directory of the window, and the classic runner
        // keeps them there for inspection. The runner is replaced only when the mode, the thread
        // count or the temporary directory changed, because a new runner discards the kept
        // render.
        void updateRunner() {
            const auto &settings = editor->settings();
            const bool classic = settings.playbackMode() == AppSettings::Prerender;
            const int threads = settings.renderThreadCount();
            const auto directory = temporaryPath();
            playback->setThreadCount(threads);
            if (runnerClassic == classic && (classic || runnerThreads == threads) &&
                runnerDirectory == directory) {
                return;
            }
            if (classic) {
                auto runner = std::make_shared<kit::ClassicSynthRunner>();
                runner->scriptDirectory = directory;
                runner->keepScripts = !directory.empty();
                playback->setRunner(runner);
            } else {
                auto runner = std::make_shared<kit::ThreadedSynthRunner>();
                runner->threadCount = threads;
                runner->scriptDirectory = directory;
                playback->setRunner(runner);
            }
            runnerClassic = classic;
            runnerThreads = threads;
            runnerDirectory = directory;
        }

        // Renders the whole track into a WAV file that the user chooses, by default the output
        // file of the project, as File > Render WAV of UTAU. The render of the last playback
        // stays as it is. A message box reports the written file once the progress dialog has
        // closed.
        void renderTrack() {
            stdc_decl_t;
            playback->stop();
            const auto renderSynthTools = projectSynthTools(tr("Render Track"));
            if (!renderSynthTools) {
                return;
            }
            const auto file = QFileDialog::getSaveFileName(
                &decl, tr("Render Track"),
                QString::fromStdU16String(defaultTrackFile().u16string()), tr("WAV files (*.wav)"));
            if (file.isEmpty()) {
                return;
            }
            if (!editor->settings().isRenderLogAccumulated()) {
                renderLog->clear();
            }
            std::optional<std::filesystem::path> written;
            const auto connection =
                QObject::connect(playback, &Playback::trackRendered, &decl,
                                 [&written](const std::filesystem::path &path) { written = path; });
            kit::DiagnosticList diagnostics;
            const bool started =
                playback->renderTrack(*document, std::filesystem::path(file.toStdU16String()),
                                      *renderSynthTools, diagnostics);
            if (started) {
                waitForRender(tr("Render Track"));
            }
            QObject::disconnect(connection);
            if (!started) {
                DiagnosticBox::report(&decl, tr("Render Track"), diagnostics);
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
            const auto folder = audioFolder();
            auto output =
                kit::Project::pathOf(kit::ProjectRef(document->session()).settings().outputFile());
            if (output.empty()) {
                output = source.empty() ? audioPathOf(projectAudioBaseName())
                                        : source.filename().replace_extension(u".wav");
            }
            return output.is_absolute() ? output : folder / output;
        }

        // Returns the folder in which an audio file of the project is proposed: the folder of the
        // project file, or the music folder of the user for a project without a file.
        std::filesystem::path audioFolder() const {
            const auto source = document->sourcePath();
            return source.empty() ? std::filesystem::path(QStandardPaths::writableLocation(
                                                              QStandardPaths::MusicLocation)
                                                              .toStdU16String())
                                  : source.parent_path();
        }

        QString projectAudioBaseName() const {
            const auto name = kit::ProjectRef(document->session()).settings().name().trimmed();
            return name.isEmpty()
                       ? editor->projectDisplayName(_decl)
                       : name;
        }

        QString projectFileBaseName() const {
            return editor->projectDisplayName(_decl);
        }

        // Save Last Played applies to the prerender mode, after a render.
        void updateSaveLastPlayed() {
            actions.value(QStringLiteral("helloutau.playback.saveLastPlayed"))
                ->setEnabled(!realtime() && playback->state() != Playback::Rendering &&
                             !playback->lastRenderFile().empty());
        }

        // A paused render goes on from where it was, a paused preview previews from there.
        void resumePlayback() {
            stdc_decl_t;
            if (!playback->isPreviewPaused()) {
                playback->resume();
                return;
            }
            const auto at = playback->position();
            const auto synthTools = projectSynthTools(tr("Play"));
            if (!synthTools) {
                previewing = false;
                return;
            }
            if (!editor->settings().isRenderLogAccumulated()) {
                renderLog->clear();
            }
            kit::DiagnosticList diagnostics;
            previewing = playback->preview(*document, at, *synthTools, diagnostics);
            if (!previewing) {
                DiagnosticBox::report(&decl, tr("Play"), diagnostics);
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
            const auto synthTools = projectSynthTools(tr("Play"));
            if (!synthTools) {
                previewing = false;
                return;
            }
            if (!editor->settings().isRenderLogAccumulated()) {
                renderLog->clear();
            }
            kit::DiagnosticList diagnostics;
            previewing = playback->preview(*document, cursorTime(), *synthTools, diagnostics);
            if (!previewing) {
                DiagnosticBox::report(&decl, tr("Play"), diagnostics);
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
            backgroundSynthTools = synthToolTexts();
            // The background render requires the synth tools that playback requires. No message box
            // is shown for it. The states of the notes in the prerender mode depend on the
            // synth tools as well.
            const auto allowed = projectSynthTools();
            if (allowed) {
                playback->setSynthTools(*allowed);
            }
            const auto synthTools = realtime() ? allowed : std::nullopt;
            if (!synthTools) {
                playback->release();
                statusTimer.stop();
                updatePreviewStatus();
                return;
            }
            kit::DiagnosticList diagnostics;
            playback->prepare(*document, cursorTime(), *synthTools, diagnostics);
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
            const auto id = QStringLiteral("helloutau.select.quantizationWidget");
            context->addWidgetFactory(id, [this](QWidget *parent) -> QWidget * {
                auto box = new QComboBox(parent);
                box->setObjectName(QStringLiteral("quantization"));
                box->setToolTip(tr("Quantization"));
                // Wider than its longest choice, which looks cramped in the tool bar
                box->setMinimumContentsLength(6);
                box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
                for (const int ticks : PianoRoll::quantizations()) {
                    box->addItem(PianoRoll::quantizationName(ticks), ticks);
                }
                box->setFixedWidth(int(box->sizeHint().width() * 0.8));
                if (roll) {
                    box->setCurrentIndex(box->findData(roll->quantization()));
                }
                QObject::connect(box, &QComboBox::currentIndexChanged, box, [this, box] {
                    if (roll) {
                        roll->setQuantization(box->currentData().toInt());
                    }
                });
                quantizationBoxes.removeAll(nullptr);
                quantizationBoxes.push_back(box);
                // The user may reorder toolbar items, so the tempo widget can be created before
                // this one. Keep both controls at the same height regardless of that order.
                const auto height = box->sizeHint().height();
                for (const auto &tempo : std::as_const(tempoBoxes)) {
                    if (tempo) {
                        tempo->setFixedHeight(height);
                    }
                }
                return box;
            });
            context->addWidgetFactory(
                QStringLiteral("helloutau.edit.projectTempoWidget"),
                [this](QWidget *parent) -> QWidget * {
                    auto box = new QDoubleSpinBox(parent);
                    box->setObjectName(QStringLiteral("tempo"));
                    box->setToolTip(tr("Project Tempo"));
                    box->setDecimals(2);
                    box->setRange(utau::VALUE_TEMPO_MIN, utau::VALUE_TEMPO_MAX);
                    box->setSuffix(tr(" BPM"));
                    // Without it, typing 140 sets 14 and then 140, in two undo steps.
                    box->setKeyboardTracking(false);
                    quantizationBoxes.removeAll(nullptr);
                    const auto height = quantizationBoxes.isEmpty()
                                            ? box->sizeHint().height()
                                            : quantizationBoxes.first()->sizeHint().height();
                    box->setFixedHeight(height);
                    box->setValue(document ? kit::ProjectRef(document->session()).settings().tempo()
                                           : 120);
                    QObject::connect(
                        box, &QDoubleSpinBox::valueChanged, box, [this](double tempo) {
                            if (!document) {
                                return;
                            }
                            kit::ProjectPropertyChanges changes;
                            changes.tempo = tempo;
                            kit::DiagnosticList diagnostics;
                            kit::ProjectEdits::setProperties(kit::ProjectRef(document->session()),
                                                             changes, diagnostics);
                            DiagnosticBox::report(_decl, tr("Project Tempo"), diagnostics);
                        });
                    tempoBoxes.push_back(box);
                    return box;
                });
            context->addWidgetFactory(
                QStringLiteral("helloutau.view.timeSignatureWidget"), [this](QWidget *parent) {
                    auto button = new QToolButton(parent);
                    button->setObjectName(QStringLiteral("timeSignature"));
                    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
                    button->setAutoRaise(true);
                    button->setToolTip(tr("Time Signature"));
                    QObject::connect(button, &QToolButton::clicked, button,
                                     [this] { editTimeSignature(); });
                    timeSignatureButtons.removeAll(nullptr);
                    timeSignatureButtons.push_back(button);
                    showTimeSignature();
                    return button;
                });
        }

        // The time signature of the project, 4/4 without a project
        kit::TimeSignature timeSignature() const {
            return document ? kit::ProjectRef(document->session()).settings().timeSignature()
                            : kit::TimeSignature();
        }

        // Shows the time signature of the project in the tool bars and the piano roll. The piano
        // roll is updated only if the time signature differs from the one it shows, because the
        // update refreshes the whole scene.
        void showTimeSignature() {
            const auto shown = timeSignature();
            timeSignatureButtons.removeAll(nullptr);
            const auto text = QStringLiteral("%1/%2").arg(shown.numerator).arg(shown.denominator);
            for (const auto &button : std::as_const(timeSignatureButtons)) {
                button->setText(text);
            }
            if (roll && (roll != timeSignatureRoll || shown != rollTimeSignature)) {
                roll->setTimeSignature(shown.numerator, shown.denominator);
                timeSignatureRoll = roll;
                rollTimeSignature = shown;
            }
        }

        void editTimeSignature() {
            stdc_decl_t;
            QDialog dialog(&decl);
            dialog.setWindowTitle(tr("Time Signature"));
            const auto current = timeSignature();
            auto numerator = new QSpinBox(&dialog);
            numerator->setRange(1, kit::TimeSignature::maximumNumerator);
            numerator->setValue(current.numerator);
            auto denominator = new QComboBox(&dialog);
            for (const int value : kit::TimeSignature::denominators) {
                denominator->addItem(QString::number(value), value);
            }
            denominator->setCurrentIndex(denominator->findData(current.denominator));
            auto form = new QFormLayout(&dialog);
            form->addRow(tr("Beats per bar:"), numerator);
            form->addRow(tr("Beat unit:"), denominator);
            auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                                &dialog);
            form->addRow(buttons);
            QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
            QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }
            kit::ProjectPropertyChanges changes;
            changes.timeSignature =
                kit::TimeSignature{numerator->value(), denominator->currentData().toInt()};
            if (*changes.timeSignature == current) {
                return;
            }
            // The step of the edit shows the time signature.
            kit::DiagnosticList diagnostics;
            kit::ProjectEdits::setProperties(kit::ProjectRef(document->session()), changes,
                                             diagnostics);
            DiagnosticBox::report(&decl, tr("Time Signature"), diagnostics);
        }

        // Shows ticks in the boxes of the quantization, as the piano roll has it.
        void showQuantization(int ticks) {
            quantizationBoxes.removeAll(nullptr);
            for (const auto &box : std::as_const(quantizationBoxes)) {
                const QSignalBlocker blocker(box.data());
                box->setCurrentIndex(box->findData(ticks));
            }
        }

        void showTempo(double tempo) {
            tempoBoxes.removeAll(nullptr);
            for (const auto &box : std::as_const(tempoBoxes)) {
                const QSignalBlocker blocker(box.data());
                box->setValue(tempo);
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

        void goToStart() {
            roll->view()->horizontalScrollBar()->setValue(
                roll->view()->horizontalScrollBar()->minimum());
        }

        void goToEnd() {
            roll->view()->horizontalScrollBar()->setValue(
                roll->view()->horizontalScrollBar()->maximum());
        }

        void goToFirstNote() {
            if (roll->timeline()->noteCount() > 0) {
                roll->showNote(0);
            }
        }

        void goToLastNote() {
            const int count = roll->timeline()->noteCount();
            if (count > 0) {
                roll->showNote(count - 1);
            }
        }

        void zoomTime(double factor) {
            auto view = roll->view();
            view->zoomTime(factor, view->viewport()->width() / 2.0);
        }

        void zoomKeys(double factor) {
            auto view = roll->view();
            view->zoomKeys(factor, view->viewport()->height() / 2.0);
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
            addCommand(QStringLiteral("helloutau.edit.replaceLyrics"), [this] { replaceLyrics(); });
            addCommand(QStringLiteral("helloutau.edit.noteProperties"),
                       [this] { editNoteProperties(); });
            addCommand(QStringLiteral("helloutau.edit.setLabel"), [this] {
                const auto selected = roll->selectedIndices();
                if (!selected.isEmpty()) {
                    editLabel(selected.first());
                }
            });
            addCommand(QStringLiteral("helloutau.edit.nameRegion"), [this] {
                if (const auto range = roll->selectedRange()) {
                    nameRegion(range->first, range->second);
                }
            });
            addCommand(QStringLiteral("helloutau.edit.removeLabel"), [this] {
                edit(tr("Remove Label"), [this](kit::DiagnosticList &diagnostics) {
                    return roll->removeLabels(roll->selectedIndices(), diagnostics);
                });
            });
            addCommand(QStringLiteral("helloutau.edit.removeRegion"), [this] {
                const auto selected = roll->selectedIndices();
                if (selected.isEmpty()) {
                    return;
                }
                edit(tr("Remove Region"), [this, &selected](kit::DiagnosticList &diagnostics) {
                    return roll->removeRegion(selected.first(), diagnostics);
                });
            });
            // An external action: its menu is ours to fill, each time it opens.
            regionMenu = new QMenu(&decl);
            QObject::connect(regionMenu, &QMenu::aboutToShow, &decl,
                             [this] { roll->fillRegionMenu(regionMenu); });
            context->addAction(QStringLiteral("helloutau.view.loadRegion"),
                               regionMenu->menuAction());
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
            addCommand(QStringLiteral("helloutau.select.selectAll"), [this] { roll->selectAll(); });
            addCommand(QStringLiteral("helloutau.view.goToStart"), [this] { goToStart(); });
            addCommand(QStringLiteral("helloutau.view.goToEnd"), [this] { goToEnd(); });
            addCommand(QStringLiteral("helloutau.view.goToFirstNote"),
                       [this] { goToFirstNote(); });
            addCommand(QStringLiteral("helloutau.view.goToLastNote"),
                       [this] { goToLastNote(); });
            addCommand(QStringLiteral("helloutau.view.zoomInTime"),
                       [this] { zoomTime(1.25); });
            addCommand(QStringLiteral("helloutau.view.zoomOutTime"),
                       [this] { zoomTime(1.0 / 1.25); });
            addCommand(QStringLiteral("helloutau.view.zoomInKeys"),
                       [this] { zoomKeys(1.25); });
            addCommand(QStringLiteral("helloutau.view.zoomOutKeys"),
                       [this] { zoomKeys(1.0 / 1.25); });
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
            addCommand(QStringLiteral("helloutau.edit.combineNotes"), [this] {
                edit(tr("Combine Notes"), [this](kit::DiagnosticList &diagnostics) {
                    return roll->combineSelected(diagnostics);
                });
            });
            addCommand(QStringLiteral("helloutau.edit.splitNote"), [this] { splitNote(); });
            addCommand(QStringLiteral("helloutau.edit.pitchControl"),
                       [this] { editPitchControl(); });
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
            const auto selectTool = addCommand(QStringLiteral("helloutau.select.selectTool"),
                                               [this] { roll->setTool(PianoRoll::SelectTool); });
            const auto penTool = addCommand(QStringLiteral("helloutau.select.penTool"),
                                            [this] { roll->setTool(PianoRoll::PenTool); });
            const auto pitchTool = addCommand(QStringLiteral("helloutau.select.pitchTool"),
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
            addCommand(QStringLiteral("helloutau.view.timeSignature"),
                       [this] { editTimeSignature(); });
            addCommand(QStringLiteral("helloutau.select.decreaseQuantizationInterval"),
                       [this] { stepQuantization(true); });
            addCommand(QStringLiteral("helloutau.select.increaseQuantizationInterval"),
                       [this] { stepQuantization(false); });
            addCommand(QStringLiteral("helloutau.view.commandPalette"), [this] {
                palette->setCommands(commandEntries());
                palette->setRecentIds(editor->settings().recentCommands());
                palette->popup();
            });
            const auto play =
                addCommand(QStringLiteral("helloutau.playback.play"), [this] { togglePlayback(); });
            playIconToggle = new ActionIconToggle(play);
            addCommand(QStringLiteral("helloutau.playback.stop"), [this] { playback->stop(); });
            addCommand(QStringLiteral("helloutau.playback.replay"), [this] { replay(); });
            addCommand(QStringLiteral("helloutau.playback.saveLastPlayed"),
                       [this] { saveLastPlayed(); });
            addCommand(QStringLiteral("helloutau.playback.renderTrack"), [this] { renderTrack(); });
            addCommand(QStringLiteral("helloutau.tools.clearCache"), [this] { clearCache(); });
            addCommand(QStringLiteral("helloutau.tools.viewRenderLog"),
                       [this] { showRenderLog(); });
            addCommand(QStringLiteral("helloutau.tools.settings"), [this] {
                stdc_decl_t;
                editor->showSettings(&decl);
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
            const auto at = FindMatches::adjacentMatch(
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
                    !setLyrics(
                        {
                            {index, search.replaced(lyric, findBar->replacement())}
                },
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
                    DiagnosticBox::report(&decl, title, diagnostics);
                    return false;
                }
            }
            const bool committed = transaction.commit(diagnostics);
            DiagnosticBox::report(&decl, title, diagnostics);
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
            const auto quantization =
                roll ? roll->quantization() : editor->settings().quantization();
            const QPointer<PianoRoll> previousRoll = roll;
            roll = new PianoRoll(document->session());
            const auto updateModifiers = [this] {
                for (const auto &bindings : editor->modifierBindings(Editor::ProjectWindowKind)) {
                    roll->setModifierBindings(bindings);
                }
            };
            updateModifiers();
            QObject::connect(editor, &Editor::modifierBindingsChanged, roll,
                             [updateModifiers](Editor::WindowKind kind) {
                                 if (kind == Editor::ProjectWindowKind) {
                                     updateModifiers();
                                 }
                             });
            roll->setVoiceBank(document->voiceBank());
            showTimeSignature();
            QObject::connect(roll, &PianoRoll::voiceBankRequested, &decl,
                             [this] { editProperties(); });
            if (quantization >= 0) {
                roll->setQuantization(quantization);
            }
            showQuantization(roll->quantization());
            showTempo(kit::ProjectRef(document->session()).settings().tempo());
            QObject::connect(roll, &PianoRoll::quantizationChanged, &decl, [this](int ticks) {
                editor->settings().setQuantization(ticks);
                showQuantization(ticks);
            });
            if (actions.value(QStringLiteral("helloutau.select.penTool"))->isChecked()) {
                roll->setTool(PianoRoll::PenTool);
            } else if (actions.value(QStringLiteral("helloutau.select.pitchTool"))->isChecked()) {
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
            // setCentralWidget() only schedules the deletion of the previous piano roll, which a
            // nested event loop, such as that of a message box, does not carry out. The piano roll
            // is deleted before its document, so that no event of it reaches a session that is
            // gone.
            delete previousRoll;

            QObject::connect(document.get(), &kit::ProjectDocument::voiceBankChanged, roll, [this] {
                roll->setVoiceBank(document->voiceBank());
                updateBackground();
            });
            QObject::connect(document->session(), &kit::edit::EditSession::changed, &decl, [this] {
                showTempo(kit::ProjectRef(document->session()).settings().tempo());
                updateTimeStatus();
            });
            QObject::connect(roll, &PianoRoll::selectionChanged, &decl, [this] {
                updateEditActions();
                updateFindResult();
                updateTimeStatus();
            });
            findBar->setAnchor(roll->view());
            QObject::connect(findBar, &FindBar::closed, roll,
                             [this] { roll->setLyricSearch(kit::TextSearch()); });
            // In the status bar, so that a refused drag does not stop the work with a dialog.
            // A dialog remains an alternative, see the open questions in docs/Tuning.md.
            QObject::connect(roll, &PianoRoll::cursorMoved, &decl, [this] {
                cursorMoved();
                updateTimeStatus();
            });
            QObject::connect(roll, &PianoRoll::tempoRequested, &decl,
                             [this](int index) { editTempo(index); });
            QObject::connect(roll, &PianoRoll::labelRequested, &decl,
                             [this](int index) { editLabel(index); });
            QObject::connect(roll, &PianoRoll::regionRequested, &decl,
                             [this](int first, int last) { nameRegion(first, last); });
            QObject::connect(roll, &PianoRoll::regionEditRequested, &decl,
                             [this](const kit::Region &region) { editRegion(region); });
            QObject::connect(
                roll, &PianoRoll::regionsRequested, &decl,
                [this](const std::optional<kit::Region> &current) { editRegions(current); });
            QObject::connect(roll, &PianoRoll::editRefused, &decl, [this](const QString &message) {
                stdc_decl_t;
                decl.statusBar()->showMessage(message, StatusMessageTimeout);
            });
            updateEditActions();
            updateTimeStatus();
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
                showTimeSignature();
                // The voice folder is read again after an undo or redo that changes it to another
                // directory. The read is queued so that it does not run inside the notification
                // of the step.
                if (!voiceBankReloadPending && voiceRoot() != voiceBankRoot &&
                    !sameVoiceRoot(voiceRoot(), voiceBankRoot)) {
                    voiceBankReloadPending = true;
                    QMetaObject::invokeMethod(
                        _decl,
                        [this] {
                            voiceBankReloadPending = false;
                            if (!sameVoiceRoot(voiceRoot(), voiceBankRoot)) {
                                _decl->loadVoiceBank();
                            }
                        },
                        Qt::QueuedConnection);
                }
                // If Project Properties or an undo has changed the synth tools, the render with the
                // previous synth tools is stopped at once. The background render then starts again
                // with the new synth tools if they may run.
                if (synthToolTexts() != backgroundSynthTools) {
                    playback->release();
                    updateBackground();
                }
                // A prerender plays the notes as they were rendered, so an edit stops it, and the
                // next play renders the notes again.
                if (!realtime() && (playback->state() == Playback::Playing ||
                                    playback->state() == Playback::Paused)) {
                    playback->stop();
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
            const auto pitchTool = actions.value(QStringLiteral("helloutau.select.pitchTool"));
            pitchTool->setEnabled(
                !mode2 && actions.value(QStringLiteral("helloutau.view.showPitch"))->isChecked());
            if (!pitchTool->isEnabled() && pitchTool->isChecked()) {
                actions.value(QStringLiteral("helloutau.select.selectTool"))->setChecked(true);
                roll->setTool(PianoRoll::SelectTool);
            }
            updatePitchControlAction();
        }

        // Pitch Control edits the Mode2 points and the vibrato of the selected notes, and is
        // available only in Mode2.
        void updatePitchControlAction() {
            actions.value(QStringLiteral("helloutau.edit.pitchControl"))
                ->setEnabled(kit::ProjectRef(document->session()).settings().mode2() &&
                             !roll->selectedIndices().isEmpty());
        }

        void updateTitle() {
            stdc_decl_t;
            decl.setWindowTitle(
                QStringLiteral("%1[*] - HelloUtau").arg(editor->projectDisplayName(&decl)));
            decl.setWindowModified(document->isModified());
        }

        // Enables the commands that act on the selection when there is one.
        void updateEditActions() {
            const int selected = int(roll->selectedIndices().size());
            for (const auto id :
                 {"helloutau.edit.delete", "helloutau.edit.editLyric", "helloutau.edit.scalePitch",
                  "helloutau.edit.convertPitchToMode1", "helloutau.edit.crossfadeP2P3",
                  "helloutau.edit.crossfadeP1P4", "helloutau.edit.copy",
                  "helloutau.edit.resetPortamento", "helloutau.edit.resetVibratos",
                  "helloutau.edit.resetEnvelopes", "helloutau.edit.resetAll",
                  "helloutau.edit.transposeUp", "helloutau.edit.transposeDown",
                  "helloutau.edit.octaveUp", "helloutau.edit.octaveDown", "helloutau.edit.setTempo",
                  "helloutau.edit.noteProperties", "helloutau.edit.replaceLyrics"}) {
                actions.value(QLatin1String(id))->setEnabled(selected > 0);
            }
            updatePitchControlAction();
            actions.value(QStringLiteral("helloutau.edit.splitNote"))->setEnabled(selected == 1);
            actions.value(QStringLiteral("helloutau.edit.setLabel"))->setEnabled(selected > 0);
            actions.value(QStringLiteral("helloutau.edit.nameRegion"))
                ->setEnabled(roll->selectedRange().has_value());
            // A label to remove among the selected notes, a region around the first of them
            const auto indices = roll->selectedIndices();
            const auto notes = kit::ProjectRef(document->session()).tracks().at(0).notes();
            const bool labeled = std::any_of(indices.begin(), indices.end(), [&notes](int i) {
                return !notes.at(i).label().isEmpty();
            });
            actions.value(QStringLiteral("helloutau.edit.removeLabel"))->setEnabled(labeled);
            actions.value(QStringLiteral("helloutau.edit.removeRegion"))
                ->setEnabled(!indices.isEmpty() && roll->regionAt(indices.first()).has_value());
            actions.value(QStringLiteral("helloutau.edit.combineNotes"))->setEnabled(selected > 1);
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
            DiagnosticBox::report(&decl, title, diagnostics);
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
            DiagnosticBox::report(&decl, tr("Paste Parameters"), diagnostics);
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
            DiagnosticBox::report(&decl, tr("Scale Pitch"), diagnostics);
        }

        // Edits the portamento and vibrato of the selected sung notes, starting from the first
        // vibrato or the default.
        void editPitchControl() {
            stdc_decl_t;
            if (roll->lyricEditor()->isVisible()) {
                return;
            }
            const auto refs = kit::ProjectRef(document->session()).tracks().at(0).notes();
            QList<kit::NoteRef> sung;
            QList<int> selected;
            for (const int index : roll->selectedIndices()) {
                if (!roll->timeline()->note(index).rest) {
                    sung.push_back(refs.at(index));
                    selected.push_back(index);
                }
            }
            if (sung.isEmpty()) {
                return;
            }
            const auto portamento = roll->selectedPortamento();
            const auto vibratoState = roll->selectedVibrato();
            const auto &settings = editor->settings();
            // The dialog starts from the first note: its points, and its vibrato or the default.
            PitchControlDialog::Selection selection;
            selection.portamento = portamento;
            selection.vibrato = vibratoState;
            selection.vibratoValues =
                sung.first().vibrato().value_or(settings.pitchControlVibrato());
            selection.vibratoPreset = settings.pitchControlVibratoPreset();
            const auto pointRefs = sung.first().portamento();
            for (int i = 0; i < pointRefs.size(); ++i) {
                selection.points.push_back(pointRefs.at(i).toPortamentoPoint());
            }
            // The settings of the default, changed to show the points of the first note
            auto &shown = selection.portamentoSettings;
            shown = settings.pitchControlPortamento();
            if (selection.points.size() == 2) {
                shown.mode = kit::PortamentoSettings::Custom;
                shown.start = int(std::lround(selection.points.first().x));
                shown.length =
                    int(std::lround(selection.points.last().x - selection.points.first().x));
            } else if (selection.points.size() > 2) {
                shown.mode = kit::PortamentoSettings::AddPoints;
                shown.count = int(selection.points.size());
            }
            const auto &timeline = *roll->timeline();
            const auto durationOf = [&timeline](int index) {
                const auto &note = timeline.note(index);
                const auto &tempoMap = timeline.tempoMap();
                return tempoMap.timeOf(note.start + note.length) - tempoMap.timeOf(note.start);
            };
            const int first = selected.first();
            selection.duration = durationOf(first);
            if (first > 0) {
                selection.previousDuration = durationOf(first - 1);
            }
            PitchControlDialog dialog(selection, &decl);
            connect(dialog.portamentoDefaultButton(), &QPushButton::clicked, &dialog,
                    [this, &dialog] {
                        editor->settings().setPitchControlPortamento(dialog.portamentoSettings());
                    });
            connect(dialog.vibratoDefaultButton(), &QPushButton::clicked, &dialog, [this, &dialog] {
                editor->settings().setPitchControlVibrato(dialog.vibrato());
                editor->settings().setPitchControlVibratoPreset(dialog.vibratoPreset());
            });
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }
            kit::DiagnosticList diagnostics;
            // The portamento, the vibrato switch and the vibrato change in one undo step.
            auto transaction = document->session()->transaction(tr("Pitch Control"));
            const bool portamentoEdited = dialog.portamentoEdited();
            if (dialog.portamentoState() == Qt::Checked &&
                (portamentoEdited || !portamento || !*portamento)) {
                roll->setPortamentoEnabled(true, diagnostics, dialog.portamentoPoints());
            } else if (dialog.portamentoState() == Qt::Unchecked && portamentoEdited &&
                       (!portamento || *portamento)) {
                roll->setPortamentoEnabled(false, diagnostics);
            }
            // As in UTAU, a change gives every note the vibrato of the dialog, also if their
            // vibratos differ. A vibrato that every note shares stays unless a field is edited.
            if (dialog.vibratoState() == Qt::Checked &&
                (vibratoState != std::optional(true) || dialog.vibratoEdited())) {
                kit::ProjectEdits::setVibrato(sung, dialog.vibrato(), diagnostics);
            } else if (dialog.vibratoState() == Qt::Unchecked &&
                       vibratoState != std::optional(false)) {
                kit::ProjectEdits::setVibrato(sung, std::nullopt, diagnostics);
            }
            transaction.commit(diagnostics);
            DiagnosticBox::report(&decl, tr("Pitch Control"), diagnostics);
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
            const auto root = track.voiceDirectory(editor->settings().voiceLocations());
            if (track.voiceDir.isEmpty() || root.empty()) {
                QMessageBox::information(
                    &decl, tr("Edit Voice Bank"),
                    track.voiceDir.isEmpty()
                        ? tr("The project names no voice bank.")
                        : tr("The voice bank \"%1\" cannot be located, because no folder against "
                             "which it is resolved exists.")
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
            QMessageBox box(QMessageBox::Warning, tr("HelloUtau"),
                            tr("Save the changes to %1?").arg(editor->projectDisplayName(_decl)),
                            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, &decl);
            box.button(QMessageBox::Discard)->setText(tr("Do not save"));
            const auto answer = box.exec();
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
        resize(1200, 800);
    }

    ProjectWindow::~ProjectWindow() {
        stdc_impl_t;
        // Stopping updates the piano roll, which refers to the session of the document, which
        // goes with _impl. The playback ends its workers before the temporary directory goes
        // with _impl as well.
        impl.playback->stopAndWait();
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

    void ProjectWindow::refreshTitle() {
        stdc_impl_t;
        impl.updateTitle();
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
        // The workers end before the previous temporary directory is removed.
        impl.playback->stopAndWait();
        const auto temporaryDirectory = impl.newTemporaryDirectory();
        impl.playback->setTemporaryDirectory(temporaryDirectory);
        impl.voiceBankRoot.clear();
        impl.voiceBankReloadPending = false;
        auto previous = std::move(impl.document);
        impl.document = std::move(document);
        impl.bindDocument();
        Q_EMIT documentChanged();
    }

    void ProjectWindow::applySettings() {
        stdc_impl_t;
        impl.renderLog->setMode(impl.editor->settings().isRenderLogAccumulated()
                                    ? kit::SynthToolOutputLog::Accumulated
                                    : kit::SynthToolOutputLog::Latest);
        impl.renderLog->setLimit(impl.editor->settings().renderLogLimit());
        if (!impl.editor->settings().isRenderLogAccumulated()) {
            impl.renderLog->clear();
        }
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
        if (!impl.voicePathValid()) {
            return false;
        }
        const auto document = impl.document.get();
        impl.voiceBankRoot = impl.voiceRoot();
        VoiceBankCharsetDialog selector(this);
        selector.setRoot(impl.voiceBankRoot);
        kit::DiagnosticList diagnostics;
        const bool loaded = document->loadVoiceBank(impl.editor->settings().voiceLocations(),
                                                    &selector, diagnostics);
        DiagnosticBox::report(this, tr("Voice Bank"), diagnostics);
        return loaded;
    }

    void ProjectWindow::showPropertiesIfPathsAreInvalid() {
        stdc_impl_t;
        if (!impl.voicePathValid() || !impl.synthToolPathsValid()) {
            impl.editProperties();
        }
    }

    bool ProjectWindow::save() {
        stdc_impl_t;
        if (impl.document->filePath().empty()) {
            return saveAs();
        }
        kit::DiagnosticList diagnostics;
        const bool saved = impl.document->save(diagnostics);
        DiagnosticBox::report(this, tr("Save"), diagnostics);
        return saved;
    }

    bool ProjectWindow::saveAs() {
        stdc_impl_t;
        const auto file = QFileDialog::getSaveFileName(
            this, tr("Save As"),
            textOf(proposedPath(*impl.document, u".usth", impl.projectFileBaseName())),
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
        DiagnosticBox::report(this, tr("Save As"), diagnostics);
        if (saved) {
            impl.editor->settings().addRecentFile(path);
        }
        return saved;
    }

    bool ProjectWindow::maybeSave() {
        stdc_impl_t;
        return impl.maybeSave();
    }

    bool ProjectWindow::exportUst() {
        stdc_impl_t;
        const auto &settings = impl.editor->settings();
        ExportUstDialog dialog(
            proposedPath(*impl.document, u".ust", impl.projectFileBaseName()),
            settings.ustExportCharset(), this);
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
        DiagnosticBox::report(this, tr("Export UST"), diagnostics);
        return exported;
    }

    void ProjectWindow::closeEvent(QCloseEvent *event) {
        stdc_impl_t;
        if (!impl.maybeSave()) {
            event->ignore();
            return;
        }
        impl.playback->stopAndWait();
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
            impl.editor->openFiles(files, this);
        });
    }

}
