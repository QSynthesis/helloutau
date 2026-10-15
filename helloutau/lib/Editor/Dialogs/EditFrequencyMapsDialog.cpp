#include "EditFrequencyMapsDialog.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <optional>

#include <QtCore/QDir>
#include <QtCore/QEventLoop>
#include <QtCore/QHash>
#include <QtCore/QSignalBlocker>
#include <QtCore/QTemporaryDir>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtGui/QActionGroup>
#include <QtGui/QScreen>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include <stdcorelib/pimpl.h>

#include <stdutau/utautils.h>

#include <QAKCore/actionregistry.h>
#include <QAKWidgets/widgetactioncontext.h>

#include <hellokit/AudioFile/Spectrogram.h>
#include <hellokit/AudioFile/WaveAudio.h>
#include <hellokit/Support/TemporaryStorage.h>
#include <hellokit/VoiceBank/FrequencyFormats.h>
#include <hellokit/VoiceBank/FrequencyMapRevisionBuffer.h>

#include <helloutau/Widgets/ToolBarPalette.h>

#include "AppSettings.h"
#include "DiagnosticBox.h"
#include "Editor.h"
#include "FrequencyMapView.h"
#include "FrequencyMapWaveform.h"
#include "FrequencyViewKeyboardModifiers.h"
#include "SamplePreview.h"

namespace hello::daw {

    namespace {

        QString textOf(const std::filesystem::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

        // Returns the name of the audio file for display, with a misdecoded name restored.
        QString shownNameOf(const EditFrequencyMapsDialog::File &file) {
            return textOf((file.shownPath.empty() ? file.path : file.shownPath).filename());
        }

        // Returns the text of a frequency in the list and the edits, as frqeditor writes it,
        // or ---- for an unvoiced frame.
        QString frequencyText(double frequency) {
            return frequency > kit::FrequencyMapRevisionBuffer::lowestFrequency
                       ? QString::asprintf("%.8f", frequency)
                       : QStringLiteral("----");
        }

        double noteOf(double frequency) {
            return 69 + 12 * std::log2(frequency / 440);
        }

        QString presence(bool present) {
            return present ? QString(QChar(0x2713)) : QString();
        }

        // The size and the time of the last write of a file, or nothing if the file is absent
        struct FileStamp {
            std::optional<std::uintmax_t> size;
            std::filesystem::file_time_type time;

            static FileStamp of(const std::filesystem::path &path) {
                FileStamp stamp;
                std::error_code error;
                const auto size = std::filesystem::file_size(path, error);
                if (error) {
                    return stamp;
                }
                stamp.size = size;
                stamp.time = std::filesystem::last_write_time(path, error);
                return stamp;
            }

            bool operator==(const FileStamp &other) const {
                return size == other.size && (!size || time == other.time);
            }
        };

    }

    class EditFrequencyMapsDialog::Impl {
    public:
        using Decl = EditFrequencyMapsDialog;

        Impl(Decl *decl, Editor *editor, const kit::FrequencyFormat &format, QList<File> files)
            : _decl(decl), editor(editor), format(format), files(std::move(files)) {
        }

        Decl *_decl;
        Editor *editor;
        const kit::FrequencyFormat &format;
        QList<File> files;

        // A map that was read, with what its save requires
        struct Opened {
            std::unique_ptr<kit::FrequencyMapRevisionBuffer> buffer;
            int sampleRate = 0;
            // The map file as it was read or last written
            FileStamp stamp;
            // The corrections of the last check, empty for none. They stay until adopted,
            // ignored or replaced by another check, also if the frames change.
            std::vector<kit::FrequencyMapCheck::Correction> corrections;
        };
        std::map<int, Opened> opened;
        int current = -1;
        // The columns of the number of corrections and of the modified mark, after the columns
        // of the caches
        int correctionColumn = 0;
        int modifiedColumn = 0;
        // The direction in which the last command moved between the corrections
        bool correctionForward = true;

        QTreeWidget *list = nullptr;
        FrequencyMapView *view = nullptr;
        FrequencyMapWaveform *waveform = nullptr;
        QToolBar *toolBar = nullptr;
        // Moves the playhead of the waveform while the preview plays
        QTimer *playheadTimer = nullptr;
        QLabel *message = nullptr;
        QCheckBox *removeCaches = nullptr;
        QAK::WidgetActionContext *context = nullptr;

        // The right side of the window of frqeditor
        QLineEdit *keyEdit = nullptr;
        QPushButton *updateKeyButton = nullptr;
        QListWidget *frameList = nullptr;
        QLineEdit *frequencyEdit = nullptr;
        QPushButton *setButton = nullptr;
        QSlider *pitchSlider = nullptr;
        QLabel *pitchLabel = nullptr;
        QLineEdit *modulationEdit = nullptr;
        QLineEdit *flagsEdit = nullptr;
        QCheckBox *directBox = nullptr;
        QPushButton *playButton = nullptr;
        QLabel *wavInfo = nullptr;
        // The buttons of the row below, with the actions that they trigger
        QList<std::pair<QToolButton *, QString>> editButtons;
        SamplePreview *preview = nullptr;
        // The folder of the audio file and the map of the preview
        std::unique_ptr<QTemporaryDir> previewDirectory;
        // The buffer whose frames the list shows
        const kit::FrequencyMapRevisionBuffer *listed = nullptr;

        // Shows the key, the frames and the current frequency of the current buffer.
        void refreshPanel() {
            const auto buffer = currentBuffer();
            {
                const QSignalBlocker blocker(keyEdit);
                keyEdit->setText(buffer ? frequencyText(buffer->key()) : QString());
            }
            const bool keyEditable = buffer && buffer->map().averageFrequency.has_value();
            keyEdit->setEnabled(keyEditable);
            updateKeyButton->setEnabled(keyEditable);

            static const std::vector<kit::FrequencyMap::Frame> none;
            const auto &frames = buffer ? buffer->map().frames : none;
            {
                const QSignalBlocker blocker(frameList);
                if (listed != buffer || frameList->count() != int(frames.size())) {
                    frameList->clear();
                    for (std::size_t i = 0; i < frames.size(); ++i) {
                        frameList->addItem(QString());
                    }
                    listed = buffer;
                }
                for (std::size_t i = 0; i < frames.size(); ++i) {
                    const auto item = frameList->item(int(i));
                    const auto text = frequencyText(frames[i].frequency);
                    if (item->text() != text) {
                        item->setText(text);
                    }
                }
                frameList->clearSelection();
                if (const auto selection = view->selection()) {
                    const auto first = frameList->model()->index(int(selection->first), 0);
                    const auto last =
                        frameList->model()->index(int(selection->first + selection->second - 1), 0);
                    frameList->selectionModel()->select(QItemSelection(first, last),
                                                        QItemSelectionModel::Select);
                    frameList->scrollToItem(frameList->item(int(selection->first)));
                }
            }
            updateFrequencyEdit();
            playButton->setEnabled(audio != nullptr);
        }

        void updateFrequencyEdit() {
            const auto buffer = currentBuffer();
            const int row = frameList->currentRow();
            const bool valid = buffer && row >= 0 && std::size_t(row) < buffer->map().frames.size();
            frequencyEdit->setText(
                valid ? frequencyText(buffer->map().frames[std::size_t(row)].frequency)
                      : QString());
            frequencyEdit->setEnabled(valid);
            setButton->setEnabled(valid);
        }

        // Selects the frames of the rows that the list selects, which are contiguous.
        void selectListedFrames() {
            const auto rows = frameList->selectionModel()->selectedRows();
            if (rows.isEmpty()) {
                view->clearSelection();
                return;
            }
            int first = rows.front().row();
            int last = first;
            for (const auto &index : rows) {
                first = std::min(first, index.row());
                last = std::max(last, index.row());
            }
            view->setSelection(std::size_t(first), std::size_t(last - first + 1));
        }

        // Returns the text of the information of the audio file of the current file.
        QString wavInfoText() {
            if (!audio) {
                return QString();
            }
            QStringList lines{tr("Sample rate: %1 Hz").arg(audio->sampleRate),
                              tr("Channels: %1").arg(audio->channels),
                              tr("Length: %1 ms").arg(std::lround(audio->duration()))};
            if (const auto buffer = currentBuffer()) {
                lines.push_back(tr("Frames: %1").arg(buffer->map().frames.size()));
                lines.push_back(tr("Hop: %1 samples").arg(buffer->map().hop));
            }
            return lines.join(QLatin1Char('\n'));
        }

