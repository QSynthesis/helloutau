#include <filesystem>
#include <fstream>

#include <QtCore/QTemporaryDir>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtTest/QTest>
#include <QtGui/QClipboard>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QGraphicsDropShadowEffect>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTreeWidget>

#include <hellokit/Edit/ProjectDocument.h>
#include <hellokit/Edit/ProjectEdits.h>
#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Edit/VoiceBankDocument.h>
#include <hellokit/Edit/VoiceBankRefs.h>

#include <helloutau/Widgets/CommandPalette.h>

#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/MainWindow.h>
#include <helloutau/Editor/PianoRoll.h>
#include <helloutau/Editor/PasteParametersDialog.h>
#include <helloutau/Editor/ScalePitchDialog.h>
#include <helloutau/Editor/VibratoDialog.h>
#include <helloutau/Editor/VoiceBankCharsetDialog.h>
#include <helloutau/Editor/VoiceBankEntryModel.h>
#include <helloutau/Editor/VoiceBankWindow.h>

using namespace hello;
using namespace hello::daw;
namespace fs = std::filesystem;

namespace {

    fs::path pathIn(const QTemporaryDir &dir, const char *name) {
        return fs::path(dir.path().toStdU16String()) / name;
    }

    fs::path savedProject(const QTemporaryDir &dir, const char *name) {
        kit::Note note;
        note.lyric = QStringLiteral("la");
        note.length = 480;
        note.noteNum = 60;
        kit::Track track;
        track.notes.push_back(note);
        kit::Project project;
        project.tracks.push_back(track);

        const auto path = pathIn(dir, name);
        kit::DiagnosticList diagnostics;
        project.save(path, diagnostics);
        return path;
    }

    void edit(MainWindow *window) {
        const auto session = window->document()->session();
        auto tx = session->transaction(QStringLiteral("rename"));
        kit::ProjectRef(session).tracks().at(0).setVoiceDir(QStringLiteral("changed"));
        tx.commit();
    }

    QAction *actionNamed(QWidget *window, const QString &text) {
        for (const auto action : window->findChildren<QAction *>()) {
            if (action->text() == text) {
                return action;
            }
        }
        return nullptr;
    }

    // Answers the next message box with \a button once it appears.
    void answerMessageBox(QMessageBox::StandardButton button) {
        QTimer::singleShot(0, [button] {
            const auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(button)->click();
        });
    }

}

class test_Editor : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    std::unique_ptr<Editor> editor() const {
        return std::make_unique<Editor>(
            std::make_unique<AppSettings>(m_dir.filePath(QStringLiteral("settings.ini"))));
    }

