#include "MainWindow.h"

#include <QtCore/QDir>
#include <QtCore/QHash>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtGui/QActionGroup>
#include <QtGui/QCloseEvent>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>

#include <QAKCore/actionextension.h>
#include <QAKCore/actionregistry.h>
#include <QAKWidgets/widgetactioncontext.h>

#include <hellokit/Edit/ProjectDocument.h>
#include <hellokit/Edit/ProjectEdits.h>
#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Edit/TrackTimeline.h>

#include <helloutau/Widgets/CommandPalette.h>

#include "AppSettings.h"
#include "DiagnosticBox_p.h"
#include "Editor.h"
#include "ExportUstDialog.h"
#include "PianoRoll.h"
#include "SettingsDialog.h"
#include "VoiceBankCharsetDialog.h"

namespace hello::daw {

    namespace {

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
        Impl(MainWindow *decl, Editor *editor) : _decl(decl), editor(editor) {
        }

        MainWindow *_decl;
        Editor *editor;
        std::unique_ptr<kit::ProjectDocument> document;
        PianoRoll *roll = nullptr;
        QAK::WidgetActionContext *context = nullptr;
        QHash<QString, QAction *> actions;
        QActionGroup *tools = nullptr;
        CommandPalette *palette = nullptr;

        QAction *addCommand(const QString &id, std::function<void()> handler) {
            auto action = new QAction(_decl);
            QObject::connect(action, &QAction::triggered, _decl, std::move(handler));
            context->addAction(id, action);
            actions.insert(id, action);
            return action;
        }

        void initActions() {
            context = new QAK::WidgetActionContext(_decl);
            context->addMenuBar(QStringLiteral("helloutau.mainMenu"), _decl->menuBar());

            addCommand(QStringLiteral("helloutau.file.new"), [this] { editor->newWindow(); });
            addCommand(QStringLiteral("helloutau.file.open"), [this] { open(); });
            addCommand(QStringLiteral("helloutau.file.save"), [this] { _decl->save(); });
            addCommand(QStringLiteral("helloutau.file.saveAs"), [this] { _decl->saveAs(); });
            addCommand(QStringLiteral("helloutau.file.exportUst"), [this] { _decl->exportUst(); });
            addCommand(QStringLiteral("helloutau.file.close"), [this] { _decl->close(); });
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

            tools = new QActionGroup(_decl);
            const auto selectTool = addCommand(QStringLiteral("helloutau.edit.selectTool"),
                                               [this] { roll->setTool(PianoRoll::SelectTool); });
            const auto penTool = addCommand(QStringLiteral("helloutau.edit.penTool"),
                                            [this] { roll->setTool(PianoRoll::PenTool); });
            for (const auto action : {selectTool, penTool}) {
                action->setCheckable(true);
                tools->addAction(action);
            }
            selectTool->setChecked(true);
            addCommand(QStringLiteral("helloutau.view.commandPalette"), [this] {
                palette->setCommands(commandEntries());
                palette->setRecentIds(editor->settings().recentCommands());
                palette->popup();
            });
            addCommand(QStringLiteral("helloutau.tools.settings"), [this] {
                const auto utau = editor->settings().utauDirectory();
                SettingsDialog dialog(editor->settings(), _decl);
                if (dialog.exec() == QDialog::Accepted &&
                    editor->settings().utauDirectory() != utau) {
                    // Every voice bank named relative to UTAU is now elsewhere.
                    for (const auto window : editor->windows()) {
                        window->loadVoiceBank();
                    }
                }
            });

            const auto registry = editor->actionRegistry();
            registry->addContext(context);
            for (const auto element :
                 {QAK::AE_Layouts, QAK::AE_Texts, QAK::AE_Keymap, QAK::AE_Icons}) {
                registry->updateContext(element);
            }

            palette = new CommandPalette(_decl);
            QObject::connect(palette, &CommandPalette::commandActivated, _decl,
                             [this](const QString &id) {
                                 editor->settings().addRecentCommand(id);
                                 // Run once the key press that chose it is over, since the command
                                 // may open a dialog. It may have been disabled in between.
                                 QTimer::singleShot(0, _decl, [this, id] {
                                     if (const auto action = context->action(id);
                                         action && action->isEnabled()) {
                                         action->trigger();
                                     }
                                 });
                             });
        }