        // Moves the playhead of the waveform to the time heard: the time of the audio file, or
        // the time of the synthesized note, which spans the audio file at about the same pace.
        void movePlayhead() {
            auto time = preview->position();
            if (!time) {
                time = preview->notePosition();
            }
            waveform->setPlayhead(time);
        }

        // Synthesizes the audio file at the key with the map as edited, as the preview of
        // frqeditor does, or plays the audio file as it is with $direct. Stops a preview in
        // progress instead.
        void play() {
            stdc_decl_t;
            if (preview->state() != SamplePreview::Stopped) {
                preview->stop();
                return;
            }
            const auto title = tr("Preview");
            kit::DiagnosticList diagnostics;
            if (!audio) {
                return;
            }
            if (directBox->isChecked()) {
                if (!preview->play(audio, 0, std::nullopt, diagnostics)) {
                    DiagnosticBox::report(&decl, title, diagnostics);
                }
                return;
            }
            const auto entry = openedOf(current);
            if (!entry) {
                return;
            }
            if (!previewDirectory) {
                previewDirectory = std::make_unique<QTemporaryDir>(
                    kit::TemporaryStorage::templatePath(QStringLiteral("frequency-preview")));
            }
            const std::filesystem::path folder(previewDirectory->path().toStdU16String());
            const auto &file = files.at(current);
            const auto name = file.path.filename();
            std::error_code error;
            std::filesystem::copy_file(file.path, folder / name,
                                       std::filesystem::copy_options::overwrite_existing, error);
            if (error) {
                QMessageBox::warning(&decl, title,
                                     tr("The audio file cannot be copied for the preview."));
                return;
            }
            // The map as edited beside the copy, and no cache of an earlier preview
            kit::FrequencyFormat::MapWrite write;
            write.wav = name;
            write.map = entry->buffer->map();
            write.sampleRate = entry->sampleRate;
            if (!kit::FrequencyFormats::writeMaps(format, folder, {write}, true, diagnostics)) {
                DiagnosticBox::report(&decl, title, diagnostics);
                return;
            }
            kit::VoiceSample sample;
            sample.path = folder / name;
            sample.fileName = textOf(name);
            sample.hasEntry = true;
            const int noteNum = std::clamp(
                int(std::lround(noteOf(entry->buffer->key()))) + pitchSlider->value(), 0, 127);
            // The whole audio file at 120 beats per minute, 480 ticks for 500 milliseconds
            const int length = std::max(1, int(std::lround(audio->duration() * 480 / 500)));
            SamplePreview::NoteOptions options;
            options.flags = flagsEdit->text();
            bool valid = false;
            const int modulation = modulationEdit->text().toInt(&valid);
            // 100 percent without a number, as in frqeditor
            options.modulation = valid ? std::clamp(modulation, -200, 200) : 100;
            const std::filesystem::path resampler(
                editor->settings().rendering().resampler().toStdU16String());
            if (!preview->synthesize(sample, noteNum, length, resampler, options, diagnostics)) {
                DiagnosticBox::report(&decl, title, diagnostics);
            }
        }
        QHash<QString, QAction *> actions;

        // Returns the map file of the file at index in its folder.
        std::optional<std::filesystem::path> mapFileOf(int index) const {
            const auto &path = files.at(index).path;
            const auto name = format.mapFileNameOf(path.filename());
            if (!name) {
                return std::nullopt;
            }
            return path.parent_path() / *name;
        }

        Opened *openedOf(int index) {
            const auto it = opened.find(index);
            return it == opened.end() ? nullptr : &it->second;
        }

        kit::FrequencyMapRevisionBuffer *currentBuffer() {
            const auto entry = openedOf(current);
            return entry ? entry->buffer.get() : nullptr;
        }

        QAction *spectrumAction() const {
            return actions.value(QStringLiteral("helloutau.frequencyMap.showSpectrogram"));
        }

        // Runs work in a thread while a progress dialog that appears after progressDelay
        // cancels it. work receives the function that returns whether it is canceled. Returns
        // whether the work completed.
        bool runInBackground(const QString &label,
                             const std::function<void(const std::function<bool()> &)> &work) {
            stdc_decl_t;
            QProgressDialog progress(label, tr("Cancel"), 0, 0, &decl);
            progress.setWindowTitle(decl.windowTitle());
            progress.setWindowModality(Qt::WindowModal);
            // Shown by the delay below only
            progress.setMinimumDuration(std::numeric_limits<int>::max());
            std::atomic<bool> canceled = false;
            QObject::connect(&progress, &QProgressDialog::canceled, &decl,
                             [&canceled] { canceled = true; });
            if (progressDelay <= 0) {
                progress.show();
            } else {
                QTimer::singleShot(progressDelay, &progress, [&progress] { progress.show(); });
            }
            QEventLoop loop;
            QThread *worker = nullptr;
            // Started once the event loop runs, so that nothing is read if a cancellation comes
            // first, such as a click on the dialog just shown.
            QTimer::singleShot(0, &loop, [&] {
                worker = QThread::create([&] {
                    if (!canceled.load()) {
                        work([&canceled] { return canceled.load(); });
                    }
                });
                QObject::connect(worker, &QThread::finished, &loop, &QEventLoop::quit);
                worker->start();
            });
            loop.exec();
            worker->wait();
            delete worker;
            progress.reset();
            return !canceled.load();
        }

        // Reads the audio of the file at index, its spectrogram if shown and its map if not read
        // yet in the background, then shows the file in the editing area, or explains in the
        // message why it cannot be edited. Returns false if the user cancels the reading, which
        // keeps the current file.
        bool show(int index) {
            stdc_decl_t;
            const auto &file = files.at(index);
            const bool readsMap = file.hasMap && !openedOf(index);
            const bool showsSpectrum = view->viewMode() == FrequencyMapView::SpectrumView;
            std::shared_ptr<const kit::WaveAudio> read;
            std::shared_ptr<const kit::Spectrogram> spectrum;
            std::optional<kit::FrequencyMap> map;
            kit::DiagnosticList audioDiagnostics;
            kit::DiagnosticList mapDiagnostics;
            const bool completed = runInBackground(
                tr("Reading the audio file and its frequency map..."),
                [&](const std::function<bool()> &canceled) {
                    auto wave = kit::WaveAudio::read(file.path, audioDiagnostics);
                    if (!wave) {
                        return;
                    }
                    read = std::make_shared<const kit::WaveAudio>(std::move(*wave));
                    if (showsSpectrum) {
                        auto computed = kit::Spectrogram::of(*read, canceled);
                        if (!computed) {
                            return;
                        }
                        spectrum = std::make_shared<const kit::Spectrogram>(std::move(*computed));
                    }
                    if (readsMap && !canceled()) {
                        map = format.read(file.path, read->sampleRate, mapDiagnostics);
                    }
                });
            if (!completed) {
                {
                    const QSignalBlocker blocker(list);
                    list->setCurrentItem(current >= 0 ? list->topLevelItem(current) : nullptr);
                }
                QMessageBox::warning(&decl, decl.windowTitle(),
                                     tr("The reading of %1 was canceled. The file in the editing "
                                        "area does not change.")
                                         .arg(shownNameOf(file)));
                return false;
            }

            // A map left without a history and without corrections is read again when it is
            // shown again.
            if (const auto left = openedOf(current);
                left && current != index && !left->buffer->canUndo() && !left->buffer->canRedo() &&
                left->corrections.empty()) {
                opened.erase(current);
            }
            if (current != index) {
                preview->stop();
            }
            current = index;
            {
                const QSignalBlocker blocker(list);
                list->setCurrentItem(list->topLevelItem(index));
            }
            audio = read;
            waveform->setAudio(read);
            spectrumSource = spectrum ? read : nullptr;
            view->setSpectrogram(spectrum);
            QString reason;
            if (!openedOf(index)) {
                if (!file.hasMap) {
                    reason = tr("This audio file has no frequency map in the format %1.")
                                 .arg(format.name());
                } else if (!read) {
                    reason = tr("The audio file cannot be read: %1")
                                 .arg(audioDiagnostics.value(0).message);
                } else if (!map) {
                    reason = tr("The frequency map cannot be read: %1")
                                 .arg(mapDiagnostics.value(0).message);
                } else {
                    Opened entry;
                    entry.buffer =
                        std::make_unique<kit::FrequencyMapRevisionBuffer>(std::move(*map));
                    entry.sampleRate = read->sampleRate;
                    if (const auto mapFile = mapFileOf(index)) {
                        entry.stamp = FileStamp::of(*mapFile);
                    }
                    opened.emplace(index, std::move(entry));
                }
            }
            if (!reason.isEmpty()) {
                message->setText(reason + QLatin1Char(' ') +
                                 tr("Tools > Generate Frequency Maps in the voice bank window "
                                    "generates the frequency map."));
            }
            message->setVisible(!reason.isEmpty());
            view->setTitle(shownNameOf(file));
            view->setBuffer(currentBuffer());
            if (const auto entry = openedOf(index)) {
                view->setCorrections(entry->corrections);
            }
            wavInfo->setText(wavInfoText());
            updateActions();
            return true;
        }