private Q_SLOTS:
    void a_new_window_has_the_menus_of_the_manifest() {
        const auto e = editor();
        const auto window = e->newWindow();
        QCOMPARE(e->windows().size(), 1);
        QCOMPARE(window->windowTitle(), QStringLiteral("Untitled[*] - HelloUtau"));

        QStringList menus;
        for (const auto action : window->menuBar()->actions()) {
            menus.push_back(action->text());
        }
        QCOMPARE(menus, (QStringList{QStringLiteral("&File"), QStringLiteral("&Edit"),
                                     QStringLiteral("&View"), QStringLiteral("&Playback"),
                                     QStringLiteral("&Tools")}));
        const auto save = actionNamed(window, QStringLiteral("&Save"));
        QVERIFY(save);
        QCOMPARE(save->shortcut(), QKeySequence(QStringLiteral("Ctrl+S")));

        // Exporting is a menu of formats, which MIDI and others will join.
        const auto file = window->menuBar()->actions().first()->menu();
        QMenu *exportMenu = nullptr;
        for (const auto action : file->actions()) {
            if (action->text() == QStringLiteral("&Export")) {
                exportMenu = action->menu();
            }
        }
        QVERIFY(exportMenu);
        QCOMPARE(exportMenu->actions().size(), 1);
        QCOMPARE(exportMenu->actions().first()->text(), QStringLiteral("&UST..."));
    }

    void undo_and_the_modified_mark_follow_the_project() {
        const auto e = editor();
        const auto window = e->newWindow();
        const auto undo = actionNamed(window, QStringLiteral("&Undo"));
        QVERIFY(undo);
        QVERIFY(!undo->isEnabled());

        edit(window);
        QVERIFY(undo->isEnabled());
        QVERIFY(window->isWindowModified());

        undo->trigger();
        QVERIFY(!undo->isEnabled());
        QVERIFY(!window->isWindowModified());
    }

    // An unmodified new project is replaced; any other window keeps its project.
    void a_file_opens_in_an_unused_window_or_a_new_one() {
        const auto e = editor();
        const auto first = e->newWindow();
        const auto a = savedProject(m_dir, "a.usth");
        const auto b = savedProject(m_dir, "b.usth");

        QCOMPARE(e->openFile(a, first), first);
        QCOMPARE(first->windowTitle(), QStringLiteral("a.usth[*] - HelloUtau"));
        QVERIFY(!first->isUnused());

        const auto second = e->openFile(b, first);
        QVERIFY(second && second != first);
        QCOMPARE(e->windows().size(), 2);

        // A file that is already open is not opened twice.
        QCOMPARE(e->openFile(a, second), first);
        QCOMPARE(e->windows().size(), 2);
    }

    // As in VS Code, the palette lists the commands that are enabled now, each after its
    // category, and not itself.
    void the_command_palette_offers_the_enabled_commands() {
        const auto e = editor();
        const auto window = e->newWindow();
        const auto open = actionNamed(window, QStringLiteral("&Command Palette..."));
        QVERIFY(open);
        QCOMPARE(open->shortcuts(),
                 (QList<QKeySequence>{QKeySequence(QStringLiteral("Ctrl+Shift+P")),
                                      QKeySequence(Qt::Key_F1)}));

        open->trigger();
        const auto palette = window->findChild<CommandPalette *>();
        QVERIFY(palette && palette->isVisible());
        const auto ids = palette->shownIds();
        QVERIFY(ids.contains(QStringLiteral("helloutau.file.save")));
        QVERIFY(!ids.contains(QStringLiteral("helloutau.edit.undo")));
        QVERIFY(!ids.contains(QStringLiteral("helloutau.view.commandPalette")));
        for (const auto &entry : palette->commands()) {
            if (entry.id == QStringLiteral("helloutau.file.save")) {
                QCOMPARE(entry.label, QStringLiteral("File: Save"));
                QCOMPARE(entry.shortcut, QKeySequence(QStringLiteral("Ctrl+S")));
            }
        }
        QTest::keyClick(palette->findChild<QLineEdit *>(), Qt::Key_Escape);

        // Once there is something to undo, the palette offers Undo and runs it.
        edit(window);
        open->trigger();
        palette->setQuery(QStringLiteral("undo"));
        QCOMPARE(palette->currentId(), QStringLiteral("helloutau.edit.undo"));
        QTest::keyClick(palette->findChild<QLineEdit *>(), Qt::Key_Return);

        // The command runs once the key press is over, and is remembered as recently used.
        QTRY_VERIFY(!window->document()->isModified());
        QCOMPARE(e->settings().recentCommands().first(), QStringLiteral("helloutau.edit.undo"));
    }

    // The shadow of the palette comes from the built-in theme, which applies to every window.
    void the_built_in_theme_gives_the_palette_its_shadow() {
        const auto e = editor();
        const auto window = e->newWindow();
        const auto palette = window->findChild<CommandPalette *>();
        QVERIFY(palette);
        palette->ensurePolished();
        const auto effect = qobject_cast<QGraphicsDropShadowEffect *>(palette->graphicsEffect());
        QVERIFY(effect);
        QCOMPARE(effect->color(), QColor(0, 0, 0, 0x40));
        QCOMPARE(effect->blurRadius(), 16.0);
        QCOMPARE(effect->offset(), QPointF(0, 4));
    }

    void closing_a_modified_project_asks_to_save() {
        const auto e = editor();
        const auto window = e->newWindow();
        edit(window);

        answerMessageBox(QMessageBox::Cancel);
        QVERIFY(!window->close());
        QCOMPARE(e->windows().size(), 1);

        answerMessageBox(QMessageBox::Discard);
        QVERIFY(window->close());
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(e->windows().size(), 0);
    }

    void saving_a_project_with_its_file_needs_no_dialog() {
        const auto e = editor();
        const auto window = e->openFile(savedProject(m_dir, "c.usth"));
        QVERIFY(window);
        edit(window);
        QVERIFY(window->save());
        QVERIFY(!window->isWindowModified());
    }

    // The commands on the selection are enabled by it, and the tool and the quantization stay
    // with the window when it shows another project.
    // The vibrato command opens the dialog with that of the first selected note, or the
    // default, and gives what it accepts to every selected note.
    void the_vibrato_of_the_selected_notes_is_edited() {
        const auto e = editor();
        const auto window = e->openFile(savedProject(m_dir, "v.usth"));
        QVERIFY(window);
        auto roll = qobject_cast<PianoRoll *>(window->centralWidget());
        const auto edit = actionNamed(window, QStringLiteral("Vibra&to..."));
        QVERIFY(edit);
        QVERIFY(!edit->isEnabled());

        roll->selectAll();
        QVERIFY(edit->isEnabled());
        double shown = 0;
        QTimer::singleShot(0, [&shown] {
            const auto dialog = qobject_cast<VibratoDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            shown = dialog->field(1)->value();
            dialog->field(1)->setValue(240);
            dialog->accept();
        });
        edit->trigger();
        QCOMPARE(shown, 180.0);
        const auto vibrato = window->document()->session()->snapshot().tracks[0].notes[0].vibrato;
        QVERIFY(vibrato);
        QCOMPARE(vibrato->period, 240.0);
        QCOMPARE(vibrato->length, 65.0);
    }

    // Copy enables Paste Parameters, whose dialog chooses what to paste onto the selection.
    void parameters_are_pasted_through_the_menu() {
        const auto e = editor();
        const auto window = e->openFile(savedProject(m_dir, "p.usth"));
        QVERIFY(window);
        auto roll = qobject_cast<PianoRoll *>(window->centralWidget());
        QGuiApplication::clipboard()->clear();
        const auto paste = actionNamed(window, QStringLiteral("Paste Para&meters..."));
        const auto pasteNotes = actionNamed(window, QStringLiteral("&Paste"));
        QVERIFY(paste && pasteNotes);

        roll->selectAll();
        QVERIFY(!paste->isEnabled());
        QVERIFY(!pasteNotes->isEnabled());
        kit::DiagnosticList diagnostics;
        roll->toggleVibrato(diagnostics);
        actionNamed(window, QStringLiteral("&Copy"))->trigger();
        QVERIFY(paste->isEnabled());
        QVERIFY(pasteNotes->isEnabled());
        // Notes are pasted after the last note without a selection, parameters onto none.
        roll->setSelectedIndices({});
        QVERIFY(pasteNotes->isEnabled());
        QVERIFY(!paste->isEnabled());
        roll->selectAll();

        roll->toggleVibrato(diagnostics);
        QVERIFY(!window->document()->session()->snapshot().tracks[0].notes[0].vibrato);
        QTimer::singleShot(0, [] {
            const auto dialog =
                qobject_cast<PasteParametersDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->box(PianoRoll::PortamentoParameter)->setChecked(false);
            dialog->box(PianoRoll::EnvelopeParameter)->setChecked(false);
            dialog->accept();
        });
        paste->trigger();
        QVERIFY(window->document()->session()->snapshot().tracks[0].notes[0].vibrato);
    }

    // Scale Pitch scales the vibrato of the selection by the factor entered in its dialog.
    void the_pitch_is_scaled_through_the_menu() {
        const auto e = editor();
        const auto window = e->openFile(savedProject(m_dir, "s.usth"));
        QVERIFY(window);
        auto roll = qobject_cast<PianoRoll *>(window->centralWidget());
        const auto scale = actionNamed(window, QStringLiteral("Scale Pitc&h..."));
        QVERIFY(scale);
        QVERIFY(!scale->isEnabled());

        roll->selectAll();
        QVERIFY(scale->isEnabled());
        kit::DiagnosticList diagnostics;
        roll->toggleVibrato(diagnostics);
        const auto depth = [window] {
            return window->document()->session()->snapshot().tracks[0].notes[0].vibrato->amplitude;
        };
        const double before = depth();
        QTimer::singleShot(0, [] {
            const auto dialog = qobject_cast<ScalePitchDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->vibratoBox()->value(), 100.0);
            dialog->vibratoBox()->setValue(200);
            dialog->accept();
        });
        scale->trigger();
        QCOMPARE(depth(), before * 2);
    }

    void nothing_chosen_pastes_nothing() {
        PasteParametersDialog dialog(PianoRoll::VibratoParameter);
        const auto ok = dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        QVERIFY(ok->isEnabled());
        QCOMPARE(dialog.parameters(), PianoRoll::Parameters(PianoRoll::VibratoParameter));
        dialog.box(PianoRoll::VibratoParameter)->setChecked(false);
        QVERIFY(!ok->isEnabled());
    }

    // An opened file heads Open Recent, from which it opens again; a file gone is reported and
    // forgotten.
    void recent_files_open_from_their_menu() {
        const auto e = editor();
        e->settings().clearRecentFiles();
        const auto first = savedProject(m_dir, "r1.usth");
        const auto second = savedProject(m_dir, "r2.usth");
        e->openFile(first);
        const auto window = e->openFile(second);
        QVERIFY(window);
        QCOMPARE(e->settings().recentFiles(), (QList<std::filesystem::path>{second, first}));

        // The menu of the external action, filled when it opens
        const auto recent = actionNamed(window, QStringLiteral("Open &Recent"));
        QVERIFY(recent && recent->menu());
        Q_EMIT recent->menu()->aboutToShow();
        const auto items = recent->menu()->actions();
        QVERIFY(items.first()->text().startsWith(QStringLiteral("&1 ")));
        QVERIFY(items.first()->text().endsWith(QStringLiteral("r2.usth")));

        // Opening one already open activates its window.
        items.at(1)->trigger();
        QCOMPARE(e->windows().size(), 2);

        std::filesystem::remove(first);
        answerMessageBox(QMessageBox::Ok);
        items.at(1)->trigger();
        QCOMPARE(e->settings().recentFiles(), (QList<std::filesystem::path>{second}));
    }

    // Why an edit in the piano roll was refused appears in the status bar.
    void a_refused_edit_is_reported_in_the_status_bar() {
        const auto e = editor();
        const auto window = e->newWindow();
        auto roll = qobject_cast<PianoRoll *>(window->centralWidget());
        QVERIFY(roll);
        Q_EMIT roll->editRefused(QStringLiteral("The point is before the one before it."));
        QCOMPARE(window->statusBar()->currentMessage(),
                 QStringLiteral("The point is before the one before it."));
    }

    void the_edit_commands_follow_the_selection() {
        const auto e = editor();
        const auto window = e->openFile(savedProject(m_dir, "d.usth"));
        QVERIFY(window);
        auto roll = qobject_cast<PianoRoll *>(window->centralWidget());
        const auto remove = actionNamed(window, QStringLiteral("&Delete"));
        const auto split = actionNamed(window, QStringLiteral("S&plit Note..."));
        QVERIFY(remove && split);
        QVERIFY(!remove->isEnabled());
        QVERIFY(!split->isEnabled());

        roll->selectAll();
        QVERIFY(remove->isEnabled());
        QVERIFY(split->isEnabled());

        // Splitting proposes half the note on the grid, and asks for the first part.
        int proposed = 0;
        QTimer::singleShot(0, [&proposed] {
            const auto dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            proposed = dialog->intValue();
            dialog->setIntValue(120);
            dialog->accept();
        });
        split->trigger();
        QCOMPARE(proposed, 240);
        const auto notes = window->document()->session()->snapshot().tracks[0].notes;
        QCOMPARE(notes.size(), 2);
        QCOMPARE(notes[0].length, 120);
        QCOMPARE(notes[1].length, 360);

        roll->selectAll();
        QVERIFY(!split->isEnabled());
        remove->trigger();
        QVERIFY(window->document()->session()->snapshot().tracks[0].notes.isEmpty());
        QVERIFY(!remove->isEnabled());

        actionNamed(window, QStringLiteral("P&en Tool"))->trigger();
        QCOMPARE(roll->tool(), PianoRoll::PenTool);
        roll->setQuantization(60);
        window->setDocument(std::make_unique<kit::ProjectDocument>());
        roll = qobject_cast<PianoRoll *>(window->centralWidget());
        QCOMPARE(roll->tool(), PianoRoll::PenTool);
        QCOMPARE(roll->quantization(), 60);
    }

    // Mode2 is checked as the project has it, and turned over as an undo step. The pitch tool is
    // enabled while Mode2 is off and the pitch shown, and otherwise gives way to the select
    // tool.
    void mode2_follows_the_project() {
        const auto e = editor();
        const auto window = e->newWindow();
        const auto roll = qobject_cast<PianoRoll *>(window->centralWidget());
        const auto session = window->document()->session();
        const auto mode2 = actionNamed(window, QStringLiteral("Mode&2 Pitch"));
        const auto pitchTool = actionNamed(window, QStringLiteral("&Freehand Pitch Tool"));
        const auto showPitch = actionNamed(window, QStringLiteral("Show &Pitch"));
        QVERIFY(mode2 && pitchTool && showPitch);
        QVERIFY(mode2->isChecked());
        QVERIFY(!pitchTool->isEnabled());

        mode2->trigger();
        QVERIFY(!mode2->isChecked());
        QVERIFY(!session->snapshot().settings.mode2);
        QCOMPARE(session->undoMessage(), kit::ProjectEdits::tr("Turn Mode2 Off"));
        QVERIFY(pitchTool->isEnabled());
        pitchTool->trigger();
        QCOMPARE(roll->tool(), PianoRoll::PitchTool);

        showPitch->trigger();
        QVERIFY(!pitchTool->isEnabled());
        QCOMPARE(roll->tool(), PianoRoll::SelectTool);
        showPitch->trigger();
        pitchTool->trigger();
        QCOMPARE(roll->tool(), PianoRoll::PitchTool);

        actionNamed(window, QStringLiteral("&Undo"))->trigger();
        QVERIFY(mode2->isChecked());
        QVERIFY(session->snapshot().settings.mode2);
        QCOMPARE(roll->tool(), PianoRoll::SelectTool);
    }

    // The voice bank is found through the UTAU folder of the settings, the user is asked for
    // the encoding of its folder, and the piano roll marks the note it has no sample for.
    void an_opened_project_reads_its_voice_bank() {
        QTemporaryDir dir;
        const auto utau = pathIn(dir, "utau");
        const auto bank = utau / "voice" / "bank";
        fs::create_directories(bank);
        {
            // The alias あ in Shift_JIS
            std::ofstream oto(bank / "oto.ini", std::ios::binary);
            oto << "a.wav=\x82\xa0,0,0,0,0,0\r\n";
            std::ofstream wav(bank / "a.wav", std::ios::binary);
        }

        kit::Note a;
        a.lyric = QString::fromUtf8("あ");
        a.length = 480;
        a.noteNum = 60;
        kit::Note la = a;
        la.lyric = QStringLiteral("la");
        kit::Track track;
        track.voiceDir = QStringLiteral("%VOICE%bank");
        track.notes = {a, la};
        kit::Project project;
        project.tracks.push_back(track);
        const auto path = pathIn(dir, "voiced.usth");
        kit::DiagnosticList diagnostics;
        QVERIFY(project.save(path, diagnostics));

        const auto e = editor();
        e->settings().setUtauDirectory(utau);
        QStringList asked;
        QTimer::singleShot(0, [&asked] {
            const auto dialog =
                qobject_cast<VoiceBankCharsetDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            asked.push_back(dialog->windowTitle());
            dialog->setSelectedCharset(QStringLiteral("Shift_JIS"));
            dialog->accept();
        });
        const auto window = e->openFile(path);
        e->settings().setUtauDirectory({});
        QVERIFY(window);
        QCOMPARE(asked, QStringList{QStringLiteral("Choose Encoding - bank")});
        QVERIFY(fs::is_regular_file(bank / "hello-config.json"));

        const auto roll = qobject_cast<PianoRoll *>(window->centralWidget());
        QVERIFY(roll);
        QVERIFY(roll->voiceBank());
        QCOMPARE(roll->voiceBank(), window->document()->voiceBank());
        QVERIFY(!roll->lacksSample(0));
        QVERIFY(roll->lacksSample(1));
    }