        // The commands of the window as the command palette offers them: every action that is a
        // command and is enabled now, as VS Code shows no disabled command, labelled with its
        // category as "File: Save". The palette does not list itself.
        QList<CommandEntry> commandEntries() const {
            const auto registry = editor->actionRegistry();
            QList<CommandEntry> entries;
            for (const auto &id : registry->actionIds()) {
                const auto info = registry->actionInfo(id);
                const auto action = context->action(id);
                if (!info || !info->isCommand() || !action || !action->isEnabled() ||
                    id == QStringLiteral("helloutau.view.commandPalette")) {
                    continue;
                }
                const auto label = [](const QAK::ActionText &category,
                                      const QAK::ActionText &text) {
                    const auto title = text.withoutMnemonic();
                    const auto group = category.withoutMnemonic();
                    return group.isEmpty() ? title : group + QStringLiteral(": ") + title;
                };
                const auto category = info->category();
                const auto text = info->text();
                const auto shown = label(category, text);
                const auto source =
                    label({category.source, std::nullopt}, {text.source, std::nullopt});
                entries.push_back({id, shown, source == shown ? QString() : source,
                                   action->shortcut(), action->isCheckable(), action->isChecked()});
            }
            return entries;
        }

        void bindDocument() {
            // Replaces the piano roll of the previous document, which is deleted with it. The
            // tool and the quantization belong to the window and carry over.
            const auto quantization = roll ? roll->quantization() : -1;
            roll = new PianoRoll(document->session());
            roll->setVoiceBank(document->voiceBank());
            if (quantization >= 0) {
                roll->setQuantization(quantization);
            }
            roll->setTool(actions.value(QStringLiteral("helloutau.edit.penTool"))->isChecked()
                              ? PianoRoll::PenTool
                              : PianoRoll::SelectTool);
            _decl->setCentralWidget(roll);

            QObject::connect(document.get(), &kit::ProjectDocument::voiceBankChanged, roll,
                             [this] { roll->setVoiceBank(document->voiceBank()); });
            QObject::connect(roll, &PianoRoll::selectionChanged, _decl,
                             [this] { updateEditActions(); });
            updateEditActions();
            QObject::connect(document.get(), &kit::ProjectDocument::modifiedChanged, _decl,
                             [this] { updateTitle(); });
            QObject::connect(document.get(), &kit::ProjectDocument::filePathChanged, _decl,
                             [this] { updateTitle(); });
            QObject::connect(document->session(), &kit::ProjectSession::stepChanged, _decl,
                             [this] { updateUndoActions(); });
            updateTitle();
            updateUndoActions();
        }

        void updateTitle() {
            const auto name = document->displayName();
            _decl->setWindowTitle(
                QStringLiteral("%1[*] - HelloUtau").arg(name.isEmpty() ? tr("Untitled") : name));
            _decl->setWindowModified(document->isModified());
        }

        // Enables the commands that act on the selection when there is one.
        void updateEditActions() {
            const int selected = int(roll->selectedIndices().size());
            for (const auto id : {"helloutau.edit.delete", "helloutau.edit.editLyric",
                                  "helloutau.edit.transposeUp", "helloutau.edit.transposeDown",
                                  "helloutau.edit.octaveUp", "helloutau.edit.octaveDown"}) {
                actions.value(QLatin1String(id))->setEnabled(selected > 0);
            }
            actions.value(QStringLiteral("helloutau.edit.splitNote"))->setEnabled(selected == 1);
        }

        // Performs an edit of the piano roll and shows why it was refused, if it was. Nothing
        // is edited while a lyric is, since the editor has the keyboard.
        void edit(const QString &title, const std::function<bool(kit::DiagnosticList &)> &run) {
            if (roll->lyricEditor()->isVisible()) {
                return;
            }
            kit::DiagnosticList diagnostics;
            run(diagnostics);
            DiagnosticBox::show(_decl, title, diagnostics);
        }