        // The audio of the current file, if it can be read, kept to compute its spectrogram when
        // the spectrogram is shown again
        std::shared_ptr<const kit::WaveAudio> audio;
        // The milliseconds after which the progress dialog of a reading appears
        int progressDelay = 400;
        // The file made current once the dialog is shown
        int initial = -1;
        bool shownOnce = false;

        // Computes the spectrogram of the current file in the background if the view shows it
        // and it is not computed yet, and shows the rows of the keys again if the user cancels.
        void updateSpectrogram() {
            stdc_decl_t;
            const QSignalBlocker blocker(spectrumAction());
            spectrumAction()->setChecked(view->viewMode() == FrequencyMapView::SpectrumView);
            if (!audio || view->viewMode() != FrequencyMapView::SpectrumView ||
                spectrumSource == audio) {
                return;
            }
            const auto source = audio;
            std::shared_ptr<const kit::Spectrogram> spectrum;
            if (!runInBackground(
                    tr("Computing the spectrogram..."), [&](const std::function<bool()> &canceled) {
                        if (auto computed = kit::Spectrogram::of(*source, canceled)) {
                            spectrum =
                                std::make_shared<const kit::Spectrogram>(std::move(*computed));
                        }
                    })) {
                view->setViewMode(FrequencyMapView::KeyRowsView);
                QMessageBox::warning(&decl, decl.windowTitle(),
                                     tr("The computation of the spectrogram was canceled."));
                return;
            }
            spectrumSource = audio;
            view->setSpectrogram(spectrum);
        }

        // The audio that the spectrogram of the view was computed from
        std::shared_ptr<const kit::WaveAudio> spectrumSource;

        // Checks the action of the color scheme of the view.
        void updateColorActions() {
            for (int i = 0; i < FrequencyMapView::colorSchemeCount; ++i) {
                if (const auto action = actions.value(QLatin1String(colorActionIds[i]))) {
                    action->setChecked(i == view->colorScheme());
                }
            }
            if (const auto action =
                    actions.value(QStringLiteral("helloutau.frequencyMap.brighten"))) {
                action->setEnabled(view->brightness() > FrequencyMapView::lowestBrightness);
            }
            if (const auto action =
                    actions.value(QStringLiteral("helloutau.frequencyMap.darken"))) {
                action->setEnabled(view->brightness() < FrequencyMapView::highestBrightness);
            }
        }

        // The actions of the color schemes, in the order of FrequencyMapView::colorScheme()
        static constexpr const char *colorActionIds[FrequencyMapView::colorSchemeCount] = {
            "helloutau.frequencyMap.coolColors",  "helloutau.frequencyMap.purpleColors",
            "helloutau.frequencyMap.jadeColors",  "helloutau.frequencyMap.grassyColors",
            "helloutau.frequencyMap.flameColors", "helloutau.frequencyMap.rubyColors",
        };

        // The actions of the tools of the view
        static constexpr std::pair<const char *, FrequencyMapView::Tool> toolActionIds[] = {
            {"helloutau.select.selectTool", FrequencyMapView::SelectTool},
            {"helloutau.select.penTool",    FrequencyMapView::PenTool   },
        };

        void updateRow(int index) {
            const auto item = list->topLevelItem(index);
            const auto &file = files.at(index);
            item->setText(1, presence(file.hasMap));
            const auto caches = format.analysisCaches();
            for (qsizetype i = 0; i < caches.size(); ++i) {
                item->setText(int(2 + i), presence(file.caches.value(i)));
            }
            const auto entry = openedOf(index);
            const auto corrected = entry ? correctedCount(entry->corrections) : 0;
            item->setText(correctionColumn, corrected > 0 ? QString::number(corrected) : QString());
            item->setText(modifiedColumn, presence(entry && entry->buffer->isModified()));
        }

        // Returns whether correction proposes a change.
        static bool corrects(const kit::FrequencyMapCheck::Correction &correction) {
            return correction.reason != kit::FrequencyMapCheck::NoCorrection;
        }

        // Returns the number of frames with a correction.
        static std::size_t
            correctedCount(const std::vector<kit::FrequencyMapCheck::Correction> &corrections) {
            return std::size_t(std::count_if(corrections.begin(), corrections.end(), corrects));
        }

        // Returns the run of frames with a correction around frame, if frame has one.
        static std::optional<kit::FrequencyMapCheck::Run>
            correctionRunAt(const std::vector<kit::FrequencyMapCheck::Correction> &corrections,
                            std::size_t frame) {
            if (frame >= corrections.size() || !corrects(corrections[frame])) {
                return std::nullopt;
            }
            kit::FrequencyMapCheck::Run run{frame, frame};
            while (run.first > 0 && corrects(corrections[run.first - 1])) {
                --run.first;
            }
            while (run.last + 1 < corrections.size() && corrects(corrections[run.last + 1])) {
                ++run.last;
            }
            return run;
        }

        // Returns the first frame with a correction after the run around from, or before it if
        // not forward, as the correction window of frqeditor finds it. Without from, the search
        // starts at the first frame, or at the last.
        static std::optional<std::size_t>
            nextCorrection(const std::vector<kit::FrequencyMapCheck::Correction> &corrections,
                           std::optional<std::size_t> from, bool forward) {
            const auto count = std::ptrdiff_t(corrections.size());
            const std::ptrdiff_t step = forward ? 1 : -1;
            std::ptrdiff_t i = from ? std::ptrdiff_t(*from) : (forward ? 0 : count - 1);
            if (from) {
                while (i >= 0 && i < count && corrects(corrections[std::size_t(i)])) {
                    i += step;
                }
            }
            for (; i >= 0 && i < count; i += step) {
                if (corrects(corrections[std::size_t(i)])) {
                    return std::size_t(i);
                }
            }
            return std::nullopt;
        }

        // Drops the corrections of the frames of run. After an adoption a correction without a
        // proposed frequency stays, because the adoption left its frame as it is, as in
        // frqeditor.
        void dropCorrections(Opened &entry, const kit::FrequencyMapCheck::Run &run, bool adopted) {
            for (std::size_t i = run.first; i <= run.last && i < entry.corrections.size(); ++i) {
                if (!adopted || entry.corrections[i].frequency > 0) {
                    entry.corrections[i] = {};
                }
            }
            if (correctedCount(entry.corrections) == 0) {
                entry.corrections.clear();
            }
        }