private:
    // A voice bank in UTF-8, declared, so that nothing is asked: in the root a.wav with an
    // entry, b.wav an entry without its file and c.wav a file without an entry; in sub, x.wav.
    static fs::path voiceBank(const QTemporaryDir &dir, const char *name = "bank") {
        const auto bank = pathIn(dir, name);
        fs::create_directories(bank / "sub");
        const auto write = [](const fs::path &path, const char *text) {
            std::ofstream file(path, std::ios::binary);
            file << text;
        };
        write(bank / "oto.ini",
              "#Charset:UTF-8\r\na.wav=a,10,20,-30,40,5\r\nb.wav=b,1,2,3,4,5\r\n");
        write(bank / "a.wav", "");
        write(bank / "c.wav", "");
        write(bank / "sub" / "oto.ini", "#Charset:UTF-8\r\nx.wav=x,9.0,2,3,4,5\r\n");
        write(bank / "sub" / "x.wav", "");
        return bank;
    }

    static QList<int> kindsOf(const VoiceBankWindow *window) {
        const auto table = window->entryTable();
        QList<int> kinds;
        for (int row = 0; row < table->model()->rowCount(); ++row) {
            kinds.push_back(
                table->model()->index(row, 0).data(VoiceBankEntryModel::RowKindRole).toInt());
        }
        return kinds;
    }

    static bool isSame(const fs::path &a, const fs::path &b) {
        std::error_code error;
        return fs::equivalent(a, b, error);
    }