        // Splits the selected note after a length that the user enters.
        void splitNote() {
            const auto indices = roll->selectedIndices();
            if (indices.size() != 1) {
                return;
            }
            const int index = indices.first();
            const int length = roll->timeline()->note(index).length;
            if (length < 2) {
                QMessageBox::information(_decl, tr("Split Note"),
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
                QInputDialog::getInt(_decl, tr("Split Note"),
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
            const auto file = QFileDialog::getOpenFileName(
                _decl, tr("Open"), {},
                tr("Projects (*.usth *.ust);;HelloUtau projects (*.usth);;UTAU projects "
                   "(*.ust);;All files (*)"));
            if (!file.isEmpty()) {
                editor->openFile(pathOf(file), _decl);
            }
        }

        // Asks whether to save a modified project before it is closed. Returns whether closing
        // may proceed.
        bool maybeSave() {
            if (!document->isModified()) {
                return true;
            }
            const auto name = document->displayName();
            const auto answer = QMessageBox::warning(
                _decl, tr("HelloUtau"),
                tr("Save the changes to %1?").arg(name.isEmpty() ? tr("Untitled") : name),
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
            if (answer == QMessageBox::Save) {
                return _decl->save();
            }
            return answer == QMessageBox::Discard;
        }

        static QString tr(const char *text) {
            return MainWindow::tr(text);
        }
    };

    MainWindow::MainWindow(Editor *editor, std::unique_ptr<kit::ProjectDocument> document)
        : _impl(std::make_unique<Impl>(this, editor)) {
        _impl->initActions();
        _impl->document = std::move(document);
        _impl->bindDocument();
        resize(960, 640);
    }

    MainWindow::~MainWindow() {
        // The piano roll refers to the session of the document, which goes with _impl.
        delete _impl->roll;
    }

    kit::ProjectDocument *MainWindow::document() const {
        return _impl->document.get();
    }

    void MainWindow::setDocument(std::unique_ptr<kit::ProjectDocument> document) {
        auto previous = std::move(_impl->document);
        _impl->document = std::move(document);
        _impl->bindDocument();
    }

    bool MainWindow::isUnused() const {
        return _impl->document->sourcePath().empty() && !_impl->document->isModified();
    }

    bool MainWindow::loadVoiceBank() {
        const auto document = _impl->document.get();
        const auto utau = _impl->editor->settings().utauDirectory();
        VoiceBankCharsetDialog selector(this);
        selector.setRoot(document->session()->snapshot().tracks.value(0).voiceDirectory(utau));
        kit::DiagnosticList diagnostics;
        const bool loaded = document->loadVoiceBank(utau, &selector, diagnostics);
        DiagnosticBox::show(this, tr("Voice Bank"), diagnostics);
        return loaded;
    }

    bool MainWindow::save() {
        if (_impl->document->filePath().empty()) {
            return saveAs();
        }
        kit::DiagnosticList diagnostics;
        const bool saved = _impl->document->save(diagnostics);
        DiagnosticBox::show(this, tr("Save"), diagnostics);
        return saved;
    }

    bool MainWindow::saveAs() {
        const auto file = QFileDialog::getSaveFileName(
            this, tr("Save As"), textOf(proposedPath(*_impl->document, u".usth")),
            tr("HelloUtau projects (*.usth)"));
        if (file.isEmpty()) {
            return false;
        }
        auto path = pathOf(file);
        if (path.extension() != u".usth") {
            path += u".usth";
        }
        kit::DiagnosticList diagnostics;
        const bool saved = _impl->document->saveAs(path, diagnostics);
        DiagnosticBox::show(this, tr("Save As"), diagnostics);
        return saved;
    }

    bool MainWindow::exportUst() {
        const auto &settings = _impl->editor->settings();
        ExportUstDialog dialog(proposedPath(*_impl->document, u".ust"), settings.ustExportCharset(),
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
        const bool exported = _impl->document->exportUst(path, options, diagnostics);
        DiagnosticBox::show(this, tr("Export UST"), diagnostics);
        return exported;
    }

    void MainWindow::closeEvent(QCloseEvent *event) {
        if (!_impl->maybeSave()) {
            event->ignore();
            return;
        }
        QMainWindow::closeEvent(event);
    }

}