        void updateActions() {
            const auto buffer = currentBuffer();
            const auto selection = view->selection();
            const auto enable = [this](const char *id, bool enabled) {
                if (const auto action = actions.value(QLatin1String(id))) {
                    action->setEnabled(enabled);
                }
            };
            const bool modified = buffer && buffer->isModified();
            enable("helloutau.file.save", modified);
            bool anyModified = false;
            for (const auto &[index, entry] : opened) {
                anyModified = anyModified || entry.buffer->isModified();
            }
            enable("helloutau.frequencyMap.saveAll", anyModified);
            enable("helloutau.edit.undo", buffer && buffer->canUndo());
            enable("helloutau.edit.redo", buffer && buffer->canRedo());
            enable("helloutau.frequencyMap.revert", modified);
            enable("helloutau.frequencyMap.revertSelection", modified && selection.has_value());
            enable("helloutau.frequencyMap.keyToAverage",
                   buffer && selection && buffer->map().averageFrequency.has_value());
            for (const auto id :
                 {"helloutau.frequencyMap.multiplyBy2", "helloutau.frequencyMap.divideBy2",
                  "helloutau.frequencyMap.multiplyBy3", "helloutau.frequencyMap.divideBy3",
                  "helloutau.frequencyMap.unvoice"}) {
                enable(id, buffer && selection);
            }
            enable("helloutau.frequencyMap.interpolate",
                   buffer && selection && selection->second >= 3);
            enable("helloutau.select.selectAll", buffer != nullptr);
            enable("helloutau.select.deselect", selection.has_value());
            enable("helloutau.frequencyMap.previousFile", current > 0);
            enable("helloutau.frequencyMap.nextFile", current + 1 < files.size());
            enable("helloutau.frequencyMap.synthesize", audio != nullptr);
            bool anyCorrection = false;
            for (const auto &[index, entry] : opened) {
                anyCorrection = anyCorrection || !entry.corrections.empty();
            }
            enable("helloutau.frequencyMap.check",
                   std::any_of(files.begin(), files.end(),
                               [](const auto &file) { return file.hasMap; }));
            for (const auto id : {"helloutau.frequencyMap.previousCorrection",
                                  "helloutau.frequencyMap.nextCorrection",
                                  "helloutau.frequencyMap.adoptAllCorrections",
                                  "helloutau.frequencyMap.ignoreAllCorrections"}) {
                enable(id, anyCorrection);
            }
            const bool atCorrection = currentCorrectionRun().has_value();
            enable("helloutau.frequencyMap.adoptCorrection", atCorrection);
            enable("helloutau.frequencyMap.ignoreCorrection", atCorrection);
            if (const auto action =
                    actions.value(QStringLiteral("helloutau.frequencyMap.synthesize"))) {
                playButton->setEnabled(action->isEnabled());
            }
            if (current >= 0) {
                updateRow(current);
            }
            refreshPanel();
        }

        // Runs an edit of the current buffer and shows its result.
        void edit(const std::function<bool(kit::FrequencyMapRevisionBuffer &)> &change) {
            if (const auto buffer = currentBuffer(); buffer && change(*buffer)) {
                view->viewport()->update();
                updateActions();
            }
        }

        // Applies change to the selected frames of the current buffer.
        void editSelection(const std::function<bool(kit::FrequencyMapRevisionBuffer &, std::size_t,
                                                    std::size_t)> &change) {
            const auto selection = view->selection();
            if (!selection) {
                return;
            }
            edit([&](kit::FrequencyMapRevisionBuffer &buffer) {
                return change(buffer, selection->first, selection->second);
            });
        }

        // Checks the map of every file that has one in the background, with the maps read so far
        // as edited, and replaces the corrections of every map with the result. Moves to the
        // first correction. Returns false if the user cancels, which keeps the corrections as
        // they were.
        bool check(const kit::FrequencyMapCheck::Settings &settings) {
            stdc_decl_t;
            // Copied here, because the dialog takes input until the progress dialog appears
            struct Job {
                int index = 0;
                std::optional<kit::FrequencyMap> map;
                std::optional<std::filesystem::path> mapFile;
            };
            struct Result {
                int index = 0;
                std::optional<kit::FrequencyMap> read;
                int sampleRate = 0;
                FileStamp stamp;
                std::vector<kit::FrequencyMapCheck::Correction> corrections;
            };
            std::vector<Job> jobs;
            for (int i = 0; i < files.size(); ++i) {
                if (!files.at(i).hasMap) {
                    continue;
                }
                const auto entry = openedOf(i);
                jobs.push_back(
                    {i, entry ? std::optional(entry->buffer->map()) : std::nullopt, mapFileOf(i)});
            }
            std::vector<std::filesystem::path> paths;
            for (const auto &job : jobs) {
                paths.push_back(files.at(job.index).path);
            }
            std::vector<Result> results;
            const bool completed = runInBackground(
                tr("Checking the frequency maps..."), [&](const std::function<bool()> &canceled) {
                    for (std::size_t j = 0; j < jobs.size() && !canceled(); ++j) {
                        const auto &job = jobs[j];
                        kit::DiagnosticList diagnostics;
                        const auto wave = kit::WaveAudio::read(paths[j], diagnostics);
                        if (!wave) {
                            continue;
                        }
                        Result result;
                        result.index = job.index;
                        auto map = job.map;
                        if (!map) {
                            map = format.read(paths[j], wave->sampleRate, diagnostics);
                            if (!map) {
                                continue;
                            }
                            result.read = map;
                            result.sampleRate = wave->sampleRate;
                            if (job.mapFile) {
                                result.stamp = FileStamp::of(*job.mapFile);
                            }
                        }
                        result.corrections = kit::FrequencyMapCheck::check(
                            kit::FrequencyMapCheck::withAmplitudes(std::move(*map), *wave),
                            settings);
                        results.push_back(std::move(result));
                    }
                });
            if (!completed) {
                QMessageBox::warning(&decl, decl.windowTitle(),
                                     tr("The check was canceled. The corrections do not change."));
                return false;
            }
            for (auto &[index, entry] : opened) {
                entry.corrections.clear();
            }
            for (auto &result : results) {
                if (correctedCount(result.corrections) == 0) {
                    result.corrections.clear();
                }
                auto entry = openedOf(result.index);
                if (!entry) {
                    if (result.corrections.empty()) {
                        continue;
                    }
                    Opened created;
                    created.buffer =
                        std::make_unique<kit::FrequencyMapRevisionBuffer>(std::move(*result.read));
                    created.sampleRate = result.sampleRate;
                    created.stamp = result.stamp;
                    entry = &opened.emplace(result.index, std::move(created)).first->second;
                }
                entry->corrections = std::move(result.corrections);
            }
            for (int i = 0; i < files.size(); ++i) {
                updateRow(i);
            }
            if (const auto entry = openedOf(current)) {
                view->setCorrections(entry->corrections);
            }
            if (!goToCorrection(true, true)) {
                QMessageBox::information(&decl, decl.windowTitle(),
                                         tr("The check found nothing to correct."));
            }
            updateActions();
            return true;
        }

        // Shows a dialog in which the user chooses the settings of the check, as the check
        // dialog of frqeditor does, initially those of the last check, and keeps them in the
        // settings. Returns std::nullopt if the user cancels.
        std::optional<kit::FrequencyMapCheck::Settings> askCheckSettings() {
            stdc_decl_t;
            auto settings = editor->settings().frequencyMaps().checkSettings();
            QDialog dialog(&decl);
            dialog.setWindowTitle(tr("Check Frequency Maps"));
            auto note = new QComboBox();
            // C1 to B7, as frqeditor offers them
            for (int noteNum = 24; noteNum <= 107; ++noteNum) {
                note->addItem(QString::fromStdString(utau::toneNumToToneName(noteNum)), noteNum);
            }
            note->setCurrentIndex(note->findData(settings.note));
            auto pitchChange = new QCheckBox(
                tr("The recordings have some change of &pitch, such as a vibrato, a scoop or a "
                   "fall"));
            pitchChange->setChecked(settings.allowsPitchChange);
            auto changeAtStart =
                new QCheckBox(tr("The recordings &start with a large change of pitch"));
            changeAtStart->setChecked(settings.allowsChangeAtStart);
            auto changeAtEnd =
                new QCheckBox(tr("The recordings &end with a large change of pitch"));
            changeAtEnd->setChecked(settings.allowsChangeAtEnd);
            auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
            QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
            QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            auto form = new QFormLayout(&dialog);
            form->addRow(tr("&Note of the recordings:"), note);
            form->addRow(pitchChange);
            form->addRow(changeAtStart);
            form->addRow(changeAtEnd);
            form->addRow(buttons);
            if (dialog.exec() != QDialog::Accepted) {
                return std::nullopt;
            }
            settings.note = note->currentData().toInt();
            settings.allowsPitchChange = pitchChange->isChecked();
            settings.allowsChangeAtStart = changeAtStart->isChecked();
            settings.allowsChangeAtEnd = changeAtEnd->isChecked();
            editor->settings().frequencyMaps().setCheckSettings(settings);
            return settings;
        }

        // Returns the run of corrections at the first selected frame of the current file.
        std::optional<kit::FrequencyMapCheck::Run> currentCorrectionRun() {
            const auto entry = openedOf(current);
            const auto selection = view->selection();
            if (!entry || !selection) {
                return std::nullopt;
            }
            return correctionRunAt(entry->corrections, selection->first);
        }