private Q_SLOTS:
    // A voice bank opens in a window of its own, its folders in a tree and the entries of the
    // folder chosen there in a table, with the files without an entry among them.
    void a_voice_bank_opens_in_a_window_of_its_own() {
        QTemporaryDir dir;
        const auto bank = voiceBank(dir);
        const auto e = editor();
        const auto window = e->openVoiceBank(bank);
        QVERIFY(window);
        QCOMPARE(e->voiceBankWindows(), QList<VoiceBankWindow *>{window});
        QCOMPARE(window->windowTitle(), QStringLiteral("bank[*] - HelloUtau"));
        QVERIFY(!window->isWindowModified());
        QStringList menus;
        for (const auto action : window->menuBar()->actions()) {
            menus.push_back(action->text());
        }
        QCOMPARE(menus, (QStringList{QStringLiteral("&File"), QStringLiteral("&Edit"),
                                     QStringLiteral("&View"), QStringLiteral("&Tools")}));

        // All folders at first: a, b missing, c unlisted, and x in sub
        const auto tree = window->directoryTree();
        QCOMPARE(tree->topLevelItemCount(), 2);
        QCOMPARE(tree->topLevelItem(1)->text(0), QStringLiteral("bank"));
        QCOMPARE(tree->topLevelItem(1)->child(0)->text(0), QStringLiteral("sub"));
        QCOMPARE(
            kindsOf(window),
            (QList<int>{VoiceBankEntryModel::EntryRow, VoiceBankEntryModel::MissingAudioRow,
                        VoiceBankEntryModel::UnlistedAudioRow, VoiceBankEntryModel::EntryRow}));
        QVERIFY(!window->entryTable()->isColumnHidden(VoiceBankEntryModel::DirectoryColumn));

        tree->setCurrentItem(tree->topLevelItem(1)->child(0));
        QCOMPARE(kindsOf(window), QList<int>{VoiceBankEntryModel::EntryRow});
        QVERIFY(window->entryTable()->isColumnHidden(VoiceBankEntryModel::DirectoryColumn));
        const auto model = window->entryTable()->model();
        QCOMPARE(model->index(0, VoiceBankEntryModel::OffsetColumn).data().toString(),
                 QStringLiteral("9.0"));

        // The search matches file names and aliases, not the values.
        tree->setCurrentItem(tree->topLevelItem(0));
        window->searchBox()->setText(QStringLiteral("C"));
        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(model->index(0, VoiceBankEntryModel::FileColumn).data().toString(),
                 QStringLiteral("c.wav"));
        window->searchBox()->setText(QStringLiteral("9"));
        QCOMPARE(model->rowCount(), 0);

        // Opened again, the same window is shown.
        QCOMPARE(e->openVoiceBank(bank), window);
        QCOMPARE(e->voiceBankWindows().size(), 1);
    }

    // An edit makes the voice bank modified; saving writes it, and closing asks first.
    void the_voice_bank_window_saves_and_asks_before_closing() {
        QTemporaryDir dir;
        const auto bank = voiceBank(dir);
        const auto e = editor();
        const auto window = e->openVoiceBank(bank);
        QVERIFY(window);
        const auto rename = [window](const QString &alias) {
            const auto session = window->document()->session();
            auto transaction = session->transaction(QStringLiteral("rename"));
            kit::VoiceBankRef(session).directories().at(0).otoEntries().at(0).setAlias(alias);
            QVERIFY(transaction.commit());
        };
        rename(QStringLiteral("renamed"));
        QVERIFY(window->isWindowModified());
        QVERIFY(actionNamed(window, QStringLiteral("&Undo"))->isEnabled());
        QCoreApplication::processEvents();
        QCOMPARE(window->entryModel()->index(0, VoiceBankEntryModel::AliasColumn).data().toString(),
                 QStringLiteral("renamed"));

        QVERIFY(window->save());
        QVERIFY(!window->isWindowModified());
        std::ifstream in(bank / "oto.ini", std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
        QVERIFY(text.find("a.wav=renamed,10,20,-30,40,5") != std::string::npos);

        rename(QStringLiteral("again"));
        bool asked = false;
        QTimer::singleShot(0, [&asked] {
            if (const auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                asked = true;
                box->button(QMessageBox::Discard)->click();
            }
        });
        QVERIFY(window->close());
        QVERIFY(asked);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(e->voiceBankWindows().isEmpty());
    }

    // The project window opens the voice bank of the project in its window.
    void a_project_window_edits_its_voice_bank() {
        QTemporaryDir dir;
        const auto utau = pathIn(dir, "utau");
        fs::create_directories(utau / "voice");
        const auto bank = voiceBank(dir, "utau/voice/bank");
        const auto e = editor();
        e->settings().setUtauDirectory(utau);
        const auto window = e->newWindow();
        {
            const auto session = window->document()->session();
            auto transaction = session->transaction(QStringLiteral("voice"));
            kit::ProjectRef(session).tracks().at(0).setVoiceDir(QStringLiteral("%VOICE%bank"));
            QVERIFY(transaction.commit());
        }
        actionNamed(window, QStringLiteral("Edit &Voice Bank"))->trigger();
        e->settings().setUtauDirectory({});
        QCOMPARE(e->voiceBankWindows().size(), 1);
        QVERIFY(isSame(e->voiceBankWindows().first()->document()->rootPath(), bank));
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_Editor test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_Editor.moc"
