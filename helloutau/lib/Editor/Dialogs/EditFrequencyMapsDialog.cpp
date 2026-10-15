#include "EditFrequencyMapsDialog.h"

#include <atomic>
#include <functional>
#include <limits>
#include <map>
#include <optional>

#include <QtCore/QDir>
#include <QtCore/QEventLoop>
#include <QtCore/QHash>
#include <QtCore/QSignalBlocker>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include <stdcorelib/pimpl.h>

#include <QAKCore/actionregistry.h>
#include <QAKWidgets/widgetactioncontext.h>

#include <hellokit/AudioFile/Spectrogram.h>
#include <hellokit/AudioFile/WaveAudio.h>
#include <hellokit/VoiceBank/FrequencyFormats.h>
#include <hellokit/VoiceBank/FrequencyMapRevisionBuffer.h>

#include "AppSettings.h"
#include "DiagnosticBox.h"
#include "Editor.h"
#include "FrequencyMapView.h"
#include "FrequencyViewKeyboardModifiers.h"

namespace hello::daw {

    namespace {

        QString textOf(const std::filesystem::path &path) {
            return QString::fromStdU16String(path.u16string());
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
        };
        std::map<int, Opened> opened;
        int current = -1;
        // The column of the modified mark, after the columns of the caches
        int modifiedColumn = 0;

        QTreeWidget *list = nullptr;
        FrequencyMapView *view = nullptr;
        QLabel *message = nullptr;
        QCheckBox *removeCaches = nullptr;
        QAK::WidgetActionContext *context = nullptr;
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
        // message why it cannot be edited. A cancellation keeps the current file, and returns
        // false.
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
                                         .arg(textOf(file.path.filename())));
                return false;
            }

            // A map left without a history is read again when it is shown again.
            if (const auto left = openedOf(current);
                left && current != index && !left->buffer->canUndo() && !left->buffer->canRedo()) {
                opened.erase(current);
            }
            current = index;
            {
                const QSignalBlocker blocker(list);
                list->setCurrentItem(list->topLevelItem(index));
            }
            audio = read;
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
            view->setTitle(textOf(file.path.filename()));
            view->setBuffer(currentBuffer());
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

        void updateRow(int index) {
            const auto item = list->topLevelItem(index);
            const auto &file = files.at(index);
            item->setText(1, presence(file.hasMap));
            const auto caches = format.analysisCaches();
            for (qsizetype i = 0; i < caches.size(); ++i) {
                item->setText(int(2 + i), presence(file.caches.value(i)));
            }
            const auto entry = openedOf(index);
            item->setText(modifiedColumn, presence(entry && entry->buffer->isModified()));
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
            if (current >= 0) {
                updateRow(current);
            }
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
                        .arg(textOf(mapFile->filename()), textOf(file.path.filename())),
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
            addCommand(QStringLiteral("helloutau.frequencyMap.revert"), [this] {
                edit([](auto &buffer) { return buffer.revert(0, buffer.map().frames.size()); });
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
            const auto relative = file.path.lexically_relative(root);
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

        auto editing = new QWidget();
        auto editingLayout = new QVBoxLayout(editing);
        editingLayout->setContentsMargins(0, 0, 0, 0);
        editingLayout->addWidget(impl.message);
        editingLayout->addWidget(impl.view, 1);
        editingLayout->addWidget(impl.removeCaches);
        editingLayout->addWidget(removeCachesNote);

        auto splitter = new QSplitter();
        splitter->addWidget(impl.list);
        splitter->addWidget(editing);
        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 3);

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Close);
        auto menuBar = new QMenuBar();
        auto layout = new QVBoxLayout(this);
        layout->setMenuBar(menuBar);
        layout->addWidget(splitter, 1);
        layout->addWidget(buttons);
        resize(1000, 640);
        impl.initActions(menuBar);

        connect(buttons, &QDialogButtonBox::rejected, this, &EditFrequencyMapsDialog::reject);
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
        connect(impl.view, &FrequencyMapView::viewModeChanged, this, [this] {
            stdc_impl_t;
            impl.updateSpectrogram();
        });
        connect(impl.view, &FrequencyMapView::spectrumChanged, this, [this] {
            stdc_impl_t;
            impl.updateColorActions();
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
            const auto answer = QMessageBox::question(
                this, tr("Edit Frequency Maps"),
                tr("Save the modified frequency maps before closing?"),
                QMessageBox::SaveAll | QMessageBox::Discard | QMessageBox::Cancel,
                QMessageBox::SaveAll);
            if (answer == QMessageBox::Cancel || (answer == QMessageBox::SaveAll && !saveAll())) {
                return;
            }
        }
        QDialog::reject();
    }

}