        // Selects the next run of corrections in the direction of forward: in the current file
        // after the selection, then in the other files from their ends, wrapping around. With
        // fromStart, the search begins at the end of the first file, or of the last. Returns
        // whether a run was found.
        bool goToCorrection(bool forward, bool fromStart = false) {
            correctionForward = forward;
            const int count = int(files.size());
            const int start = fromStart ? (forward ? 0 : count - 1) : std::max(current, 0);
            for (int k = 0; k < count; ++k) {
                const int index = ((start + (forward ? k : -k)) % count + count) % count;
                const auto entry = openedOf(index);
                if (!entry || entry->corrections.empty()) {
                    continue;
                }
                std::optional<std::size_t> from;
                if (k == 0 && !fromStart && index == current) {
                    if (const auto selection = view->selection()) {
                        from = selection->first;
                    }
                }
                const auto found = nextCorrection(entry->corrections, from, forward);
                if (!found) {
                    continue;
                }
                if (index != current && !show(index)) {
                    return false;
                }
                const auto run = *correctionRunAt(openedOf(index)->corrections, *found);
                view->setSelection(run.first, run.last - run.first + 1);
                // A few frames before the run stay in sight, as in frqeditor.
                view->scrollToFrame(run.first > 5 ? run.first - 5 : 0);
                return true;
            }
            return false;
        }

        // Adopts or ignores the run of corrections at the selection, then moves to the next run
        // in the direction of the last move.
        void resolveCorrection(bool adopt) {
            const auto run = currentCorrectionRun();
            if (!run) {
                return;
            }
            auto &entry = *openedOf(current);
            if (adopt) {
                const std::vector<kit::FrequencyMapCheck::Correction> part(
                    entry.corrections.begin() + std::ptrdiff_t(run->first),
                    entry.corrections.begin() + std::ptrdiff_t(run->last + 1));
                entry.buffer->adoptCorrections(run->first, part);
            }
            dropCorrections(entry, *run, adopt);
            view->setCorrections(entry.corrections);
            goToCorrection(correctionForward);
            view->viewport()->update();
            updateActions();
        }

        // Adopts every correction of every map, each map as one step of its history.
        void adoptAllCorrections() {
            for (auto &[index, entry] : opened) {
                if (entry.corrections.empty()) {
                    continue;
                }
                entry.buffer->adoptCorrections(0, entry.corrections);
                dropCorrections(entry, {0, entry.corrections.size() - 1}, true);
                updateRow(index);
            }
            if (const auto entry = openedOf(current)) {
                view->setCorrections(entry->corrections);
            }
            view->viewport()->update();
            updateActions();
        }

        // Drops every correction of every map and leaves the frames as they are. A map other
        // than the current one, left without a history, is read again when it is shown again.
        void ignoreAllCorrections() {
            for (auto it = opened.begin(); it != opened.end();) {
                auto &[index, entry] = *it;
                entry.corrections.clear();
                const bool released =
                    index != current && !entry.buffer->canUndo() && !entry.buffer->canRedo();
                const int row = index;
                it = released ? opened.erase(it) : std::next(it);
                updateRow(row);
            }
            view->setCorrections({});
            view->viewport()->update();
            updateActions();
        }

        bool save(int index) {
            stdc_decl_t;
            const auto entry = openedOf(index);
            if (!entry || !entry->buffer->isModified()) {
                return false;
            }
            const auto &file = files.at(index);
            const auto title = tr("Save Frequency Map");
            const auto mapFile = mapFileOf(index);
            if (mapFile && !(FileStamp::of(*mapFile) == entry->stamp)) {
                const auto answer = QMessageBox::question(
                    &decl, title,
                    tr("The file %1 changed after the frequency map of %2 was read. Overwrite "
                       "it?")
                        .arg(textOf(mapFile->filename()), shownNameOf(file)),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                if (answer != QMessageBox::Yes) {
                    return false;
                }
            }
            kit::FrequencyFormat::MapWrite write;
            write.wav = file.path.filename();
            write.map = entry->buffer->map();
            write.sampleRate = entry->sampleRate;
            write.editedByHand = true;
            kit::DiagnosticList diagnostics;
            if (!kit::FrequencyFormats::writeMaps(format, file.path.parent_path(), {write},
                                                  removeCaches->isChecked(), diagnostics)) {
                DiagnosticBox::report(&decl, title, diagnostics);
                return false;
            }
            entry->buffer->markSaved();
            // The other maps in the same file, such as desc.mrq, were read before this write.
            if (mapFile) {
                const auto stamp = FileStamp::of(*mapFile);
                for (auto &[other, sharing] : opened) {
                    if (mapFileOf(other) == mapFile) {
                        sharing.stamp = stamp;
                    }
                }
            }
            auto &listed = files[index];
            const auto caches = format.analysisCaches();
            for (qsizetype i = 0; i < caches.size() && i < listed.caches.size(); ++i) {
                std::error_code error;
                listed.caches[i] = std::filesystem::is_regular_file(
                    file.path.parent_path() / caches.at(i).fileNameOf(file.path.filename()), error);
            }
            if (!diagnostics.isEmpty()) {
                DiagnosticBox::report(&decl, title, diagnostics);
            }
            updateRow(index);
            view->viewport()->update();
            return true;
        }

        QAction *addCommand(const QString &id, std::function<void()> handler) {
            stdc_decl_t;
            const auto action = new QAction(&decl);
            QObject::connect(action, &QAction::triggered, &decl, std::move(handler));
            context->addAction(id, action);
            actions.insert(id, action);
            return action;
        }

        void initActions(QMenuBar *menuBar) {
            stdc_decl_t;
            context = new QAK::WidgetActionContext(&decl);
            context->addMenuBar(QStringLiteral("helloutau.frequencyMapEditorMenu"), menuBar);
            context->addToolBar(QStringLiteral("helloutau.frequencyMap.toolBar"), toolBar);
            // The same command as the button Play of the preview
            addCommand(QStringLiteral("helloutau.frequencyMap.synthesize"), [this] { play(); });
            addCommand(QStringLiteral("helloutau.playback.stop"), [this] {
                preview->stop();
            })->setEnabled(false);
            addCommand(QStringLiteral("helloutau.frequencyMap.check"), [this] {
                if (const auto settings = askCheckSettings()) {
                    check(*settings);
                }
            });
            addCommand(QStringLiteral("helloutau.frequencyMap.previousCorrection"), [this] {
                goToCorrection(false);
                updateActions();
            });
            addCommand(QStringLiteral("helloutau.frequencyMap.nextCorrection"), [this] {
                goToCorrection(true);
                updateActions();
            });
            addCommand(QStringLiteral("helloutau.frequencyMap.adoptCorrection"),
                       [this] { resolveCorrection(true); });
            addCommand(QStringLiteral("helloutau.frequencyMap.ignoreCorrection"),
                       [this] { resolveCorrection(false); });
            addCommand(QStringLiteral("helloutau.frequencyMap.adoptAllCorrections"),
                       [this] { adoptAllCorrections(); });
            addCommand(QStringLiteral("helloutau.frequencyMap.ignoreAllCorrections"),
                       [this] { ignoreAllCorrections(); });
            addCommand(QStringLiteral("helloutau.file.save"), [this] {
                stdc_decl_t;
                decl.save();
            });
            addCommand(QStringLiteral("helloutau.frequencyMap.saveAll"), [this] {
                stdc_decl_t;
                decl.saveAll();
            });
            addCommand(QStringLiteral("helloutau.file.close"), [this] {
                stdc_decl_t;
                decl.reject();
            });
            addCommand(QStringLiteral("helloutau.edit.undo"),
                       [this] { edit([](auto &buffer) { return buffer.undo(); }); });
            addCommand(QStringLiteral("helloutau.edit.redo"),
                       [this] { edit([](auto &buffer) { return buffer.redo(); }); });
            addCommand(QStringLiteral("helloutau.frequencyMap.revert"),
                       [this] { edit([](auto &buffer) { return buffer.revertAll(); }); });
            addCommand(QStringLiteral("helloutau.frequencyMap.revertSelection"), [this] {
                editSelection([](auto &buffer, std::size_t first, std::size_t count) {
                    return buffer.revert(first, count);
                });
            });
            addCommand(QStringLiteral("helloutau.frequencyMap.keyToAverage"), [this] {
                editSelection([](auto &buffer, std::size_t first, std::size_t count) {
                    return buffer.setKeyToMean(first, count);
                });
            });
            const std::pair<const char *, double> factors[] = {
                {"helloutau.frequencyMap.multiplyBy2", 2      },
                {"helloutau.frequencyMap.divideBy2",   0.5    },
                {"helloutau.frequencyMap.multiplyBy3", 3      },
                {"helloutau.frequencyMap.divideBy3",   1.0 / 3},
            };
            for (const auto &[id, factor] : factors) {
                addCommand(QLatin1String(id), [this, factor = factor] {
                    editSelection([factor](auto &buffer, std::size_t first, std::size_t count) {
                        return buffer.scale(first, count, factor);
                    });
                });
            }
            addCommand(QStringLiteral("helloutau.frequencyMap.unvoice"), [this] {
                editSelection([](auto &buffer, std::size_t first, std::size_t count) {
                    return buffer.unvoice(first, count);
                });
            });
            addCommand(QStringLiteral("helloutau.frequencyMap.interpolate"), [this] {
                editSelection([](auto &buffer, std::size_t first, std::size_t count) {
                    return buffer.interpolate(first, count);
                });
            });
            addCommand(QStringLiteral("helloutau.select.selectAll"), [this] { view->selectAll(); });
            addCommand(QStringLiteral("helloutau.select.deselect"),
                       [this] { view->clearSelection(); });
            const auto tools = new QActionGroup(&decl);
            for (const auto &[id, tool] : toolActionIds) {
                const auto action =
                    addCommand(QLatin1String(id), [this, tool = tool] { view->setTool(tool); });
                action->setCheckable(true);
                action->setChecked(view->tool() == tool);
                tools->addAction(action);
            }
            addCommand(QStringLiteral("helloutau.frequencyMap.zoomIn"),
                       [this] { view->setZoomLevel(view->zoomLevel() + 1); });
            addCommand(QStringLiteral("helloutau.frequencyMap.zoomOut"),
                       [this] { view->setZoomLevel(view->zoomLevel() - 1); });
            const auto spectrum =
                addCommand(QStringLiteral("helloutau.frequencyMap.showSpectrogram"), [this] {
                    view->setViewMode(spectrumAction()->isChecked()
                                          ? FrequencyMapView::SpectrumView
                                          : FrequencyMapView::KeyRowsView);
                });
            spectrum->setCheckable(true);
            spectrum->setChecked(view->viewMode() == FrequencyMapView::SpectrumView);
            for (int i = 0; i < FrequencyMapView::colorSchemeCount; ++i) {
                addCommand(QLatin1String(colorActionIds[i]), [this, i] {
                    view->setColorScheme(i);
                    updateColorActions();
                })->setCheckable(true);
            }
            // A lower brightness is brighter, as in frqeditor.
            addCommand(QStringLiteral("helloutau.frequencyMap.brighten"),
                       [this] { view->setBrightness(view->brightness() - 1); });
            addCommand(QStringLiteral("helloutau.frequencyMap.darken"),
                       [this] { view->setBrightness(view->brightness() + 1); });
            addCommand(QStringLiteral("helloutau.frequencyMap.previousFile"), [this] {
                if (current > 0) {
                    show(current - 1);
                }
            });
            addCommand(QStringLiteral("helloutau.frequencyMap.nextFile"), [this] {
                if (current + 1 < files.size()) {
                    show(current + 1);
                }
            });
            editor->addContributedActions(&decl, context);

            const auto registry = editor->actionRegistry(Editor::FrequencyMapEditorKind);
            registry->addContext(context);
            for (const auto element :
                 {QAK::AE_Layouts, QAK::AE_Texts, QAK::AE_Keymap, QAK::AE_Icons}) {
                registry->updateContext(element);
            }
        }
    };

    EditFrequencyMapsDialog::EditFrequencyMapsDialog(Editor *editor,
                                                     const kit::FrequencyFormat &format,
                                                     QList<File> files,
                                                     const std::filesystem::path &root,
                                                     QWidget *parent)
        : QDialog(parent), _impl(std::make_unique<Impl>(this, editor, format, std::move(files))) {
        stdc_impl_t;
        setWindowTitle(tr("Edit Frequency Maps"));

        const auto caches = format.analysisCaches();
        QStringList headers{tr("File"), tr("Frequency Map")};
        for (const auto &cache : caches) {
            headers.push_back(cache.name);
        }
        headers.push_back(tr("Modified"));
        impl.modifiedColumn = int(headers.size() - 1);
        headers.push_back(tr("Corrections"));
        impl.correctionColumn = int(headers.size() - 1);
        impl.list = new QTreeWidget();
        impl.list->setRootIsDecorated(false);
        impl.list->setHeaderLabels(headers);
        impl.list->header()->setSectionResizeMode(QHeaderView::Interactive);
        impl.list->header()->setStretchLastSection(false);
        for (int column = 1; column < int(headers.size()); ++column) {
            impl.list->headerItem()->setTextAlignment(column, Qt::AlignCenter);
        }
        int initial = -1;
        int firstWithMap = -1;
        for (int i = 0; i < impl.files.size(); ++i) {
            const auto &file = impl.files.at(i);
            auto relative = file.path.lexically_relative(root);
            if (!file.shownPath.empty()) {
                relative = file.shownPath;
            }
            auto item = new QTreeWidgetItem(impl.list);
            item->setText(
                0, QDir::toNativeSeparators(textOf(relative.empty() ? file.path : relative)));
            for (int column = 1; column < int(headers.size()); ++column) {
                item->setTextAlignment(column, Qt::AlignCenter);
            }
            impl.updateRow(i);
            if (initial < 0 && file.selected) {
                initial = i;
            }
            if (firstWithMap < 0 && file.hasMap) {
                firstWithMap = i;
            }
        }
        for (int column = 0; column < int(headers.size()); ++column) {
            impl.list->resizeColumnToContents(column);
        }

        impl.view = new FrequencyMapView();
        impl.view->setKeyboardModifierBindings(editor->keyboardModifierBindings(
            Editor::FrequencyMapEditorKind, FrequencyViewKeyboardModifiers::scheme()));
        // The view, the brightness and the color scheme that the last dialog left. The first file
        // computes its spectrogram as it is read if the view shows it.
        const auto &mapSettings = editor->settings().frequencyMaps();
        impl.view->setViewMode(mapSettings.showsSpectrogram() ? FrequencyMapView::SpectrumView
                                                              : FrequencyMapView::KeyRowsView);
        impl.view->setBrightness(mapSettings.spectrogramBrightness());
        impl.view->setColorScheme(mapSettings.spectrogramColorScheme());
        impl.message = new QLabel();
        impl.message->setWordWrap(true);
        impl.message->hide();
        impl.removeCaches =
            new QCheckBox(tr("Re&move the analysis caches when saving a frequency map"));
        impl.removeCaches->setChecked(editor->settings().frequencyMaps().removesCachesOnSave());
        impl.removeCaches->setVisible(!caches.isEmpty());
        auto removeCachesNote =
            new QLabel(tr("This choice applies while the dialog is open. The default is set on "
                          "the Frequency Maps page of the settings. Without removing the caches, "
                          "the resampler keeps synthesizing from the frequencies of the caches."));
        removeCachesNote->setWordWrap(true);
        removeCachesNote->setVisible(!caches.isEmpty());

        // The right side of the window of frqeditor: the key and the frames in the left column,
        // the frequency of the current frame, the preview and the information of the audio file
        // in the middle column, the editing area at the right and the buttons along the bottom.
        // The two columns keep their widths, as in frqeditor.
        impl.keyEdit = new QLineEdit();
        impl.updateKeyButton = new QPushButton(tr("&Update"));
        auto keyLabel = new QLabel(tr("&Key"));
        keyLabel->setBuddy(impl.keyEdit);
        // The label above the field, so that the field takes the width of the column
        auto keyRow = new QHBoxLayout();
        keyRow->addWidget(impl.keyEdit, 1);
        keyRow->addWidget(new QLabel(tr("Hz")));
        keyRow->addWidget(impl.updateKeyButton);
        impl.frameList = new QListWidget();
        impl.frameList->setSelectionMode(QAbstractItemView::ContiguousSelection);
        impl.frameList->setUniformItemSizes(true);
        auto leftColumn = new QWidget();
        leftColumn->setFixedWidth(200);
        auto leftLayout = new QVBoxLayout(leftColumn);
        leftLayout->setContentsMargins(0, 0, 0, 0);
        leftLayout->addWidget(keyLabel);
        leftLayout->addLayout(keyRow);
        leftLayout->addWidget(impl.frameList, 1);

        impl.frequencyEdit = new QLineEdit();
        impl.setButton = new QPushButton(tr("<-- Se&t"));
        impl.pitchSlider = new QSlider(Qt::Horizontal);
        impl.pitchSlider->setRange(-12, 12);
        impl.pitchSlider->setValue(0);
        impl.pitchLabel = new QLabel(QStringLiteral("0"));
        impl.modulationEdit = new QLineEdit();
        impl.flagsEdit = new QLineEdit();
        impl.directBox = new QCheckBox(QStringLiteral("$direct=True"));
        impl.playButton = new QPushButton(tr("&Play"));
        auto previewGroup = new QGroupBox(tr("Preview"));
        auto previewLayout = new QGridLayout(previewGroup);
        previewLayout->addWidget(new QLabel(tr("Pitch")), 0, 0, 1, 3);
        previewLayout->addWidget(impl.pitchSlider, 1, 0, 1, 2);
        previewLayout->addWidget(impl.pitchLabel, 1, 2);
        previewLayout->addWidget(new QLabel(tr("Modulation")), 2, 0);
        previewLayout->addWidget(impl.modulationEdit, 2, 1);
        previewLayout->addWidget(new QLabel(QStringLiteral("%")), 2, 2);
        previewLayout->addWidget(new QLabel(tr("Flags")), 3, 0);
        previewLayout->addWidget(impl.flagsEdit, 3, 1, 1, 2);
        previewLayout->addWidget(impl.directBox, 4, 0, 1, 3);
        previewLayout->addWidget(impl.playButton, 5, 0, 1, 3, Qt::AlignHCenter);
        impl.wavInfo = new QLabel();
        impl.wavInfo->setWordWrap(true);
        auto wavInfoGroup = new QGroupBox(tr("Information of the Audio File"));
        auto wavInfoLayout = new QVBoxLayout(wavInfoGroup);
        wavInfoLayout->addWidget(impl.wavInfo);
        auto middleColumn = new QWidget();
        middleColumn->setFixedWidth(180);
        auto middleLayout = new QVBoxLayout(middleColumn);
        middleLayout->setContentsMargins(0, 0, 0, 0);
        middleLayout->addWidget(impl.frequencyEdit);
        middleLayout->addWidget(impl.setButton, 0, Qt::AlignLeft);
        middleLayout->addWidget(previewGroup);
        middleLayout->addStretch(1);
        middleLayout->addWidget(wavInfoGroup);

        auto editing = new QWidget();
        auto editingLayout = new QVBoxLayout(editing);
        editingLayout->setContentsMargins(0, 0, 0, 0);
        editingLayout->addWidget(impl.message);
        // The waveform below the editing area, which follows its time axis
        impl.waveform = new FrequencyMapWaveform(impl.view);
        auto editingSplitter = new QSplitter(Qt::Vertical);
        editingSplitter->setChildrenCollapsible(false);
        editingSplitter->addWidget(impl.view);
        editingSplitter->addWidget(impl.waveform);
        editingSplitter->setStretchFactor(0, 3);
        editingSplitter->setStretchFactor(1, 1);
        editingLayout->addWidget(editingSplitter, 1);

        // The buttons 1009 to 1016 of frqeditor, as icons of the commands. Each button takes the
        // icon, the tool tip and the enabled state of its command once the commands exist.
        auto buttonRow = new QHBoxLayout();
        buttonRow->setSpacing(2);
        for (const auto id : {
                 "helloutau.frequencyMap.multiplyBy3",
                 "helloutau.frequencyMap.multiplyBy2",
                 "helloutau.frequencyMap.unvoice",
                 "helloutau.frequencyMap.divideBy2",
                 "helloutau.frequencyMap.divideBy3",
                 "helloutau.frequencyMap.interpolate",
                 "helloutau.frequencyMap.revert",
                 "helloutau.frequencyMap.keyToAverage",
             }) {
            auto button = new QToolButton();
            button->setAutoRaise(true);
            buttonRow->addWidget(button);
            impl.editButtons.push_back({button, QLatin1String(id)});
        }
        buttonRow->addStretch(1);
        // The tool bar below the menu bar, which QActionKit fills from the layout of
        // helloutau.frequencyMap.toolBar: the tools of the editing area, the playback and the
        // corrections
        impl.toolBar = new QToolBar();
        impl.toolBar->setObjectName(QStringLiteral("frequencyMapToolBar"));
        followToolBarPalette(impl.toolBar);

        auto panel = new QWidget();
        auto grid = new QGridLayout(panel);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->addWidget(leftColumn, 0, 0);
        grid->addWidget(middleColumn, 0, 1);
        grid->addWidget(editing, 0, 2);
        grid->addLayout(buttonRow, 1, 0, 1, 3);
        grid->addWidget(impl.removeCaches, 2, 0, 1, 3);
        grid->addWidget(removeCachesNote, 3, 0, 1, 3);
        grid->setColumnStretch(2, 1);
        grid->setRowStretch(0, 1);

        auto splitter = new QSplitter();
        splitter->addWidget(impl.list);
        splitter->addWidget(panel);
        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 1);

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Close);
        auto menuBar = new QMenuBar();
        auto layout = new QVBoxLayout(this);
        layout->setMenuBar(menuBar);
        layout->addWidget(impl.toolBar);
        layout->addWidget(splitter, 1);
        layout->addWidget(buttons);
        impl.initActions(menuBar);

        // Wide enough to show the whole list beside the right side with an editing area of
        // editingWidth, within the screen. If the screen is narrower, the list is narrowed so that
        // the right side keeps its least width. A resize changes the right side only.
        constexpr int editingWidth = 600;
        const QRect available = (parent ? parent->screen() : screen())->availableGeometry();
        const auto margins = layout->contentsMargins();
        const int listWidth = impl.list->header()->length() + 2 * impl.list->frameWidth() +
                              impl.list->verticalScrollBar()->sizeHint().width();
        const int panelMinimum = panel->minimumSizeHint().width();
        const int width = std::min(margins.left() + listWidth + splitter->handleWidth() +
                                       panelMinimum + editingWidth + margins.right(),
                                   available.width() * 9 / 10);
        const int splitterWidth = width - margins.left() - margins.right();
        const int listShown =
            std::max(impl.list->minimumSizeHint().width(),
                     std::min(listWidth, splitterWidth - splitter->handleWidth() - panelMinimum));
        splitter->setSizes({listShown, splitterWidth - splitter->handleWidth() - listShown});
        resize(width, std::min(680, available.height() * 9 / 10));

        connect(buttons, &QDialogButtonBox::rejected, this, &EditFrequencyMapsDialog::reject);
        for (const auto &[button, id] : std::as_const(impl.editButtons)) {
            button->setDefaultAction(impl.actions.value(id));
        }
        connect(impl.frameList, &QListWidget::itemSelectionChanged, this, [this] {
            stdc_impl_t;
            impl.selectListedFrames();
        });
        connect(impl.frameList, &QListWidget::currentRowChanged, this, [this] {
            stdc_impl_t;
            impl.updateFrequencyEdit();
        });
        const auto setFrequency = [this] {
            stdc_impl_t;
            const int row = impl.frameList->currentRow();
            const double frequency = impl.frequencyEdit->text().toDouble();
            impl.edit([row, frequency](auto &buffer) {
                return row >= 0 && buffer.setFrequency(std::size_t(row), frequency);
            });
        };
        connect(impl.setButton, &QPushButton::clicked, this, setFrequency);
        connect(impl.frequencyEdit, &QLineEdit::returnPressed, this, setFrequency);
        const auto setKey = [this] {
            stdc_impl_t;
            bool valid = false;
            const double key = impl.keyEdit->text().toDouble(&valid);
            if (valid) {
                impl.edit([key](auto &buffer) { return buffer.setKey(key); });
            }
            impl.refreshPanel();
        };
        connect(impl.updateKeyButton, &QPushButton::clicked, this, setKey);
        connect(impl.keyEdit, &QLineEdit::editingFinished, this, setKey);
        connect(impl.pitchSlider, &QSlider::valueChanged, this, [this](int value) {
            stdc_impl_t;
            impl.pitchLabel->setNum(value);
        });
        impl.preview = new SamplePreview(this);
        impl.playheadTimer = new QTimer(this);
        impl.playheadTimer->setInterval(30);
        connect(impl.playheadTimer, &QTimer::timeout, this, [this] {
            stdc_impl_t;
            impl.movePlayhead();
        });
        connect(impl.playButton, &QPushButton::clicked, this, [this] {
            stdc_impl_t;
            if (const auto action =
                    impl.actions.value(QStringLiteral("helloutau.frequencyMap.synthesize"));
                action && action->isEnabled()) {
                action->trigger();
            }
        });
        connect(impl.preview, &SamplePreview::stateChanged, this,
                [this](SamplePreview::State state) {
                    stdc_impl_t;
                    impl.playButton->setText(state == SamplePreview::Stopped ? tr("&Play")
                                                                             : tr("&Stop"));
                    impl.actions.value(QStringLiteral("helloutau.playback.stop"))
                        ->setEnabled(state != SamplePreview::Stopped);
                    if (state == SamplePreview::Playing) {
                        impl.playheadTimer->start();
                        impl.movePlayhead();
                    } else {
                        impl.playheadTimer->stop();
                        impl.waveform->setPlayhead(std::nullopt);
                    }
                });
        connect(impl.preview, &SamplePreview::failed, this,
                [this](const kit::DiagnosticList &diagnostics) {
                    DiagnosticBox::report(this, tr("Preview"), diagnostics);
                });
        connect(impl.list, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) {
            stdc_impl_t;
            if (item) {
                impl.show(impl.list->indexOfTopLevelItem(item));
            }
        });
        connect(impl.view, &FrequencyMapView::edited, this, [this] {
            stdc_impl_t;
            impl.updateActions();
        });
        // The view kept for the next dialog after a canceled computation of the spectrogram is the
        // rows of the keys, which the view returns to.
        connect(impl.view, &FrequencyMapView::viewModeChanged, this, [this, editor] {
            stdc_impl_t;
            impl.updateSpectrogram();
            editor->settings().frequencyMaps().setShowsSpectrogram(impl.view->viewMode() ==
                                                                   FrequencyMapView::SpectrumView);
        });
        connect(impl.view, &FrequencyMapView::spectrumChanged, this, [this, editor] {
            stdc_impl_t;
            impl.updateColorActions();
            auto &settings = editor->settings().frequencyMaps();
            settings.setSpectrogramBrightness(impl.view->brightness());
            settings.setSpectrogramColorScheme(impl.view->colorScheme());
        });
        impl.updateColorActions();
        connect(impl.view, &FrequencyMapView::selectionChanged, this, [this] {
            stdc_impl_t;
            impl.updateActions();
        });
        connect(editor, &Editor::keyboardModifierBindingsChanged, this,
                [this, editor](Editor::WindowKind kind) {
                    stdc_impl_t;
                    if (kind == Editor::FrequencyMapEditorKind) {
                        impl.view->setKeyboardModifierBindings(editor->keyboardModifierBindings(
                            kind, FrequencyViewKeyboardModifiers::scheme()));
                    }
                });

        if (!impl.files.isEmpty()) {
            impl.initial = initial >= 0 ? initial : std::max(0, firstWithMap);
        }
    }

    EditFrequencyMapsDialog::~EditFrequencyMapsDialog() = default;

    int EditFrequencyMapsDialog::currentFile() const {
        stdc_impl_t;
        return impl.current;
    }

    bool EditFrequencyMapsDialog::setCurrentFile(int index) {
        stdc_impl_t;
        return index >= 0 && index < impl.files.size() && impl.show(index);
    }

    bool EditFrequencyMapsDialog::check(const kit::FrequencyMapCheck::Settings &settings) {
        stdc_impl_t;
        return impl.check(settings);
    }

    std::vector<kit::FrequencyMapCheck::Correction>
        EditFrequencyMapsDialog::correctionsOf(int index) const {
        stdc_impl_t;
        const auto it = impl.opened.find(index);
        return it == impl.opened.end() ? std::vector<kit::FrequencyMapCheck::Correction>()
                                       : it->second.corrections;
    }

    kit::FrequencyMapRevisionBuffer *EditFrequencyMapsDialog::bufferOf(int index) const {
        stdc_impl_t;
        const auto it = impl.opened.find(index);
        return it == impl.opened.end() ? nullptr : it->second.buffer.get();
    }

    bool EditFrequencyMapsDialog::isModified(int index) const {
        const auto buffer = bufferOf(index);
        return buffer && buffer->isModified();
    }

    bool EditFrequencyMapsDialog::save() {
        stdc_impl_t;
        const bool saved = impl.save(impl.current);
        impl.updateActions();
        return saved;
    }

    bool EditFrequencyMapsDialog::saveAll() {
        stdc_impl_t;
        QList<int> modified;
        for (const auto &[index, entry] : impl.opened) {
            if (entry.buffer->isModified()) {
                modified.push_back(index);
            }
        }
        bool saved = true;
        for (const int index : std::as_const(modified)) {
            if (!impl.save(index)) {
                saved = false;
                break;
            }
        }
        impl.updateActions();
        return saved;
    }

    QTreeWidget *EditFrequencyMapsDialog::fileList() const {
        stdc_impl_t;
        return impl.list;
    }

    FrequencyMapView *EditFrequencyMapsDialog::view() const {
        stdc_impl_t;
        return impl.view;
    }

    FrequencyMapWaveform *EditFrequencyMapsDialog::waveform() const {
        stdc_impl_t;
        return impl.waveform;
    }

    QLineEdit *EditFrequencyMapsDialog::keyEdit() const {
        stdc_impl_t;
        return impl.keyEdit;
    }

    QListWidget *EditFrequencyMapsDialog::frameList() const {
        stdc_impl_t;
        return impl.frameList;
    }

    QLineEdit *EditFrequencyMapsDialog::frequencyEdit() const {
        stdc_impl_t;
        return impl.frequencyEdit;
    }

    QPushButton *EditFrequencyMapsDialog::setButton() const {
        stdc_impl_t;
        return impl.setButton;
    }

    QPushButton *EditFrequencyMapsDialog::playButton() const {
        stdc_impl_t;
        return impl.playButton;
    }

    QCheckBox *EditFrequencyMapsDialog::directBox() const {
        stdc_impl_t;
        return impl.directBox;
    }

    SamplePreview *EditFrequencyMapsDialog::preview() const {
        stdc_impl_t;
        return impl.preview;
    }

    QCheckBox *EditFrequencyMapsDialog::removeCachesBox() const {
        stdc_impl_t;
        return impl.removeCaches;
    }

    QLabel *EditFrequencyMapsDialog::messageLabel() const {
        stdc_impl_t;
        return impl.message;
    }

    QAK::WidgetActionContext *EditFrequencyMapsDialog::actionContext() const {
        stdc_impl_t;
        return impl.context;
    }

    void EditFrequencyMapsDialog::setProgressDelay(int milliseconds) {
        stdc_impl_t;
        impl.progressDelay = milliseconds;
    }

    void EditFrequencyMapsDialog::showEvent(QShowEvent *event) {
        stdc_impl_t;
        QDialog::showEvent(event);
        // Read once the dialog is shown, so that the progress dialog of the reading has the
        // dialog as its window.
        if (!impl.shownOnce && impl.initial >= 0) {
            impl.shownOnce = true;
            QTimer::singleShot(0, this, [this] {
                stdc_impl_t;
                impl.show(impl.initial);
            });
        }
    }

    void EditFrequencyMapsDialog::reject() {
        stdc_impl_t;
        bool anyModified = false;
        for (const auto &[index, entry] : impl.opened) {
            anyModified = anyModified || entry.buffer->isModified();
        }
        if (anyModified) {
            // The button that discards is named as in the project window.
            QMessageBox box(QMessageBox::Question, tr("Edit Frequency Maps"),
                            tr("Save the modified frequency maps before closing?"),
                            QMessageBox::SaveAll | QMessageBox::Discard | QMessageBox::Cancel,
                            this);
            box.button(QMessageBox::Discard)->setText(tr("Do not save"));
            box.setDefaultButton(QMessageBox::SaveAll);
            const auto answer = QMessageBox::StandardButton(box.exec());
            if (answer == QMessageBox::Cancel || (answer == QMessageBox::SaveAll && !saveAll())) {
                return;
            }
        }
        QDialog::reject();
    }

}
