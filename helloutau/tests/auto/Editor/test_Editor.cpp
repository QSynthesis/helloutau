#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <thread>

#include <QtCore/QMimeData>
#include <QtCore/QStandardPaths>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtCore/QtEndian>
#include <QtGui/QAction>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QImage>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtGui/QClipboard>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QGraphicsDropShadowEffect>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeWidget>
#include <QtCore/QFile>

#include <hellokit/Edit/ProjectDocument.h>
#include <hellokit/Edit/ProjectEdits.h>
#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Edit/VoiceBankDocument.h>
#include <hellokit/Edit/VoiceBankRefs.h>
#include <hellokit/VoiceBank/BuiltinFrequencyFormats.h>
#include <hellokit/VoiceBank/FrequencyFormats.h>

#include <helloutau/Audio/AudioEngine.h>
#include <helloutau/Editor/Dialogs/RegionDialog.h>
#include <helloutau/Widgets/CommandPalette.h>
#include <helloutau/Widgets/FindBar.h>
#include <helloutau/Widgets/SettingPage.h>
#include <helloutau/Widgets/SettingPageRegistry.h>
#include <helloutau/Widgets/SettingsDialog.h>

#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/BuiltinActions.h>
#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/Dialogs/NotePropertiesDialog.h>
#include <helloutau/Editor/OtoWaveformView.h>
#include <helloutau/Editor/Dialogs/ProjectPropertiesDialog.h>
#include <helloutau/Editor/ProjectWindow.h>
#include <helloutau/Editor/Restarter.h>
#include <helloutau/Editor/PianoRoll.h>
#include <helloutau/Editor/Dialogs/PasteParametersDialog.h>
#include <helloutau/Editor/Dialogs/ScalePitchDialog.h>
#include <helloutau/Editor/Dialogs/PitchControlDialog.h>
#include <helloutau/Editor/Dialogs/VoiceBankCharsetDialog.h>
#include <helloutau/Editor/VoiceBankEntryModel.h>
#include <helloutau/Editor/VoiceBankInfoPanel.h>
#include <helloutau/Editor/VoiceBankWindow.h>

#include <helloutau/Testing/Editor/TestingEditor.h>

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

    // A project of quarter notes with lyrics
    fs::path savedLyrics(const QTemporaryDir &dir, const char *name, const QStringList &lyrics) {
        kit::Track track;
        for (const auto &lyric : lyrics) {
            kit::Note note;
            note.lyric = lyric;
            note.length = 480;
            note.noteNum = 60;
            track.notes.push_back(note);
        }
        kit::Project project;
        project.tracks.push_back(track);

        const auto path = pathIn(dir, name);
        kit::DiagnosticList diagnostics;
        project.save(path, diagnostics);
        return path;
    }

    QStringList lyricsOf(ProjectWindow *window) {
        QStringList lyrics;
        const auto project = window->document()->session()->snapshot();
        for (const auto &note : project.tracks[0].notes) {
            lyrics.push_back(note.lyric);
        }
        return lyrics;
    }

    void edit(ProjectWindow *window) {
        const auto session = window->document()->session();
        auto tx = session->transaction(QStringLiteral("rename"));
        kit::ProjectRef(session).tracks().at(0).setVoiceDir(QStringLiteral("changed"));
        tx.commit();
    }

    // Answers the next message box with \a button once it appears.
    void answerMessageBox(QMessageBox::StandardButton button) {
        QTimer::singleShot(0, [button] {
            const auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(button)->click();
        });
    }

    // Calls trigger, and closes every modal dialog that is shown meanwhile: a message box with No
    // if it has that button, or else with its escape button, and another dialog by rejecting it.
    // Returns, in order, Question for a message box with No, Message for another message box, and
    // the class name of any other dialog.
    QStringList dialogsDuring(const std::function<void()> &trigger) {
        QStringList dialogs;
        QTimer timer;
        timer.setInterval(10);
        QObject::connect(&timer, &QTimer::timeout, [&dialogs] {
            const auto widget = QApplication::activeModalWidget();
            if (const auto box = qobject_cast<QMessageBox *>(widget)) {
                if (const auto no = box->button(QMessageBox::No)) {
                    dialogs.push_back(QStringLiteral("Question"));
                    no->click();
                } else {
                    dialogs.push_back(QStringLiteral("Message"));
                    box->button(QMessageBox::Ok) ? box->button(QMessageBox::Ok)->click()
                                                 : box->reject();
                }
            } else if (const auto dialog = qobject_cast<QDialog *>(widget)) {
                dialogs.push_back(QString::fromLatin1(dialog->metaObject()->className()));
                dialog->reject();
            }
        });
        timer.start();
        trigger();
        return dialogs;
    }

    // A project of four notes in the region A, whose middle two are in the region B
    fs::path savedRegions(const QTemporaryDir &dir, const char *name) {
        kit::Track track;
        for (int i = 0; i < 4; ++i) {
            kit::Note note;
            note.lyric = QStringLiteral("la");
            note.length = 480;
            note.noteNum = 60;
            track.notes.push_back(note);
        }
        track.notes[0].regions = {QStringLiteral("A")};
        track.notes[1].regions = {QStringLiteral("B")};
        track.notes[2].regionEnds = {QStringLiteral("B")};
        track.notes[3].regionEnds = {QStringLiteral("A")};
        kit::Project project;
        project.tracks.push_back(track);
        const auto path = pathIn(dir, name);
        kit::DiagnosticList diagnostics;
        project.save(path, diagnostics);
        return path;
    }

    QStringList regionNamesOf(const PianoRoll *roll) {
        QStringList names;
        for (const auto &region : roll->regions()) {
            names.push_back(region.name);
        }
        return names;
    }

}

class test_Editor : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    // The formats of frequency tables that the plugin FrequencyEditor registers, one object for
    // each editor that editor() created. An object that outlives its editor removes nothing.
    mutable std::vector<std::unique_ptr<kit::BuiltinFrequencyFormats>> m_formats;

    // An editor with the menus, the commands and the frequency table formats that the core
    // plugin and the plugin FrequencyEditor register, whose voice bank windows do not follow the
    // disk on their own, which would ask at any moment; a test calls VoiceBankWindow::checkDisk()
    // instead.
    std::unique_ptr<Editor> editor() const {
        auto e = std::make_unique<Editor>(
            std::make_unique<AppSettings>(m_dir.filePath(QStringLiteral("settings.json"))));
        e->setWatchesDisk(false);
        new BuiltinActions(e.get());
        m_formats.push_back(
            std::make_unique<kit::BuiltinFrequencyFormats>(e->frequencyFormats().registry()));
        return e;
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
                                     QStringLiteral("&Select"), QStringLiteral("&View"),
                                     QStringLiteral("&Playback"), QStringLiteral("&Tools"),
                                     QStringLiteral("&Help")}));
        const auto save = declaredActionOf(*e, window, QStringLiteral("helloutau.file.save"));
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

    // Every action that the manifest of a kind of window declares has an action or a widget of
    // the window. An id renamed in the manifest or in the window alone leaves the declared
    // action without a handler.
    void every_declared_action_has_a_handler() {
        QTemporaryDir dir;
        const auto e = editor();
        const auto unhandled = unhandledActionsOf(*e, e->newWindow());
        QVERIFY2(unhandled.isEmpty(), qPrintable(unhandled.join(QStringLiteral(", "))));
        const auto bankWindow = e->openVoiceBank(voiceBank(dir));
        QVERIFY(bankWindow);
        const auto bankUnhandled = unhandledActionsOf(*e, bankWindow);
        QVERIFY2(bankUnhandled.isEmpty(), qPrintable(bankUnhandled.join(QStringLiteral(", "))));
    }

    void untitled_window_numbers_are_reserved_until_close() {
        const auto e = editor();
        const auto first = e->newWindow();
        const auto second = e->newWindow();
        QCOMPARE(first->windowTitle(), QStringLiteral("Untitled[*] - HelloUtau"));
        QCOMPARE(second->windowTitle(), QStringLiteral("Untitled-2[*] - HelloUtau"));

        first->close();
        QCoreApplication::processEvents();
        QCOMPARE(second->windowTitle(), QStringLiteral("Untitled-2[*] - HelloUtau"));

        const auto third = e->newWindow();
        const auto fourth = e->newWindow();
        QCOMPARE(third->windowTitle(), QStringLiteral("Untitled[*] - HelloUtau"));
        QCOMPARE(fourth->windowTitle(), QStringLiteral("Untitled-3[*] - HelloUtau"));
    }

    void undo_and_the_modified_mark_follow_the_project() {
        const auto e = editor();
        const auto window = e->newWindow();
        const auto undo = declaredActionOf(*e, window, QStringLiteral("helloutau.edit.undo"));
        QVERIFY(undo);
        QVERIFY(!undo->isEnabled());

        edit(window);
        QVERIFY(undo->isEnabled());
        QVERIFY(window->isWindowModified());

        undo->trigger();
        QVERIFY(!undo->isEnabled());
        QVERIFY(!window->isWindowModified());
    }

    // Opening replaces the current project; a second window is opened explicitly with New.
    void a_file_opens_in_the_current_window() {
        const auto e = editor();
        const auto first = e->newWindow();
        const auto a = savedProject(m_dir, "a.usth");
        const auto b = savedProject(m_dir, "b.usth");
        const auto c = savedProject(m_dir, "c.usth");

        QCOMPARE(e->openFile(a, first), first);
        QCOMPARE(first->windowTitle(), QStringLiteral("a.usth[*] - HelloUtau"));
        QVERIFY(!first->isUnused());

        const auto second = e->openFile(b, first);
        QCOMPARE(second, first);
        QCOMPARE(e->windows().size(), 1);

        const auto newWindow = e->newWindow();
        QCOMPARE(e->openFile(c, newWindow), newWindow);
        QCOMPARE(e->windows().size(), 2);

        // A file that is already open is not opened twice.
        QCOMPARE(e->openFile(b, newWindow), first);
        QCOMPARE(e->windows().size(), 2);
    }

    // As in VS Code, the palette lists the commands that are enabled now, each after its
    // category, including the command that opens the palette.
    void the_command_palette_offers_the_enabled_commands() {
        const auto e = editor();
        const auto window = e->newWindow();
        const auto open =
            declaredActionOf(*e, window, QStringLiteral("helloutau.view.commandPalette"));
        QVERIFY(open);
        QCOMPARE(open->shortcuts(),
                 (QList<QKeySequence>{QKeySequence(QStringLiteral("Ctrl+Shift+P")),
                                      QKeySequence(Qt::Key_F1)}));

        open->trigger();
        const auto palette = window->findChild<CommandPalette *>();
        QVERIFY(palette && palette->isVisible());
        const auto ids = palette->shownIds();
        QVERIFY(ids.contains(QStringLiteral("helloutau.file.save")));
        QVERIFY(isDeclared(*e, Editor::ProjectWindowKind, QStringLiteral("helloutau.edit.undo")));
        QVERIFY(!ids.contains(QStringLiteral("helloutau.edit.undo")));
        QVERIFY(ids.contains(QStringLiteral("helloutau.view.commandPalette")));
        bool exportFound = false;
        for (const auto &entry : palette->commands()) {
            if (entry.id == QStringLiteral("helloutau.file.save")) {
                QCOMPARE(entry.label, QStringLiteral("File: Save"));
                QCOMPARE(entry.shortcut, QKeySequence(QStringLiteral("Ctrl+S")));
            }
            // The palette shows the full text, and the Export menu shows UST...
            if (entry.id == QStringLiteral("helloutau.file.exportUst")) {
                QCOMPARE(entry.label, QStringLiteral("File: Export UST..."));
                exportFound = true;
            }
        }
        QVERIFY(exportFound);
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

    void opening_over_a_modified_project_asks_to_save() {
        const auto e = editor();
        const auto window = e->newWindow();
        const auto first = savedProject(m_dir, "open1.usth");
        const auto second = savedProject(m_dir, "open2.usth");
        QCOMPARE(e->openFile(first, window), window);
        edit(window);

        answerMessageBox(QMessageBox::Cancel);
        QCOMPARE(e->openFile(second, window), nullptr);
        QCOMPARE(window->document()->sourcePath(), first);
        QVERIFY(window->isWindowModified());

        answerMessageBox(QMessageBox::Discard);
        QCOMPARE(e->openFile(second, window), window);
        QCOMPARE(window->document()->sourcePath(), second);
    }

    // A file dropped on a window opens as by Open and asks whether modified changes may be
    // discarded or saved.
    void a_dropped_file_opens_as_by_open() {
        const auto e = editor();
        const auto first = savedProject(m_dir, "drop1.usth");
        const auto second = savedProject(m_dir, "drop2.usth");
        const auto window = e->newWindow();
        // Drops path on target and returns whether the user was asked to save, answering with
        // answer if so.
        const auto drop = [](ProjectWindow *target, const fs::path &path,
                             QMessageBox::StandardButton answer = QMessageBox::Cancel) {
            QMimeData data;
            data.setUrls({QUrl::fromLocalFile(QString::fromStdU16String(path.u16string()))});
            QDragEnterEvent enter(QPoint(10, 10), Qt::CopyAction, &data, Qt::LeftButton,
                                  Qt::NoModifier);
            QCoreApplication::sendEvent(target, &enter);
            if (!enter.isAccepted()) {
                return false;
            }
            QDropEvent event(QPointF(10, 10), Qt::CopyAction, &data, Qt::LeftButton,
                             Qt::NoModifier);
            QCoreApplication::sendEvent(target, &event);
            // The file opens once the drop has returned, which may ask whether to save.
            bool asked = false;
            QTimer::singleShot(0, [&asked, answer] {
                if (const auto box =
                        qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                    asked = true;
                    box->button(answer)->click();
                }
            });
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            return asked;
        };
        const auto shows = [](ProjectWindow *target, const fs::path &path) {
            std::error_code error;
            return fs::equivalent(target->document()->sourcePath(), path, error);
        };

        QVERIFY(window->acceptDrops());
        QVERIFY(!drop(window, first));
        QCOMPARE(e->windows().size(), 1);
        QVERIFY(shows(window, first));

        edit(window);
        QVERIFY(drop(window, second, QMessageBox::Cancel));
        QCOMPARE(e->windows().size(), 1);
        QVERIFY(shows(window, first));
        QVERIFY(window->isWindowModified());
        QVERIFY(drop(window, second, QMessageBox::Discard));
        QCOMPARE(e->windows().size(), 1);
        QVERIFY(shows(window, second));

        QVERIFY(!drop(window, second));
        QCOMPARE(e->windows().size(), 1);
        QVERIFY(shows(window, second));
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
    // The pitch-control command opens the dialog with that of the first selected note, or the
    // default, and gives what it accepts to every selected note.
    void the_vibrato_of_the_selected_notes_is_edited() {
        const auto e = editor();
        const auto window = e->openFile(savedProject(m_dir, "v.usth"));
        QVERIFY(window);
        auto roll = qobject_cast<PianoRoll *>(window->centralWidget());
        const auto edit =
            declaredActionOf(*e, window, QStringLiteral("helloutau.edit.pitchControl"));
        QVERIFY(edit);
        QVERIFY(!edit->isEnabled());

        roll->selectAll();
        QVERIFY(edit->isEnabled());
        double shown = 0;
        QTimer::singleShot(0, [&shown] {
            const auto dialog =
                qobject_cast<PitchControlDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            shown = dialog->field(1)->value();
            // The notes have no vibrato, and the fields are enabled only while the group is
            // checked.
            QVERIFY(!dialog->field(1)->isEnabled());
            for (const auto box : dialog->findChildren<QCheckBox *>()) {
                if (box->text() == QStringLiteral("&Vibrato")) {
                    box->setChecked(true);
                }
            }
            QVERIFY(dialog->field(1)->isEnabled());
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

    // The find bar selects the notes whose lyric matches, from the selected note on and from the
    // start again past the last match. Replace changes the selected match and selects the next
    // match. Replace All changes every match. Each replacement is one undo step.
    void lyrics_are_found_and_replaced() {
        const auto e = editor();
        const auto window =
            e->openFile(savedLyrics(m_dir, "find.usth", {"ka", "a", "sa", "ka", "R"}));
        QVERIFY(window);
        const auto roll = window->pianoRoll();
        const auto bar = window->findChild<FindBar *>();
        QVERIFY(bar);
        const auto find = declaredActionOf(*e, window, QStringLiteral("helloutau.edit.find"));
        const auto next = declaredActionOf(*e, window, QStringLiteral("helloutau.edit.findNext"));
        const auto previous =
            declaredActionOf(*e, window, QStringLiteral("helloutau.edit.findPrevious"));
        QVERIFY(find && next && previous);

        find->trigger();
        QVERIFY(bar->isVisible());
        QVERIFY(!bar->isReplaceShown());
        bar->setText(QStringLiteral("ka"));
        QCOMPARE(roll->selectedIndices(), QList<int>{0});
        QCOMPARE(bar->resultText(), QStringLiteral("1 of 2"));
        QCOMPARE(roll->lyricSearch().pattern(), QStringLiteral("ka"));
        next->trigger();
        QCOMPARE(roll->selectedIndices(), QList<int>{3});
        next->trigger();
        QCOMPARE(roll->selectedIndices(), QList<int>{0});
        previous->trigger();
        QCOMPARE(roll->selectedIndices(), QList<int>{3});

        // A whole word matches only the lyric a. Rests are searched as well.
        bar->setWholeWord(true);
        bar->setText(QStringLiteral("a"));
        QCOMPARE(roll->selectedIndices(), QList<int>{1});
        QCOMPARE(bar->resultText(), QStringLiteral("1 of 1"));
        // A change of the query keeps the selected note if it still matches.
        bar->setWholeWord(false);
        QCOMPARE(roll->selectedIndices(), QList<int>{1});
        bar->setText(QStringLiteral("r"));
        QCOMPARE(roll->selectedIndices(), QList<int>{4});

        bar->setRegularExpression(true);
        bar->setText(QStringLiteral("("));
        QCOMPARE(bar->resultText(), QStringLiteral("Invalid"));

        // The search starts from the selected note: sa is the next match of (k|s)a after a.
        roll->setSelectedIndices({1});
        bar->setText(QStringLiteral("(k|s)a"));
        QCOMPARE(roll->selectedIndices(), QList<int>{2});
        const auto session = window->document()->session();
        const int step = session->currentStep();
        declaredActionOf(*e, window, QStringLiteral("helloutau.edit.replace"))->trigger();
        QVERIFY(bar->isReplaceShown());
        bar->setReplacement(QStringLiteral("$1o"));
        QTest::keyClick(bar->replaceField(), Qt::Key_Return);
        QCOMPARE(lyricsOf(window), (QStringList{"ka", "a", "so", "ka", "R"}));
        QCOMPARE(session->currentStep(), step + 1);
        QCOMPARE(roll->selectedIndices(), QList<int>{3});

        QTest::keyClick(bar->replaceField(), Qt::Key_Return,
                        Qt::ControlModifier | Qt::AltModifier);
        QCOMPARE(lyricsOf(window), (QStringList{"ko", "a", "so", "ko", "R"}));
        QCOMPARE(session->currentStep(), step + 2);
        QCOMPARE(roll->selectedIndices(), (QList<int>{0, 3}));
        QCOMPARE(bar->resultText(), QStringLiteral("No results"));

        session->undo();
        QCOMPARE(lyricsOf(window), (QStringList{"ka", "a", "so", "ka", "R"}));

        // The roll highlights the matches until the find bar is closed.
        QVERIFY(roll->lyricSearch().isValid());
        QTest::keyClick(bar->findField(), Qt::Key_Escape);
        QVERIFY(!roll->lyricSearch().isValid());
    }

    // Copy enables Paste Parameters, whose dialog chooses what to paste onto the selection.
    void parameters_are_pasted_through_the_menu() {
        const auto e = editor();
        const auto window = e->openFile(savedProject(m_dir, "p.usth"));
        QVERIFY(window);
        auto roll = qobject_cast<PianoRoll *>(window->centralWidget());
        QGuiApplication::clipboard()->clear();
        const auto paste =
            declaredActionOf(*e, window, QStringLiteral("helloutau.edit.pasteParameters"));
        const auto pasteNotes =
            declaredActionOf(*e, window, QStringLiteral("helloutau.edit.paste"));
        QVERIFY(paste && pasteNotes);

        roll->selectAll();
        QVERIFY(!paste->isEnabled());
        QVERIFY(!pasteNotes->isEnabled());
        kit::DiagnosticList diagnostics;
        roll->toggleVibrato(diagnostics);
        declaredActionOf(*e, window, QStringLiteral("helloutau.edit.copy"))->trigger();
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
        const auto scale =
            declaredActionOf(*e, window, QStringLiteral("helloutau.edit.scalePitch"));
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
    // "Open Recent" as in VS Code: the latest ten projects, then the latest ten voice banks,
    // the first ten numbered. A section with ten items ends with a "More" item, which lists
    // every item of its kind in a palette. "Clear Recent" forgets both kinds.
    void open_recent_lists_projects_then_voice_banks() {
        const auto e = editor();
        e->settings().clearRecentFiles();
        e->settings().clearRecentVoiceBanks();
        for (int i = 0; i < 12; ++i) {
            e->settings().addRecentFile(
                pathIn(m_dir, qPrintable(QStringLiteral("f%1.usth").arg(i))));
        }
        e->settings().addRecentVoiceBank(pathIn(m_dir, "bank0"));
        e->settings().addRecentVoiceBank(pathIn(m_dir, "bank1"));
        const auto window = e->newWindow();
        const auto recent =
            declaredActionOf(*e, window, QStringLiteral("helloutau.file.openRecent"));
        QVERIFY(recent && recent->menu());
        Q_EMIT recent->menu()->aboutToShow();
        auto items = recent->menu()->actions();
        QCOMPARE(items.size(), 10 + 1 + 1 + 2 + 1 + 1);
        QVERIFY(items[0]->text().startsWith(QStringLiteral("&1 ")));
        QVERIFY(items[0]->text().endsWith(QStringLiteral("f11.usth")));
        QVERIFY(items[9]->text().startsWith(QStringLiteral("&0 ")));
        QCOMPARE(items[10]->text(), QStringLiteral("More &Projects..."));
        QVERIFY(items[11]->isSeparator());
        QVERIFY(items[12]->text().endsWith(QStringLiteral("bank1")));
        QVERIFY(!items[12]->text().startsWith(QLatin1Char('&')));
        QVERIFY(items[14]->isSeparator());
        QCOMPARE(items[15]->text(), QStringLiteral("&Clear Recently Opened..."));

        items[10]->trigger();
        const auto palette = window->findChild<CommandPalette *>(QStringLiteral("recentPalette"));
        QVERIFY(palette && palette->isVisible());
        auto commands = palette->commands();
        QCOMPARE(commands.size(), 12);
        QCOMPARE(commands[0].id,
                 QStringLiteral("project:") +
                     QDir::toNativeSeparators(
                         QString::fromStdU16String(pathIn(m_dir, "f11.usth").u16string())));
        QCOMPARE(commands[0].label, QStringLiteral("f11.usth"));
        QCOMPARE(commands[0].description, QDir::toNativeSeparators(m_dir.path()));
        // The latest first, also while the user types
        QCOMPARE(palette->shownIds().first(), commands[0].id);
        palette->setQuery(QStringLiteral("f1"));
        QCOMPARE(palette->shownIds().first(), commands[0].id);
        palette->hide();

        // The command in no menu lists the voice banks.
        const auto command =
            declaredActionOf(*e, window, QStringLiteral("helloutau.file.openRecentVoiceBank"));
        QVERIFY(command);
        command->trigger();
        commands = palette->commands();
        QCOMPARE(commands.size(), 2);
        QCOMPARE(commands[0].label, QStringLiteral("bank1"));
        QVERIFY(commands[1].id.startsWith(QStringLiteral("voicebank:")));

        // The button of an item forgets it, and the palette stays open.
        const auto list = palette->findChild<QListWidget *>();
        QVERIFY(list);
        const auto button = CommandPalette::removeButtonRect(list->visualItemRect(list->item(1)),
                                                             list->fontMetrics());
        QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, button.center());
        QCOMPARE(palette->commands().size(), 1);
        QCOMPARE(e->settings().recentVoiceBanks(), QList<fs::path>{pathIn(m_dir, "bank1")});
        QVERIFY(palette->isVisible());
        palette->hide();
        e->settings().addRecentVoiceBank(pathIn(m_dir, "bank0"));

        // Ten voice banks end their section with a "More" item as well.
        for (int i = 2; i < 10; ++i) {
            e->settings().addRecentVoiceBank(
                pathIn(m_dir, qPrintable(QStringLiteral("bank%1").arg(i))));
        }
        Q_EMIT recent->menu()->aboutToShow();
        items = recent->menu()->actions();
        QCOMPARE(items[22]->text(), QStringLiteral("More &Voice Banks..."));

        // Nine projects end their section without one.
        for (int i = 0; i < 3; ++i) {
            e->settings().removeRecentFile(
                pathIn(m_dir, qPrintable(QStringLiteral("f%1.usth").arg(i))));
        }
        Q_EMIT recent->menu()->aboutToShow();
        items = recent->menu()->actions();
        QVERIFY(items[9]->isSeparator());
        QVERIFY(std::none_of(items.begin(), items.end(), [](const QAction *item) {
            return item->text() == QStringLiteral("More &Projects...");
        }));

        // Clearing asks first, and Cancel keeps both kinds.
        const auto answer = [](bool clear) {
            QTimer::singleShot(0, [clear] {
                const auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                QVERIFY(box);
                for (const auto button : box->buttons()) {
                    if ((box->buttonRole(button) == QMessageBox::DestructiveRole) == clear) {
                        button->click();
                        return;
                    }
                }
            });
        };
        answer(false);
        items.last()->trigger();
        QVERIFY(!e->settings().recentFiles().isEmpty());
        answer(true);
        items.last()->trigger();
        QVERIFY(e->settings().recentFiles().isEmpty());
        QVERIFY(e->settings().recentVoiceBanks().isEmpty());
        Q_EMIT recent->menu()->aboutToShow();
        items = recent->menu()->actions();
        QCOMPARE(items.size(), 1);
        QVERIFY(!items[0]->isEnabled());
    }

    void recent_files_open_from_their_menu() {
        const auto e = editor();
        e->settings().clearRecentFiles();
        e->settings().clearRecentVoiceBanks();
        const auto first = savedProject(m_dir, "r1.usth");
        const auto second = savedProject(m_dir, "r2.usth");
        e->openFile(first);
        const auto window = e->openFile(second);
        QVERIFY(window);
        QCOMPARE(e->settings().recentFiles(), (QList<std::filesystem::path>{second, first}));

        // The palette of a window lists the project of the window as well.
        e->showRecent(Editor::RecentProjects, window);
        const auto palette = window->findChild<CommandPalette *>(QStringLiteral("recentPalette"));
        QVERIFY(palette);
        QCOMPARE(palette->commands().size(), 2);
        QCOMPARE(palette->commands()[0].label, QStringLiteral("r2.usth"));
        palette->hide();

        // The menu of the external action, filled when it opens
        const auto recent =
            declaredActionOf(*e, window, QStringLiteral("helloutau.file.openRecent"));
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

    // In the prerender mode, as in UTAU, playing renders the selected notes, and without them
    // nothing.
    void prerendering_needs_notes_selected() {
        const auto e = editor();
        QCOMPARE(e->settings().playbackMode(), AppSettings::Prerender);
        const auto window = e->newWindow();
        declaredActionOf(*e, window, QStringLiteral("helloutau.playback.play"))->trigger();
        QCOMPARE(window->statusBar()->currentMessage(),
                 QStringLiteral("Select the notes to render first."));

        // Only playback shows the playhead, which the realtime mode shows at rest as well.
        const auto roll = qobject_cast<PianoRoll *>(window->centralWidget());
        QVERIFY(roll);
        QVERIFY(!roll->isCursorEnabled());
        e->settings().setPlaybackMode(AppSettings::Realtime);
        window->applySettings();
        QVERIFY(roll->isCursorEnabled());
        e->settings().setPlaybackMode(AppSettings::Prerender);
        window->applySettings();
        QVERIFY(!roll->isCursorEnabled());
    }

    // The tool bar of the project window holds the box of the quantization, which follows the
    // piano roll both ways, and the two commands step through the quantizations. The tool bar
    // is shown as the settings store it, and the commands carry icons.
    void the_tool_bar_holds_the_quantization() {
        const auto e = editor();
        const auto window = e->newWindow();
        window->show();
        const auto roll = window->pianoRoll();
        const auto toolBar = window->findChild<QToolBar *>(QStringLiteral("mainToolBar"));
        QVERIFY(toolBar && toolBar->isVisible());
        const auto box = window->quantizationBox();
        QVERIFY(box);
        QCOMPARE(box->currentData().toInt(), roll->quantization());

        box->setCurrentIndex(box->findData(60));
        QCOMPARE(roll->quantization(), 60);
        QCOMPARE(e->settings().quantization(), 60);
        roll->setQuantization(240);
        QCOMPARE(box->currentData().toInt(), 240);
        QCOMPARE(e->settings().quantization(), 240);

        // Finer steps down to off and coarser steps up to a quarter note, and neither goes further.
        const auto finer = declaredActionOf(
            *e, window, QStringLiteral("helloutau.select.decreaseQuantizationInterval"));
        const auto coarser = declaredActionOf(
            *e, window, QStringLiteral("helloutau.select.increaseQuantizationInterval"));
        QVERIFY(finer && coarser);
        QCOMPARE(finer->shortcut(), QKeySequence(QStringLiteral("Ctrl+[")));
        for (int i = 0; i < 10; ++i) {
            finer->trigger();
        }
        QCOMPARE(roll->quantization(), 0);
        QCOMPARE(box->currentData().toInt(), 0);
        for (int i = 0; i < 10; ++i) {
            coarser->trigger();
        }
        QCOMPARE(roll->quantization(), PianoRoll::quantizations().first());

        const auto show =
            declaredActionOf(*e, window, QStringLiteral("helloutau.view.showToolBar"));
        QVERIFY(show && show->isChecked());
        show->trigger();
        QVERIFY(toolBar->isHidden());
        QVERIFY(!e->settings().isToolBarVisible());
        show->trigger();
        QVERIFY(e->settings().isToolBarVisible());

        QVERIFY(
            !declaredActionOf(*e, window, QStringLiteral("helloutau.edit.undo"))->icon().isNull());

        // The icon has visible pixels in the checked, disabled and hover states.
        const auto icon =
            declaredActionOf(*e, window, QStringLiteral("helloutau.view.showPitch"))->icon();
        for (const auto &[mode, state] : {
                 std::pair{QIcon::Normal,   QIcon::On },
                 std::pair{QIcon::Disabled, QIcon::Off},
                 std::pair{QIcon::Active,   QIcon::Off}
        }) {
            const auto image = icon.pixmap(QSize(16, 16), 1.0, mode, state).toImage();
            bool drawn = false;
            for (int y = 0; y < image.height() && !drawn; ++y) {
                for (int x = 0; x < image.width() && !drawn; ++x) {
                    drawn = qAlpha(image.pixel(x, y)) > 0;
                }
            }
            QVERIFY2(drawn, qPrintable(QStringLiteral("mode %1, state %2").arg(mode).arg(state)));
        }

        // A checked button has a subtle background and white text.
        const auto buttons = toolBar->findChildren<QToolButton *>();
        QVERIFY(!buttons.isEmpty());
        const auto colors = buttons.first()->palette();
        QCOMPARE(colors.color(QPalette::Active, QPalette::Accent),
                 QGuiApplication::palette().color(QPalette::Active, QPalette::Accent));
        // The Windows 11 style uses the button text color for a checked button only if it is set.
        QVERIFY(colors.isBrushSet(QPalette::Active, QPalette::ButtonText));
        QCOMPARE(colors.color(QPalette::Active, QPalette::ButtonText), QColor(Qt::white));

        // The box is no command of the palette, since the context has no action for a box.
        declaredActionOf(*e, window, QStringLiteral("helloutau.view.commandPalette"))->trigger();
        const auto palette = window->findChild<CommandPalette *>();
        QVERIFY(palette);
        const auto boxId = QStringLiteral("helloutau.select.quantizationWidget");
        QVERIFY(isDeclared(*e, Editor::ProjectWindowKind, boxId));
        QVERIFY(!palette->shownIds().contains(boxId));
        palette->hide();
    }

    // A registered page is placed under its parent and before its sibling, at the top level if
    // the parent does not exist, and removed when its registration is destroyed. A page whose id
    // differs from the name of its entry is rejected. The registration may outlive the editor.
    void registered_setting_pages_are_placed_and_removed() {
        const auto pageOf = [](const char *id, const char *parent, const char *before) {
            return [id, parent, before] {
                return SettingPagePlacement{std::make_unique<SettingPage>(QLatin1String(id)),
                                            QLatin1String(parent), QLatin1String(before)};
            };
        };
        auto e = editor();
        const auto catalog = e->settingCatalog();
        SettingPageRegistry::AddFactory top(e->settingPages(), "test.top", {},
                                            pageOf("test.top", "", "editor.Rendering"));
        SettingPageRegistry::AddFactory child(
            e->settingPages(), "test.child", {},
            pageOf("test.child", "editor.AppearanceAndBehavior", "editor.SystemSettings"));
        SettingPageRegistry::AddFactory orphan(e->settingPages(), "test.orphan", {},
                                               pageOf("test.orphan", "test.none", ""));
        SettingPageRegistry::AddFactory renamed(e->settingPages(), "test.renamed", {},
                                                pageOf("test.other", "", ""));
        QVERIFY(top.entry() && child.entry() && orphan.entry() && renamed.entry());

        QStringList topLevel;
        for (const auto page : catalog->pages()) {
            topLevel.push_back(page->id());
        }
        QCOMPARE(topLevel,
                 (QStringList{"editor.AppearanceAndBehavior", "editor.Editor", "editor.Utau",
                              "editor.Audio", "test.top", "editor.Rendering", "test.orphan"}));
        const auto parent = catalog->page(QStringLiteral("editor.AppearanceAndBehavior"));
        QCOMPARE(parent->pages().first()->id(), QStringLiteral("test.child"));
        QVERIFY(!catalog->page(QStringLiteral("test.other")));

        child = {};
        QVERIFY(!catalog->page(QStringLiteral("test.child")));
        QVERIFY(catalog->page(QStringLiteral("test.top")));
        e.reset();
        top = {};
    }

    // The settings are pages of the catalog of the editor, in the order of the settings of
    // JetBrains IDEs, and what their dialog applies reaches every project window at once.
    void the_settings_apply_to_every_window() {
        const auto e = editor();
        QStringList topLevel;
        for (const auto page : e->settingCatalog()->pages()) {
            topLevel.push_back(page->id());
        }
        // Keymap and Menus and Toolbars are pages of the core plugin, see test_CoreSettingPages.
        QCOMPARE(topLevel, (QStringList{"editor.AppearanceAndBehavior", "editor.Editor",
                                        "editor.Utau", "editor.Audio", "editor.Rendering"}));
        const auto system = e->settingCatalog()->page(QStringLiteral("editor.SystemSettings"));
        QVERIFY(system);
        QCOMPARE(system->parentPage()->id(), QStringLiteral("editor.AppearanceAndBehavior"));
        const auto window = e->newWindow();
        const auto roll = qobject_cast<PianoRoll *>(window->centralWidget());
        QVERIFY(roll && !roll->isCursorEnabled());

        QTimer::singleShot(0, [] {
            const auto dialog = qobject_cast<SettingsDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->currentPage()->id(), QStringLiteral("editor.Rendering"));
            const auto mode = dialog->currentPage()->widget()->findChild<QComboBox *>();
            QVERIFY(mode);
            mode->setCurrentIndex(mode->findData(AppSettings::Realtime));
            QVERIFY(dialog->applyButton()->isEnabled());
            dialog->accept();
        });
        e->showSettings(window, QStringLiteral("editor.Rendering"));
        QCOMPARE(e->settings().playbackMode(), AppSettings::Realtime);
        QVERIFY(roll->isCursorEnabled());
        e->settings().setPlaybackMode(AppSettings::Prerender);
    }

    // The language is chosen on System Settings from the system default, English and Simplified
    // Chinese. A changed language is applied for the next start, and once the dialog closes a
    // restart is offered; declined, nothing restarts.
    void the_language_is_chosen_for_the_next_start() {
        const auto e = editor();
        e->settings().setLanguage(QString());
        const auto page = e->settingCatalog()->page(QStringLiteral("editor.SystemSettings"));
        QVERIFY(page);
        const auto box = page->widget()->findChild<QComboBox *>(QStringLiteral("language"));
        QVERIFY(box);
        QStringList languages;
        for (int i = 0; i < box->count(); ++i) {
            languages.push_back(box->itemData(i).toString());
        }
        QCOMPARE(languages,
                 (QStringList{QString(), QStringLiteral("en"), QStringLiteral("zh_CN")}));
        QCOMPARE(box->currentIndex(), 0);
        QVERIFY(!page->isModified());

        const auto window = e->newWindow();
        bool asked = false;
        QTimer::singleShot(0, [&asked] {
            const auto dialog = qobject_cast<SettingsDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            const auto language =
                dialog->currentPage()->widget()->findChild<QComboBox *>(QStringLiteral("language"));
            QVERIFY(language);
            language->setCurrentIndex(language->findData(QStringLiteral("zh_CN")));
            QVERIFY(dialog->applyButton()->isEnabled());
            // The question comes once the dialog has closed.
            QTimer::singleShot(0, [&asked] {
                const auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                QVERIFY(box);
                asked = true;
                box->button(QMessageBox::No)->click();
            });
            dialog->accept();
        });
        e->showSettings(window, QStringLiteral("editor.SystemSettings"));
        QVERIFY(asked);
        QCOMPARE(e->settings().language(), QStringLiteral("zh_CN"));
        QVERIFY(!Restarter::isNeeded());
        QVERIFY(!Restarter::isRestarting());
        QVERIFY(window->isVisible());
        e->settings().setLanguage(QString());
    }

    // The number of rendering threads is chosen from Automatic, the powers of two below the
    // number of hardware threads and that number, or typed in without an upper limit. A text
    // that is not a positive count is not applied.
    void the_thread_count_is_chosen_or_typed() {
        const auto e = editor();
        e->settings().setRenderThreadCount(0);
        const auto page = e->settingCatalog()->page(QStringLiteral("editor.Rendering"));
        QVERIFY(page);
        const auto box = page->widget()->findChild<QComboBox *>(QStringLiteral("threads"));
        QVERIFY(box);
        QVERIFY(box->isEditable());
        const int hardware = int(std::max(1u, std::thread::hardware_concurrency()));
        QList<int> counts;
        for (int i = 0; i < box->count(); ++i) {
            counts.push_back(box->itemData(i).toInt());
        }
        QList<int> expected{0};
        for (int count = 1; count < hardware; count *= 2) {
            expected.push_back(count);
        }
        expected.push_back(hardware);
        QCOMPARE(counts, expected);
        QCOMPARE(box->currentIndex(), 0);
        QVERIFY(box->currentText().startsWith(QStringLiteral("Automatic (")));
        QVERIFY(!page->isModified());

        QString error;
        box->setCurrentIndex(1);
        QVERIFY(page->isModified());
        QVERIFY(page->apply(&error));
        QCOMPARE(e->settings().renderThreadCount(), 1);

        box->setEditText(QStringLiteral("1000"));
        QVERIFY(page->apply(&error));
        QCOMPARE(e->settings().renderThreadCount(), 1000);

        box->setEditText(QStringLiteral("0"));
        QVERIFY(!page->apply(&error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(e->settings().renderThreadCount(), 1000);

        // A count not in the list is shown as typed.
        const auto again = editor();
        again->settings().setRenderThreadCount(6);
        const auto shown = again->settingCatalog()
                               ->page(QStringLiteral("editor.Rendering"))
                               ->widget()
                               ->findChild<QComboBox *>(QStringLiteral("threads"));
        QCOMPARE(shown->currentText(), QStringLiteral("6"));
        again->settings().setRenderThreadCount(0);
        e->settings().setRenderThreadCount(0);
    }

    // The shortcuts of UTAU (its menu resource) where they do not clash with ours, and no key
    // for two commands of a window.
    void the_shortcuts_follow_utau() {
        const auto e = editor();
        const auto window = e->newWindow();
        const QList<std::pair<QString, QString>> expected{
            {QStringLiteral("helloutau.playback.play"),       QStringLiteral("Space; F5")     },
            {QStringLiteral("helloutau.playback.stop"),       QStringLiteral("F7")            },
            {QStringLiteral("helloutau.playback.replay"),     QStringLiteral("Shift+F5")      },
            {QStringLiteral("helloutau.edit.delete"),         QStringLiteral("Del; Shift+Del")},
            {QStringLiteral("helloutau.view.showPitch"),      QStringLiteral("X")             },
            {QStringLiteral("helloutau.view.showParameters"), QStringLiteral("Z")             },
            {QStringLiteral("helloutau.tools.editVoiceBank"), QStringLiteral("Ctrl+G")        },
            {QStringLiteral("helloutau.edit.noteProperties"), QStringLiteral("Ctrl+E")        },
            {QStringLiteral("helloutau.edit.insertRest"),     QStringLiteral("Ctrl+R")        },
            {QStringLiteral("helloutau.edit.mergeNotes"),     QStringLiteral("Ctrl+U")        },
            {QStringLiteral("helloutau.file.save"),           QStringLiteral("Ctrl+S")        },
            {QStringLiteral("helloutau.edit.undo"),           QStringLiteral("Ctrl+Z")        },
            {QStringLiteral("helloutau.select.selectAll"),    QStringLiteral("Ctrl+A")        },
        };
        for (const auto &[id, keys] : expected) {
            const auto action = declaredActionOf(*e, window, id);
            QVERIFY2(action, qPrintable(id));
            QCOMPARE(QKeySequence::listToString(action->shortcuts()), keys);
        }

        QHash<QKeySequence, QString> owners;
        for (const auto action : window->findChildren<QAction *>()) {
            for (const auto &key : action->shortcuts()) {
                const auto owner = owners.value(key);
                QVERIFY2(owner.isEmpty() || owner == action->text(),
                         qPrintable(key.toString() + QLatin1Char(' ') + owner + QLatin1Char(' ') +
                                    action->text()));
                owners.insert(key, action->text());
            }
        }
    }

    // Set Tempo sets the tempo of the first selected note, and Note Properties those of all.
    void the_tempo_and_the_note_properties_are_set_from_the_menu() {
        const auto e = editor();
        const auto window = e->openFile(savedProject(m_dir, "t.usth"));
        QVERIFY(window);
        auto roll = qobject_cast<PianoRoll *>(window->centralWidget());
        const auto setTempo =
            declaredActionOf(*e, window, QStringLiteral("helloutau.edit.setTempo"));
        const auto properties =
            declaredActionOf(*e, window, QStringLiteral("helloutau.edit.noteProperties"));
        QVERIFY(setTempo && properties);
        QVERIFY(!setTempo->isEnabled());
        QCOMPARE(properties->shortcut(), QKeySequence(QStringLiteral("Ctrl+E")));
        roll->selectAll();

        QTimer::singleShot(0, [] {
            const auto dialog = qobject_cast<TempoDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->followBox()->setChecked(false);
            dialog->tempoBox()->setValue(150);
            dialog->accept();
        });
        setTempo->trigger();
        const auto session = window->document()->session();
        QCOMPARE(session->snapshot().tracks[0].notes[0].tempo, std::optional(150.0));

        QTimer::singleShot(0, [] {
            const auto dialog =
                qobject_cast<NotePropertiesDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QTest::keyClicks(dialog->field(NotePropertiesDialog::Lyric), QStringLiteral("ka"));
            dialog->accept();
        });
        properties->trigger();
        QCOMPARE(session->snapshot().tracks[0].notes[0].lyric, QStringLiteral("laka"));
        QCOMPARE(session->currentStep(), 2);
    }

    // Project Properties changes the project in one step, and reads a new voice folder.
    void the_project_properties_are_edited_from_the_menu() {
        const auto e = editor();
        const auto window = e->newWindow();
        const auto properties =
            declaredActionOf(*e, window, QStringLiteral("helloutau.file.properties"));
        QVERIFY(properties);
        QTimer::singleShot(0, [] {
            const auto dialog =
                qobject_cast<ProjectPropertiesDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->nameEdit()->setText(QStringLiteral("renamed"));
            dialog->flagsEdit()->setText(QStringLiteral("B0"));
            dialog->accept();
        });
        properties->trigger();
        const auto session = window->document()->session();
        QCOMPARE(session->snapshot().settings.name, QStringLiteral("renamed"));
        QCOMPARE(session->snapshot().settings.flags, QStringLiteral("B0"));
        QCOMPARE(session->currentStep(), 1);
    }

    void opening_a_project_with_a_missing_synth_tool_opens_properties() {
        const auto e = editor();
        kit::Project project;
        project.settings.resampler = QStringLiteral("missing-resampler.exe");
        kit::Track track;
        kit::Note note;
        note.length = 480;
        note.noteNum = 60;
        track.notes.push_back(note);
        project.tracks.push_back(track);
        const auto path = pathIn(m_dir, "missing-synth-tool.usth");
        kit::DiagnosticList diagnostics;
        QVERIFY(project.save(path, diagnostics));

        // The dialog is closed before any check, so that a failed check does not leave the modal
        // dialog open.
        bool opened = false;
        bool marked = false;
        QTimer::singleShot(0, [&opened, &marked] {
            const auto dialog =
                qobject_cast<ProjectPropertiesDialog *>(QApplication::activeModalWidget());
            if (!dialog) {
                return;
            }
            opened = true;
            const auto actions = dialog->resamplerEdit()->actions();
            marked = !actions.isEmpty() && actions.constFirst()->isVisible();
            dialog->reject();
        });
        const auto window = e->openFile(path);
        QVERIFY(opened);
        QVERIFY(marked);
        QVERIFY(window);
        QCOMPARE(window->document()->sourcePath(), path);
    }

    void the_edit_commands_follow_the_selection() {
        const auto e = editor();
        const auto window = e->openFile(savedProject(m_dir, "d.usth"));
        QVERIFY(window);
        auto roll = qobject_cast<PianoRoll *>(window->centralWidget());
        const auto remove = declaredActionOf(*e, window, QStringLiteral("helloutau.edit.delete"));
        const auto split = declaredActionOf(*e, window, QStringLiteral("helloutau.edit.splitNote"));
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

        declaredActionOf(*e, window, QStringLiteral("helloutau.select.penTool"))->trigger();
        QCOMPARE(roll->tool(), PianoRoll::PenTool);
        roll->setQuantization(60);
        window->setDocument(std::make_unique<kit::ProjectDocument>());
        roll = qobject_cast<PianoRoll *>(window->centralWidget());
        QCOMPARE(roll->tool(), PianoRoll::PenTool);
        QCOMPARE(roll->quantization(), 60);
    }

    // The window gives its piano roll, where plugins find the selection, and tells when a new
    // document replaced it.
    void the_piano_roll_is_replaced_with_the_document() {
        const auto e = editor();
        const auto window = e->newWindow();
        const auto first = window->pianoRoll();
        QVERIFY(first);
        QVERIFY(first == window->centralWidget());

        QSignalSpy changed(window, &ProjectWindow::documentChanged);
        window->setDocument(std::make_unique<kit::ProjectDocument>());
        QCOMPARE(changed.count(), 1);
        QVERIFY(window->pianoRoll() != first);
        QVERIFY(window->pianoRoll() == window->centralWidget());
    }

    // Mode2 is checked as the project has it, and turned over as an undo step. The pitch tool is
    // enabled while Mode2 is off and the pitch shown, and otherwise gives way to the select
    // tool.
    void mode2_follows_the_project() {
        const auto e = editor();
        const auto window = e->newWindow();
        const auto roll = qobject_cast<PianoRoll *>(window->centralWidget());
        const auto session = window->document()->session();
        const auto mode2 = declaredActionOf(*e, window, QStringLiteral("helloutau.edit.mode2"));
        const auto pitchTool =
            declaredActionOf(*e, window, QStringLiteral("helloutau.select.pitchTool"));
        const auto showPitch =
            declaredActionOf(*e, window, QStringLiteral("helloutau.view.showPitch"));
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

        declaredActionOf(*e, window, QStringLiteral("helloutau.edit.undo"))->trigger();
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

        // The ruler shows the render states once the cache is scanned on a worker thread: the
        // sung note without its fragment, and the silent note without a sample.
        QTRY_COMPARE(roll->renderStates(), (QList<PianoRoll::RenderState>{
                                               PianoRoll::RenderWaiting, PianoRoll::RenderSilent}));
    }

private:
    // A voice bank in UTF-8, declared, so that nothing is asked: in the root a.wav with an
    // entry, b.wav an entry without an alias and without its file, and c.wav a file without an
    // entry; in sub, x.wav with the aliases x and y.
    static fs::path voiceBank(const QTemporaryDir &dir, const char *name = "bank") {
        const auto bank = pathIn(dir, name);
        fs::create_directories(bank / "sub");
        const auto write = [](const fs::path &path, const char *text) {
            std::ofstream file(path, std::ios::binary);
            file << text;
        };
        write(bank / "oto.ini", "#Charset:UTF-8\r\na.wav=a,10,20,-30,40,5\r\nb.wav=,1,2,3,4,5\r\n");
        write(bank / "a.wav", "");
        write(bank / "c.wav", "");
        write(bank / "sub" / "oto.ini",
              "#Charset:UTF-8\r\nx.wav=x,9.0,2,3,4,5\r\nx.wav=y,1,2,3,4,5\r\n");
        write(bank / "sub" / "x.wav", "");
        return bank;
    }

    static void writeFile(const fs::path &path, const char *text) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << text;
    }

    static void writeFile(const fs::path &path, const char *bytes, qsizetype size) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(bytes, std::streamsize(size));
    }

    // A WAVE file of 16-bit mono PCM at 1000 Hz, one frame per millisecond
    static void writeWave(const fs::path &path, int frames) {
        QByteArray bytes;
        const auto put32 = [&bytes](quint32 value) {
            for (int i = 0; i < 4; ++i) {
                bytes.push_back(char((value >> (8 * i)) & 0xff));
            }
        };
        const auto put16 = [&bytes](quint16 value) {
            bytes.push_back(char(value & 0xff));
            bytes.push_back(char(value >> 8));
        };
        bytes.append("RIFF");
        put32(quint32(36 + frames * 2));
        bytes.append("WAVEfmt ");
        put32(16);
        put16(1);
        put16(1);
        put32(1000);
        put32(2000);
        put16(2);
        put16(16);
        bytes.append("data");
        put32(quint32(frames * 2));
        for (int i = 0; i < frames; ++i) {
            put16(quint16(i % 2 ? 8000 : -8000));
        }
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(bytes.constData(), bytes.size());
    }

    static int entryCount(const VoiceBankWindow *window) {
        int count = 0;
        const auto bank = window->document()->session()->snapshot();
        for (const auto &sample : bank.samples()) {
            count += sample.hasEntry ? 1 : 0;
        }
        return count;
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
    // A changed oto.ini is asked about and read as one undo step; declined, it is listed in the
    // bar and not asked about again until it changes again. A new folder waits in the bar, and a
    // new audio file is taken at once.
    void the_voice_bank_window_follows_the_disk() {
        QTemporaryDir dir;
        const auto bank = voiceBank(dir);
        const auto e = editor();
        const auto window = e->openVoiceBank(bank);
        QVERIFY(window);
        QVERIFY(window->changeBar()->isHidden());
        const auto session = window->document()->session();
        QCOMPARE(entryCount(window), 4);

        writeFile(bank / "oto.ini",
                  "#Charset:UTF-8\r\na.wav=a,10,20,-30,40,5\r\nb.wav=,1,2,3,4,5\r\n"
                  "c.wav=c,1,2,3,4,5\r\n");
        bool asked = false;
        QTimer::singleShot(0, [&asked] {
            const auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            asked = true;
            box->button(QMessageBox::Yes)->click();
        });
        window->checkDisk();
        QVERIFY(asked);
        QCOMPARE(entryCount(window), 5);
        QVERIFY(window->changeBar()->isHidden());
        session->undo();
        QCOMPARE(entryCount(window), 4);
        session->redo();

        // Declined, then checked again without a question, then changed again
        writeFile(bank / "sub" / "oto.ini", "#Charset:UTF-8\r\nx.wav=x,9.0,2,3,4,5\r\n");
        asked = false;
        QTimer::singleShot(0, [&asked] {
            const auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            asked = true;
            box->button(QMessageBox::No)->click();
        });
        window->checkDisk();
        QVERIFY(asked);
        QVERIFY(!window->changeBar()->isHidden());
        QCOMPARE(entryCount(window), 5);
        window->checkDisk();
        QVERIFY(QApplication::activeModalWidget() == nullptr);
        QVERIFY(!window->changeBar()->isHidden());

        writeFile(bank / "sub" / "oto.ini", "#Charset:UTF-8\r\nx.wav=z,9.0,2,3,4,5\r\n");
        asked = false;
        QTimer::singleShot(0, [&asked] {
            const auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            asked = true;
            box->button(QMessageBox::No)->click();
        });
        window->checkDisk();
        QVERIFY(asked);

        // The bar reads what it lists.
        QPushButton *readAgain = nullptr;
        for (const auto button : window->changeBar()->findChildren<QPushButton *>()) {
            if (button->text() == QStringLiteral("&Read Again")) {
                readAgain = button;
            }
        }
        QVERIFY(readAgain && readAgain->isVisible());
        readAgain->click();
        QCOMPARE(entryCount(window), 4);
        QVERIFY(window->changeBar()->isHidden());

        // A new audio file is taken as no step; a new folder waits in the bar.
        const int step = session->currentStep();
        writeFile(bank / "d.wav", "");
        fs::create_directories(bank / "new");
        writeFile(bank / "new" / "oto.ini", "#Charset:UTF-8\r\nn.wav=n,1,2,3,4,5\r\n");
        writeFile(bank / "new" / "n.wav", "");
        window->checkDisk();
        QCOMPARE(session->currentStep(), step);
        QVERIFY(!window->changeBar()->isHidden());
        QCOMPARE(entryCount(window), 4);
        bool unlisted = false;
        const auto model = window->entryModel();
        window->directoryTree()->setCurrentItem(window->directoryTree()->topLevelItem(0));
        for (int row = 0; row < model->rowCount(); ++row) {
            unlisted =
                unlisted || model->index(row, VoiceBankEntryModel::FileColumn).data().toString() ==
                                QStringLiteral("d.wav");
        }
        QVERIFY(unlisted);

        // Read All reads everything, the new folder too.
        window->reloadAll();
        QCOMPARE(entryCount(window), 5);
        QVERIFY(window->changeBar()->isHidden());
    }

    void a_voice_bank_opens_in_a_window_of_its_own() {
        QTemporaryDir dir;
        const auto bank = voiceBank(dir);
        const auto e = editor();
        const auto window = e->openVoiceBank(bank);
        QVERIFY(window);
        QCOMPARE(e->voiceBankWindows(), QList<VoiceBankWindow *>{window});
        QCOMPARE(e->settings().recentVoiceBanks().value(0), bank);
        QCOMPARE(window->windowTitle(), QStringLiteral("bank[*] - HelloUtau"));
        QVERIFY(!window->isWindowModified());
        QStringList menus;
        for (const auto action : window->menuBar()->actions()) {
            menus.push_back(action->text());
        }
        QCOMPARE(menus, (QStringList{QStringLiteral("&File"), QStringLiteral("&Edit"),
                                     QStringLiteral("&View"), QStringLiteral("&Playback"),
                                     QStringLiteral("&Tools"), QStringLiteral("&Help")}));

        // All folders at first: a, b missing, c unlisted, and x and y in sub
        const auto tree = window->directoryTree();
        QCOMPARE(tree->topLevelItemCount(), 2);
        QCOMPARE(tree->topLevelItem(1)->text(0), QStringLiteral("bank"));
        QCOMPARE(tree->topLevelItem(1)->child(0)->text(0), QStringLiteral("sub"));
        QCOMPARE(kindsOf(window),
                 (QList<int>{VoiceBankEntryModel::EntryRow, VoiceBankEntryModel::MissingAudioRow,
                             VoiceBankEntryModel::UnlistedAudioRow, VoiceBankEntryModel::EntryRow,
                             VoiceBankEntryModel::EntryRow}));
        QVERIFY(!window->entryTable()->isColumnHidden(VoiceBankEntryModel::DirectoryColumn));

        tree->setCurrentItem(tree->topLevelItem(1)->child(0));
        QCOMPARE(kindsOf(window),
                 (QList<int>{VoiceBankEntryModel::EntryRow, VoiceBankEntryModel::EntryRow}));
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
        QVERIFY(declaredActionOf(*e, window, QStringLiteral("helloutau.edit.undo"))->isEnabled());
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

    // Every editable cell of an entry edits it as one undo step; an unlisted file is included by
    // editing its alias. A value that is not a number, and an alias that another entry of the
    // file has, are refused with a message.
    void the_entry_table_edits_the_entries() {
        QTemporaryDir dir;
        const auto bank = voiceBank(dir);
        const auto e = editor();
        const auto window = e->openVoiceBank(bank);
        QVERIFY(window);
        const auto session = window->document()->session();
        const auto tree = window->directoryTree();
        tree->setCurrentItem(tree->topLevelItem(1));
        const auto model = window->entryModel();
        QSignalSpy rejected(model, &VoiceBankEntryModel::editRejected);
        QCOMPARE(model->rowCount(), 3);
        QVERIFY(!(model->flags(model->index(0, VoiceBankEntryModel::DirectoryColumn)) &
                  Qt::ItemIsEditable));
        QVERIFY(!(model->flags(model->index(0, VoiceBankEntryModel::FileColumn)) &
                  Qt::ItemIsEditable));
        QVERIFY(
            !(model->flags(model->index(2, VoiceBankEntryModel::FileColumn)) & Qt::ItemIsEditable));

        const auto play =
            declaredActionOf(*e, window, QStringLiteral("helloutau.voiceBank.playAudio"));
        const auto playSpan =
            declaredActionOf(*e, window, QStringLiteral("helloutau.voiceBank.playSpan"));
        QVERIFY(play);
        QVERIFY(playSpan);
        QSignalSpy playTriggered(play, &QAction::triggered);
        QSignalSpy playSpanTriggered(playSpan, &QAction::triggered);
        window->entryTable()->setFocus();
        QTest::keyClick(window->entryTable(), Qt::Key_Space);
        QTest::keyClick(window->entryTable(), Qt::Key_Space, Qt::ShiftModifier);
        QCOMPARE(playTriggered.size(), 1);
        QCOMPARE(playSpanTriggered.size(), 1);
        play->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Space));
        QTest::keyClick(window->entryTable(), Qt::Key_Space, Qt::ControlModifier);
        QCOMPARE(playTriggered.size(), 2);

        // The alias and a value, each one step; the edited row stays current.
        window->setCurrentRow(0);
        const int step = session->currentStep();
        QVERIFY(model->setData(model->index(0, VoiceBankEntryModel::AliasColumn),
                               QStringLiteral("aa")));
        QVERIFY(model->setData(model->index(0, VoiceBankEntryModel::OffsetColumn),
                               QStringLiteral(" 15.5")));
        QCOMPARE(session->currentStep(), step + 2);
        QCOMPARE(window->currentRow(), 0);
        QCOMPARE(model->index(0, VoiceBankEntryModel::OffsetColumn).data().toString(),
                 QStringLiteral("15.5"));

        // The message is shown once the editor would have closed, and answered after it.
        QVERIFY(!model->setData(model->index(0, VoiceBankEntryModel::CutoffColumn),
                                QStringLiteral("abc")));
        answerMessageBox(QMessageBox::Ok);
        QCoreApplication::processEvents();
        QCOMPARE(rejected.size(), 1);
        QCOMPARE(session->currentStep(), step + 2);

        // An unlisted file is included by its alias, in one step.
        QVERIFY(model->setData(model->index(2, VoiceBankEntryModel::AliasColumn),
                               QStringLiteral("cc")));
        QCOMPARE(session->currentStep(), step + 3);
        QCOMPARE(model->index(2, 0).data(VoiceBankEntryModel::RowKindRole).toInt(),
                 int(VoiceBankEntryModel::EntryRow));
        QCOMPARE(model->entryOf(2).alias, QStringLiteral("cc"));
        session->undo();
        QCoreApplication::processEvents();
        QCOMPARE(model->index(2, 0).data(VoiceBankEntryModel::RowKindRole).toInt(),
                 int(VoiceBankEntryModel::UnlistedAudioRow));
        session->redo();
        QCoreApplication::processEvents();

        // In sub, y cannot become x, which x.wav already has.
        tree->setCurrentItem(tree->topLevelItem(1)->child(0));
        QVERIFY(!model->setData(model->index(1, VoiceBankEntryModel::AliasColumn),
                                QStringLiteral("x")));
        answerMessageBox(QMessageBox::Ok);
        QCoreApplication::processEvents();
        QCOMPARE(rejected.size(), 2);
        QCOMPARE(model->entryOf(1).alias, QStringLiteral("y"));

        QVERIFY(window->save());
        std::ifstream in(bank / "oto.ini", std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
        QCOMPARE(QByteArray::fromStdString(text),
                 QByteArray("#Charset:UTF-8\r\na.wav=aa,15.5,20,-30,40,5\r\nb.wav=,1,2,3,4,5\r\n"
                            "c.wav=cc,0,0,0,0,0\r\n"));
    }

    // Insert, Duplicate, Include and Delete each make one undo step and select what they made.
    void the_entry_table_inserts_duplicates_includes_and_removes() {
        QTemporaryDir dir;
        const auto bank = voiceBank(dir);
        const auto e = editor();
        const auto window = e->openVoiceBank(bank);
        QVERIFY(window);
        const auto session = window->document()->session();
        const auto tree = window->directoryTree();
        tree->setCurrentItem(tree->topLevelItem(1));
        const auto model = window->entryModel();
        const auto aliases = [model] {
            QStringList list;
            for (int row = 0; row < model->rowCount(); ++row) {
                list.push_back(model->entryOf(row).fileName + QLatin1Char('=') +
                               model->entryOf(row).alias);
            }
            return list;
        };
        const auto duplicate =
            declaredActionOf(*e, window, QStringLiteral("helloutau.voiceBank.duplicateEntries"));
        const auto include =
            declaredActionOf(*e, window, QStringLiteral("helloutau.voiceBank.includeAudio"));
        const auto remove = declaredActionOf(*e, window, QStringLiteral("helloutau.edit.delete"));
        QVERIFY(duplicate && include && remove);

        // Only an entry is duplicated or removed, and only an unlisted file included.
        window->setCurrentRow(2);
        QVERIFY(!duplicate->isEnabled());
        QVERIFY(!remove->isEnabled());
        QVERIFY(include->isEnabled());
        window->setCurrentRow(0);
        QVERIFY(duplicate->isEnabled());
        QVERIFY(!include->isEnabled());

        // a2 after a, twice a3 as the next free name
        const int step = session->currentStep();
        duplicate->trigger();
        QCOMPARE(session->currentStep(), step + 1);
        QCOMPARE(aliases(), (QStringList{QStringLiteral("a.wav=a"), QStringLiteral("a.wav=a2"),
                                         QStringLiteral("b.wav="), QStringLiteral("c.wav=")}));
        QCOMPARE(window->selectedRows(), QList<int>{1});
        window->setCurrentRow(0);
        duplicate->trigger();
        QCOMPARE(model->entryOf(2).alias, QStringLiteral("a3"));

        // The unlisted file and an entry selected: Include takes the file alone.
        window->setCurrentRow(4);
        include->trigger();
        QCOMPARE(session->currentStep(), step + 3);
        QCOMPARE(model->index(4, 0).data(VoiceBankEntryModel::RowKindRole).toInt(),
                 int(VoiceBankEntryModel::EntryRow));
        QCOMPARE(window->selectedRows(), QList<int>{4});

        // b.wav and a3 removed in one step
        window->setCurrentRow(2);
        window->entryTable()->selectionModel()->select(window->entryTable()->model()->index(3, 0),
                                                       QItemSelectionModel::Select |
                                                           QItemSelectionModel::Rows);
        remove->trigger();
        QCOMPARE(session->currentStep(), step + 4);
        QCOMPARE(aliases(), (QStringList{QStringLiteral("a.wav=a"), QStringLiteral("a.wav=a2"),
                                         QStringLiteral("c.wav=")}));

        // An entry for a.wav, whose stem is taken: a3
        window->setCurrentRow(0);
        QTimer::singleShot(0, [] {
            const auto input = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            QVERIFY(input);
            QCOMPARE(input->textValue(), QStringLiteral("a.wav"));
            input->accept();
        });
        QVERIFY(window->insertEntry());
        QCOMPARE(session->currentStep(), step + 5);
        QCOMPARE(aliases(), (QStringList{QStringLiteral("a.wav=a"), QStringLiteral("a.wav=a2"),
                                         QStringLiteral("a.wav=a3"), QStringLiteral("c.wav=")}));
        QCOMPARE(window->currentRow(), 2);

        // An entry for c.wav, whose stem is free: an empty alias
        window->setCurrentRow(3);
        QTimer::singleShot(0, [] {
            const auto input = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            QVERIFY(input);
            input->setTextValue(QStringLiteral("d.wav"));
            input->accept();
        });
        QVERIFY(window->insertEntry());
        QCOMPARE(model->entryOf(4).fileName, QStringLiteral("d.wav"));
        QCOMPARE(model->entryOf(4).alias, QString());
        QCOMPARE(model->index(4, 0).data(VoiceBankEntryModel::RowKindRole).toInt(),
                 int(VoiceBankEntryModel::MissingAudioRow));

        QCOMPARE(session->currentStep(), step + 6);
        for (int i = 0; i < 6; ++i) {
            session->undo();
        }
        QCoreApplication::processEvents();
        QCOMPARE(aliases(), (QStringList{QStringLiteral("a.wav=a"), QStringLiteral("b.wav="),
                                         QStringLiteral("c.wav=")}));
    }

    // The table highlights the matches of the find bar in the aliases, in the file name for an
    // empty alias, whose stem is matched, and in the file names in their scope. A query without
    // matches keeps the current row, so that a cell is compared in the same state of selection.
    void matches_are_highlighted_in_the_entry_table() {
        QTemporaryDir dir;
        const auto e = editor();
        const auto window = e->openVoiceBank(voiceBank(dir));
        QVERIFY(window);
        window->resize(1000, 600);
        window->show();
        const auto tree = window->directoryTree();
        tree->setCurrentItem(tree->topLevelItem(1));
        const auto table = window->entryTable();
        const auto cell = [table](int row, int column) {
            const auto rect = table->visualRect(table->model()->index(row, column));
            return table->viewport()->grab(rect).toImage();
        };
        const auto bar = window->findChild<FindBar *>();
        declaredActionOf(*e, window, QStringLiteral("helloutau.edit.find"))->trigger();

        bar->setText(QStringLiteral("a"));
        QCOMPARE(window->currentRow(), 0);
        auto alias = cell(0, VoiceBankEntryModel::AliasColumn);
        auto file = cell(0, VoiceBankEntryModel::FileColumn);
        bar->setText(QStringLiteral("none"));
        QVERIFY(cell(0, VoiceBankEntryModel::AliasColumn) != alias);
        QVERIFY(cell(0, VoiceBankEntryModel::FileColumn) == file);

        // b.wav has an empty alias.
        bar->setText(QStringLiteral("b"));
        QCOMPARE(window->currentRow(), 1);
        file = cell(1, VoiceBankEntryModel::FileColumn);
        bar->setText(QStringLiteral("none"));
        QVERIFY(cell(1, VoiceBankEntryModel::FileColumn) != file);

        bar->setScope(1);
        bar->setText(QStringLiteral("c"));
        QCOMPARE(window->currentRow(), 2);
        file = cell(2, VoiceBankEntryModel::FileColumn);
        bar->setText(QStringLiteral("none"));
        QVERIFY(cell(2, VoiceBankEntryModel::FileColumn) != file);

        // Closing the find bar removes the orange of the highlight from a row not selected.
        const auto orange = [&cell](int row) {
            const auto image = cell(row, VoiceBankEntryModel::FileColumn);
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    const auto pixel = image.pixelColor(x, y);
                    if (pixel.red() - pixel.blue() > 40 && pixel.red() - pixel.green() > 20) {
                        return true;
                    }
                }
            }
            return false;
        };
        bar->setText(QStringLiteral("wav"));
        QCOMPARE(window->currentRow(), 2);
        QVERIFY(orange(0));
        QTest::keyClick(bar->findField(), Qt::Key_Escape);
        QVERIFY(!orange(0));
    }

    // The find bar of the voice bank window searches the names of the entries, an empty alias
    // counting as the stem of the file name, or the file names. Only aliases are replaced, each
    // replacement as one undo step. A replacement that gives two entries of one audio file the
    // same name is refused.
    void aliases_and_file_names_are_found_and_replaced() {
        QTemporaryDir dir;
        const auto bank = voiceBank(dir);
        const auto e = editor();
        const auto window = e->openVoiceBank(bank);
        QVERIFY(window);
        const auto session = window->document()->session();
        const auto tree = window->directoryTree();
        tree->setCurrentItem(tree->topLevelItem(1));
        const auto model = window->entryModel();
        const auto bar = window->findChild<FindBar *>();
        QVERIFY(bar);
        declaredActionOf(*e, window, QStringLiteral("helloutau.edit.find"))->trigger();
        QCOMPARE(bar->scopes().size(), 2);

        bar->setText(QStringLiteral("b"));
        QCOMPARE(window->currentRow(), 1);
        QCOMPARE(bar->resultText(), QStringLiteral("1 of 1"));
        // c.wav has no entry and therefore no alias.
        bar->setText(QStringLiteral("c"));
        QCOMPARE(bar->resultText(), QStringLiteral("No results"));
        bar->setScope(1);
        QCOMPARE(window->currentRow(), 2);
        QVERIFY(!bar->replaceField()->isEnabled());

        bar->setScope(0);
        QVERIFY(bar->replaceField()->isEnabled());
        bar->setRegularExpression(true);
        bar->setText(QStringLiteral("^"));
        bar->setReplacement(QStringLiteral("x_"));
        const int step = session->currentStep();
        QTest::keyClick(bar->findField(), Qt::Key_Return, Qt::ControlModifier | Qt::AltModifier);
        QCOMPARE(session->currentStep(), step + 1);
        QCOMPARE(model->entryOf(0).alias, QStringLiteral("x_a"));
        QCOMPARE(model->entryOf(1).alias, QStringLiteral("x_b"));

        // x.wav of the subfolder has the entries x and y.
        tree->setCurrentItem(tree->topLevelItem(1)->child(0));
        bar->setRegularExpression(false);
        bar->setText(QStringLiteral("y"));
        QCOMPARE(window->currentRow(), 1);
        bar->setReplacement(QStringLiteral("x"));
        answerMessageBox(QMessageBox::Ok);
        QTest::keyClick(bar->findField(), Qt::Key_Return, Qt::ControlModifier | Qt::AltModifier);
        QCOMPARE(session->currentStep(), step + 1);
        QCOMPARE(model->entryOf(1).alias, QStringLiteral("y"));

        bar->setReplacement(QStringLiteral("z"));
        QTest::keyClick(bar->replaceField(), Qt::Key_Return);
        QCOMPARE(session->currentStep(), step + 2);
        QCOMPARE(model->entryOf(1).alias, QStringLiteral("z"));

        // An empty alias is refused, as the renaming of aliases refuses it, although the stem a
        // of a.wav is free.
        tree->setCurrentItem(tree->topLevelItem(1));
        bar->setText(QStringLiteral("x_a"));
        QCOMPARE(window->currentRow(), 0);
        bar->setReplacement(QString());
        answerMessageBox(QMessageBox::Ok);
        QTest::keyClick(bar->replaceField(), Qt::Key_Return);
        QCOMPARE(session->currentStep(), step + 2);
        QCOMPARE(model->entryOf(0).alias, QStringLiteral("x_a"));
    }

    // The waveform shows the current entry; a drag and the keys 1 to 5 over it edit the entry,
    // each as one undo step, and an unlisted file is included by them.
    void the_waveform_edits_the_current_entry() {
        QTemporaryDir dir;
        const auto bank = voiceBank(dir);
        writeWave(bank / "a.wav", 1000);
        writeWave(bank / "c.wav", 500);
        const auto e = editor();
        const auto window = e->openVoiceBank(bank);
        QVERIFY(window);
        window->resize(1000, 700);
        window->show();
        const auto session = window->document()->session();
        const auto tree = window->directoryTree();
        tree->setCurrentItem(tree->topLevelItem(1));
        const auto view = window->waveformView();
        QVERIFY(!view->audio());

        window->setCurrentRow(0);
        QVERIFY(view->audio());
        QCOMPARE(view->duration(), 1000.0);
        QCOMPARE(view->entry()->preUtterance, 40.0);

        // The pre-utterance at 50 dragged to 200
        const int step = session->currentStep();
        const auto viewport = view->viewport();
        const auto point = [view](double time) { return QPointF(view->xOf(time), 100).toPoint(); };
        QTest::mousePress(viewport, Qt::LeftButton, {}, point(50));
        QTest::mouseMove(viewport, point(120));
        QTest::mouseMove(viewport, point(200));
        QTest::mouseRelease(viewport, Qt::LeftButton, {}, point(200));
        QCOMPARE(session->currentStep(), step + 1);
        QCOMPARE(window->entryModel()->entryOf(0).preUtterance, 190.0);
        QCOMPARE(window->currentRow(), 0);

        // The key 2 sets the overlap at the pointer, the whole millisecond of the pixel nearest
        // to 100 ms, after the offset of 10 ms.
        view->setFocus();
        QTest::mouseMove(viewport, point(100));
        QVERIFY(view->pointerTime() && std::abs(*view->pointerTime() - 100) < 1);
        QTest::keyClick(view, Qt::Key_2);
        QCOMPARE(session->currentStep(), step + 2);
        QCOMPARE(window->entryModel()->entryOf(0).voiceOverlap,
                 std::round(*view->pointerTime()) - 10);
        session->undo();
        QCoreApplication::processEvents();
        QCOMPARE(view->entry()->voiceOverlap, 5.0);

        // Elsewhere the key is typed, with the pointer still over the waveform.
        window->searchBox()->setFocus();
        QTest::keyClick(window->searchBox(), Qt::Key_2);
        QCOMPARE(window->searchBox()->text(), QStringLiteral("2"));
        QCOMPARE(session->currentStep(), step + 1);
        window->searchBox()->clear();

        // The key 1 on the unlisted c.wav includes it with its offset.
        window->setCurrentRow(2);
        QCOMPARE(view->duration(), 500.0);
        view->setFocus();
        QTest::mouseMove(viewport, point(30));
        QTest::keyClick(view, Qt::Key_1);
        QCOMPARE(window->entryModel()->index(2, 0).data(VoiceBankEntryModel::RowKindRole).toInt(),
                 int(VoiceBankEntryModel::EntryRow));
        QCOMPARE(window->entryModel()->entryOf(2).offset, 30.0);
        QCOMPARE(window->currentRow(), 2);

        // The missing b.wav has no audio to show values on.
        window->setCurrentRow(1);
        QVERIFY(!view->audio());
        window->hide();
    }

    // Play Audio File plays the audio of the current entry, and again stops it.
    void the_audio_file_of_the_current_entry_plays() {
        if (AudioEngine::instance()->sampleRate() <= 0) {
            QSKIP("This machine has no audio output device.");
        }
        QTemporaryDir dir;
        const auto bank = voiceBank(dir);
        writeWave(bank / "a.wav", 1000);
        const auto e = editor();
        const auto window = e->openVoiceBank(bank);
        QVERIFY(window);
        const auto tree = window->directoryTree();
        tree->setCurrentItem(tree->topLevelItem(1));
        window->setCurrentRow(0);
        const auto play =
            declaredActionOf(*e, window, QStringLiteral("helloutau.voiceBank.playAudio"));
        const auto stop = declaredActionOf(*e, window, QStringLiteral("helloutau.playback.stop"));
        QVERIFY(play && stop);
        QVERIFY(!stop->isEnabled());
        play->trigger();
        QVERIFY(stop->isEnabled());
        play->trigger();
        QVERIFY(!stop->isEnabled());

        // Another entry stops it.
        play->trigger();
        QVERIFY(stop->isEnabled());
        window->setCurrentRow(1);
        QVERIFY(!stop->isEnabled());
    }

    // The audio files that carry metadata are listed, and written again without it once the
    // user agrees.
    void the_metadata_of_the_audio_files_is_removed() {
        QTemporaryDir dir;
        const auto bank = voiceBank(dir);
        writeWave(bank / "a.wav", 100);
        writeWave(bank / "sub" / "x.wav", 100);
        {
            std::ofstream file(bank / "sub" / "x.wav", std::ios::binary | std::ios::app);
            file.write("LIST\x04\0\0\0abcd", 12);
        }
        const auto size = [&bank] { return fs::file_size(bank / "sub" / "x.wav"); };
        const auto e = editor();
        const auto window = e->openVoiceBank(bank);
        QVERIFY(window);

        answerMessageBox(QMessageBox::No);
        QVERIFY(!window->removeAudioMetadata());
        QCOMPARE(size(), std::uintmax_t(44 + 200 + 12));

        answerMessageBox(QMessageBox::Yes);
        QCOMPARE(window->removeAudioMetadata(), std::optional<int>(1));
        QCOMPARE(size(), std::uintmax_t(44 + 200));
        QCOMPARE(fs::file_size(bank / "a.wav"), std::uintmax_t(44 + 200));

        answerMessageBox(QMessageBox::Ok);
        QVERIFY(!window->removeAudioMetadata());
    }

    // An alias that another entry of the same file has, as read, is marked.
    void the_entry_table_marks_a_repeated_alias() {
        QTemporaryDir dir;
        const auto bank = voiceBank(dir);
        writeFile(bank / "sub" / "oto.ini",
                  "#Charset:UTF-8\r\nx.wav=x,9.0,2,3,4,5\r\nx.wav=,1,2,3,4,5\r\n"
                  "x.wav=y,1,2,3,4,5\r\n");
        const auto e = editor();
        const auto window = e->openVoiceBank(bank);
        QVERIFY(window);
        const auto tree = window->directoryTree();
        tree->setCurrentItem(tree->topLevelItem(1)->child(0));
        const auto model = window->entryModel();
        QCOMPARE(model->rowCount(), 3);
        QVERIFY(model->index(0, 0).data(VoiceBankEntryModel::DuplicateAliasRole).toBool());
        QVERIFY(model->index(1, 0).data(VoiceBankEntryModel::DuplicateAliasRole).toBool());
        QVERIFY(!model->index(2, 0).data(VoiceBankEntryModel::DuplicateAliasRole).toBool());
    }

    // The frequency table of the current entry is drawn over its audio, in the format of the
    // resampler of the settings at first, another chosen in the box; the View menu shows the
    // spectrogram in place of the waveform.
    void the_frequency_table_of_the_entry_is_shown() {
        QTemporaryDir dir;
        const auto bank = voiceBank(dir);
        writeWave(bank / "a.wav", 1000);
        writeWave(bank / "c.wav", 1000);
        const auto le = [](auto value) {
            QByteArray bytes(sizeof(value), '\0');
            qToLittleEndian(value, bytes.data());
            return bytes;
        };
        // a_wav.frq at 220 Hz, and an entry of desc.mrq at 440 Hz, a frame each 10 ms
        QByteArray frq("FREQ0003");
        frq += le(qint32(10)) + le(220.0) + QByteArray(16, '\0') + le(qint32(100));
        for (int i = 0; i < 100; ++i) {
            frq += le(220.0) + le(1.0);
        }
        writeFile(bank / "a_wav.frq", frq.constData(), frq.size());
        QByteArray data = le(qint32(100)) + le(qint32(1000)) + le(qint32(10));
        for (int i = 0; i < 100; ++i) {
            data += le(440.0f);
        }
        QByteArray mrq("mrq ");
        mrq += le(qint32(2)) + le(qint32(1)) + le(qint32(5));
        for (const auto c : QStringLiteral("a.wav")) {
            mrq += le(quint16(c.unicode()));
        }
        mrq += le(qint32(data.size())) + data;
        writeFile(bank / "desc.mrq", mrq.constData(), mrq.size());

        const auto e = editor();
        e->settings().setResampler(QStringLiteral("C:/synthTools/moresampler.exe"));
        const auto window = e->openVoiceBank(bank);
        QVERIFY(window);
        const auto tree = window->directoryTree();
        tree->setCurrentItem(tree->topLevelItem(1));
        window->setCurrentRow(0);
        const auto view = window->waveformView();
        const auto box = window->frequencyFormatBox();
        QVERIFY(box);
        QCOMPARE(box->currentData().toString(), QStringLiteral("mrq"));
        QVERIFY(view->frequencyTable());
        QCOMPARE(view->frequencyTable()->frames.at(50).frequency, 440.0);
        QCOMPARE(view->frequencyTable()->frames.at(50).time, 500.0);
        QCOMPARE(box->itemText(box->findData(QStringLiteral("dio"))),
                 QStringLiteral("dio (world4utau) (none)"));

        box->setCurrentIndex(box->findData(QStringLiteral("frq")));
        QCOMPARE(view->frequencyTable()->frames.at(50).frequency, 220.0);
        box->setCurrentIndex(box->findData(QString()));
        QVERIFY(!view->frequencyTable());
        box->setCurrentIndex(box->findData(QStringLiteral("frq")));

        // c.wav has no table.
        window->setCurrentRow(2);
        QVERIFY(!view->frequencyTable());
        QCOMPARE(box->itemText(box->findData(QStringLiteral("frq"))),
                 QStringLiteral("frq (resampler.exe) (none)"));

        const auto spectrogram =
            declaredActionOf(*e, window, QStringLiteral("helloutau.voiceBank.showSpectrogram"));
        QVERIFY(spectrogram && !spectrogram->isChecked());
        QVERIFY(!view->spectrogram());
        spectrogram->trigger();
        QVERIFY(view->spectrogram());
        window->setCurrentRow(0);
        QVERIFY(view->spectrogram());
        spectrogram->trigger();
        QVERIFY(!view->spectrogram());
    }

    // The box of formats follows the registrations: the format chosen stays while it is
    // registered, and gives way to that of the resampler when it goes.
    void the_frequency_box_follows_the_registrations() {
        QTemporaryDir dir;
        const auto e = editor();
        e->settings().setResampler(QStringLiteral("C:/synthTools/moresampler.exe"));
        const auto window = e->openVoiceBank(voiceBank(dir));
        QVERIFY(window);
        const auto box = window->frequencyFormatBox();
        QVERIFY(box);
        box->setCurrentIndex(box->findData(QStringLiteral("frq")));

        class Extra : public kit::FrequencyFormat {
        public:
            QString id() const override {
                return QStringLiteral("extra");
            }
            QString name() const override {
                return QStringLiteral("Extra");
            }
            QStringList resamplerPatterns() const override {
                return {};
            }
            bool exists(const fs::path &) const override {
                return false;
            }
            std::optional<kit::FrequencyTable> read(const fs::path &, int,
                                                    kit::DiagnosticList &) const override {
                return std::nullopt;
            }
        };
        auto extra =
            kit::FrequencyFormatRegistry::Add<Extra>(e->frequencyFormats().registry(), "extra", {});
        QVERIFY(box->findData(QStringLiteral("extra")) > 0);
        QCOMPARE(box->currentData().toString(), QStringLiteral("frq"));

        box->setCurrentIndex(box->findData(QStringLiteral("extra")));
        extra = {};
        QCOMPARE(box->findData(QStringLiteral("extra")), -1);
        QCOMPARE(box->currentData().toString(), QStringLiteral("mrq"));
    }

    // A voice bank that is opened and checked against the disk without an edit is not
    // modified, so that closing it asks nothing. The files of UTAU end their lines with CRLF,
    // which a text box shows as LF, and the other lines of character.txt may be one empty line,
    // which a text box shows as none. Neither counts as an edit of the panel.
    void an_unedited_voice_bank_is_not_modified_data() {
        QTest::addColumn<QByteArray>("character");
        QTest::newRow("lines") << QByteArray("name=Voice\r\nimage=icon.png\r\n\r\nvoice: a\r\n");
        QTest::newRow("an empty line") << QByteArray("name=Voice\r\n\r\n");
    }

    void an_unedited_voice_bank_is_not_modified() {
        QFETCH(QByteArray, character);
        QTemporaryDir dir;
        const auto bank = voiceBank(dir);
        // The line breaks are mixed, which a text box does not keep either.
        writeFile(bank / "readme.txt", "The first line.\r\nThe second line.\nThe third line.\r\n");
        writeFile(bank / "character.txt", character.constData());
        const auto e = editor();
        const auto window = e->openVoiceBank(bank);
        QVERIFY(window);
        const auto document = window->document();
        const int step = document->session()->currentStep();
        QVERIFY(!document->isModified());

        window->infoPanel()->commit();
        window->checkDisk();
        QCOMPARE(document->session()->currentStep(), step);
        QVERIFY(!document->isModified());

        // An edited readme keeps the CRLF of the file.
        window->infoPanel()->readmeEdit()->setPlainText(QStringLiteral("The first line.\nNew."));
        window->infoPanel()->commit();
        QCOMPARE(kit::VoiceBankRef(document->session()).readme(),
                 QStringLiteral("The first line.\r\nNew."));
    }

    // The information pane edits character.txt, readme.txt and prefix.map, each edit one step,
    // creating the files the voice bank lacks; the tree converts the encoding of a folder.
    void the_voice_bank_info_is_edited() {
        QTemporaryDir dir;
        const auto bank = voiceBank(dir);
        QImage icon(10, 10, QImage::Format_RGB32);
        icon.fill(Qt::red);
        QVERIFY(icon.save(QString::fromStdU16String((bank / "icon.png").u16string())));
        const auto e = editor();
        const auto window = e->openVoiceBank(bank);
        QVERIFY(window);
        const auto session = window->document()->session();
        const auto panel = window->infoPanel();
        QVERIFY(panel);
        QVERIFY(panel->nameEdit()->text().isEmpty());
        const int step = session->currentStep();

        panel->nameEdit()->setText(QStringLiteral("Name"));
        Q_EMIT panel->nameEdit()->editingFinished();
        QCOMPARE(session->currentStep(), step + 1);
        QCOMPARE(kit::VoiceBankRef(session).character().name(), QStringLiteral("Name"));
        panel->imageEdit()->setText(QStringLiteral("icon.png"));
        Q_EMIT panel->imageEdit()->editingFinished();
        QVERIFY(!panel->imagePreview()->pixmap().isNull());
        // Finishing again without a change is no step.
        Q_EMIT panel->imageEdit()->editingFinished();
        QCOMPARE(session->currentStep(), step + 2);

        panel->otherLinesEdit()->setPlainText(QStringLiteral("voice: a"));
        panel->readmeEdit()->setPlainText(QStringLiteral("Read me."));
        panel->commit();
        QCOMPARE(session->currentStep(), step + 4);
        QCOMPARE(kit::VoiceBankRef(session).character().extraLines(),
                 QStringList{QStringLiteral("voice: a")});
        QCOMPARE(kit::VoiceBankRef(session).readme(), QStringLiteral("Read me."));

        // A key of prefix.map, which the voice bank lacks, from its cell
        const auto table = panel->prefixTable();
        const int row = 60 - kit::VoicePrefix::minimumKey;
        QCOMPARE(table->item(row, 0)->text(), QStringLiteral("C4"));
        table->item(row, 2)->setText(QStringLiteral("_H"));
        QCOMPARE(session->currentStep(), step + 5);
        QCOMPARE(kit::VoiceBankRef(session).prefixMap().value(60).suffix, QStringLiteral("_H"));
        table->item(row + 1, 1)->setText(QStringLiteral("x"));
        QVERIFY(panel->removePrefix(61));
        QCOMPARE(kit::VoiceBankRef(session).prefixMap().keys(), QList<int>{60});

        // The panel follows an undo.
        session->undo();
        QCoreApplication::processEvents();
        QCOMPARE(table->item(row + 1, 1)->text(), QStringLiteral("x"));
        session->redo();
        QCoreApplication::processEvents();
        QCOMPARE(table->item(row + 1, 1)->text(), QString());

        QVERIFY(window->convertCharset({}, QStringLiteral("Shift_JIS")));
        QCOMPARE(kit::VoiceBankRef(session).directories().at(0).charset(),
                 QStringLiteral("Shift_JIS"));
        QCOMPARE(window->directoryTree()->topLevelItem(1)->toolTip(0),
                 QStringLiteral("Encoding: Shift_JIS"));
        QVERIFY(window->rereadCharset("sub", QStringLiteral("UTF-8")));

        // A text still being typed is saved with the rest.
        panel->readmeEdit()->setPlainText(QStringLiteral("Saved."));
        QVERIFY(window->save());
        std::ifstream readme(bank / "readme.txt", std::ios::binary);
        QCOMPARE(
            std::string((std::istreambuf_iterator<char>(readme)), std::istreambuf_iterator<char>()),
            std::string("Saved."));
        QVERIFY(fs::exists(bank / "character.txt"));
        QVERIFY(fs::exists(bank / "readme.txt"));
        QVERIFY(fs::exists(bank / "prefix.map"));
        std::ifstream in(bank / "character.txt", std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
        QVERIFY(text.find("name=Name") != std::string::npos);
        QVERIFY(text.find("image=icon.png") != std::string::npos);
    }

    // Saved in its window, a voice bank reaches the projects that sing it, without an edit of
    // them; unsaved edits do not.
    void a_saved_voice_bank_reaches_its_projects() {
        QTemporaryDir dir;
        const auto utau = pathIn(dir, "utau");
        fs::create_directories(utau / "voice");
        const auto bank = voiceBank(dir, "utau/voice/bank");
        const auto e = editor();
        e->settings().setUtauDirectory(utau);
        const auto window = e->newWindow();
        const auto session = window->document()->session();
        {
            auto transaction = session->transaction(QStringLiteral("voice"));
            kit::ProjectRef(session).tracks().at(0).setVoiceDir(QStringLiteral("%VOICE%bank"));
            QVERIFY(transaction.commit());
            kit::Note note;
            note.lyric = QStringLiteral("zzz");
            note.length = 480;
            note.noteNum = 60;
            kit::DiagnosticList diagnostics;
            QVERIFY(kit::ProjectEdits::insertNotes(kit::ProjectRef(session).tracks().at(0).notes(),
                                                   0, {note}, diagnostics));
        }
        QVERIFY(window->loadVoiceBank());
        const auto roll = qobject_cast<PianoRoll *>(window->centralWidget());
        QVERIFY(roll->lacksSample(0));
        const int step = session->currentStep();
        const bool modified = window->document()->isModified();

        const auto bankWindow = e->openVoiceBank(bank);
        QVERIFY(bankWindow);
        const auto bankSession = bankWindow->document()->session();
        {
            auto transaction = bankSession->transaction(QStringLiteral("rename"));
            kit::VoiceBankRef(bankSession)
                .directories()
                .at(0)
                .otoEntries()
                .at(0)
                .setAlias(QStringLiteral("zzz"));
            QVERIFY(transaction.commit());
        }
        QVERIFY(roll->lacksSample(0));

        QVERIFY(bankWindow->save());
        QVERIFY(!roll->lacksSample(0));
        QVERIFY(window->document()->voiceBank()->find(60, QStringLiteral("zzz")));
        QCOMPARE(session->currentStep(), step);
        QCOMPARE(window->document()->isModified(), modified);

        // Saved into another folder, it is another voice bank than that of the project.
        {
            auto transaction = bankSession->transaction(QStringLiteral("rename"));
            kit::VoiceBankRef(bankSession)
                .directories()
                .at(0)
                .otoEntries()
                .at(0)
                .setAlias(QStringLiteral("yyy"));
            QVERIFY(transaction.commit());
        }
        kit::DiagnosticList diagnostics;
        QVERIFY(bankWindow->document()->saveAs(pathIn(dir, "copy"),
                                               kit::VoiceBankSession::TextFiles, diagnostics));
        QVERIFY(window->document()->voiceBank()->find(60, QStringLiteral("zzz")));
        QVERIFY(!window->document()->voiceBank()->find(60, QStringLiteral("yyy")));
        e->settings().setUtauDirectory({});
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
        declaredActionOf(*e, window, QStringLiteral("helloutau.tools.editVoiceBank"))->trigger();
        QCOMPARE(e->voiceBankWindows().size(), 1);
        const auto bankWindow = e->voiceBankWindows().first();
        QVERIFY(isSame(bankWindow->document()->rootPath(), bank));

        // From a note to the entry it uses: x in sub, c without an entry in the root, y the
        // second entry of x.wav; none for zzz, and none for a rest.
        {
            const auto session = window->document()->session();
            QList<kit::Note> notes;
            for (const auto lyric : {"x", "c", "zzz", "R", "y"}) {
                kit::Note note;
                note.lyric = QString::fromLatin1(lyric);
                note.length = 480;
                note.noteNum = 60;
                notes.push_back(note);
            }
            kit::DiagnosticList diagnostics;
            QVERIFY(kit::ProjectEdits::insertNotes(kit::ProjectRef(session).tracks().at(0).notes(),
                                                   0, notes, diagnostics));
        }
        const auto roll = qobject_cast<PianoRoll *>(window->centralWidget());
        // With a selection, Edit Voice Bank goes to the entry of the first selected note.
        const auto go =
            declaredActionOf(*e, window, QStringLiteral("helloutau.tools.editVoiceBank"));
        QVERIFY(go);
        const auto current = [bankWindow](int column) {
            return bankWindow->entryTable()
                ->currentIndex()
                .siblingAtColumn(column)
                .data()
                .toString();
        };

        roll->setSelectedIndices({0});
        go->trigger();
        QCOMPARE(bankWindow->directoryTree()->currentItem()->text(0), QStringLiteral("sub"));
        QCOMPARE(current(VoiceBankEntryModel::FileColumn), QStringLiteral("x.wav"));
        QCOMPARE(current(VoiceBankEntryModel::AliasColumn), QStringLiteral("x"));

        bankWindow->searchBox()->setText(QStringLiteral("a"));
        roll->setSelectedIndices({1});
        go->trigger();
        QVERIFY(bankWindow->searchBox()->text().isEmpty());
        QCOMPARE(bankWindow->directoryTree()->currentItem()->text(0), QStringLiteral("bank"));
        QCOMPARE(current(VoiceBankEntryModel::FileColumn), QStringLiteral("c.wav"));

        roll->setSelectedIndices({2});
        go->trigger();
        QVERIFY(window->statusBar()->currentMessage().contains(QStringLiteral("zzz")));
        roll->setSelectedIndices({3});
        go->trigger();
        QCOMPARE(window->statusBar()->currentMessage(), QStringLiteral("A rest has no entry."));

        roll->setSelectedIndices({4});
        go->trigger();
        QCOMPARE(current(VoiceBankEntryModel::FileColumn), QStringLiteral("x.wav"));
        QCOMPARE(current(VoiceBankEntryModel::AliasColumn), QStringLiteral("y"));
        e->settings().setUtauDirectory({});
    }

    // Render Track proposes the project name as a single file name with the extension .wav, in
    // the music folder for a project without a file.
    void render_track_proposes_the_project_name_as_a_file_name() {
        const auto e = editor();
        const auto wavtool = pathIn(m_dir, "proposal-wavtool.exe");
        const auto resampler = pathIn(m_dir, "proposal-resampler.exe");
        for (const auto &tool : {wavtool, resampler}) {
            std::ofstream(tool, std::ios::binary) << "tool";
        }
        const auto wavtoolText = QString::fromStdU16String(wavtool.u16string());
        const auto resamplerText = QString::fromStdU16String(resampler.u16string());
        e->settings().setWavtool(wavtoolText);
        e->settings().setResampler(resamplerText);
        const auto window = e->newWindow();
        const auto session = window->document()->session();
        const auto renderTrack =
            declaredActionOf(*e, window, QStringLiteral("helloutau.playback.renderTrack"));
        QVERIFY(renderTrack);
        const auto music = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);

        // Returns the file name that the file dialog proposes for the project name.
        const auto proposalFor = [&](const QString &name) {
            auto tx = session->transaction(QStringLiteral("rename"));
            const auto settings = kit::ProjectRef(session).settings();
            settings.setName(name);
            settings.setWavtool(wavtoolText);
            settings.setResampler(resamplerText);
            tx.commit();
            QString proposal;
            QTimer::singleShot(0, [&proposal] {
                const auto dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
                if (dialog) {
                    const auto files = dialog->selectedFiles();
                    proposal = files.isEmpty() ? QString() : files.constFirst();
                    dialog->reject();
                }
            });
            renderTrack->trigger();
            return proposal;
        };
        const auto inMusic = [&music](const char *name) {
            return QDir(music).filePath(QLatin1String(name));
        };
        QCOMPARE(QDir::fromNativeSeparators(proposalFor(QStringLiteral("Song v1.2"))),
                 inMusic("Song v1.2.wav"));
        QCOMPARE(QDir::fromNativeSeparators(proposalFor(QStringLiteral("..\\x"))),
                 inMusic(".._x.wav"));
        QCOMPARE(QDir::fromNativeSeparators(proposalFor(QStringLiteral("a:b*c?"))),
                 inMusic("a_b_c_.wav"));
        QCOMPARE(QDir::fromNativeSeparators(proposalFor(QStringLiteral("take.wav"))),
                 inMusic("take.wav"));
        QCOMPARE(QDir::fromNativeSeparators(proposalFor(QStringLiteral(". ."))),
                 inMusic("track.wav"));
        e->settings().setWavtool({});
        e->settings().setResampler({});
    }

    // Playing in either mode and Render Track report missing project synth tools, and ask about
    // untrusted ones. Nothing is rendered unless both exist and are trusted.
    void rendering_requires_existing_and_trusted_synth_tools() {
        const auto e = editor();
        const auto window = e->openFile(savedProject(m_dir, "untrusted.usth"));
        QVERIFY(window);
        const auto roll = qobject_cast<PianoRoll *>(window->centralWidget());
        roll->selectAll();
        const auto play = declaredActionOf(*e, window, QStringLiteral("helloutau.playback.play"));
        const auto renderTrack =
            declaredActionOf(*e, window, QStringLiteral("helloutau.playback.renderTrack"));
        QVERIFY(play && renderTrack);
        const auto setSynthTools = [&](const QString &wavtool, const QString &resampler) {
            const auto session = window->document()->session();
            auto tx = session->transaction(QStringLiteral("synth tools"));
            const auto settings = kit::ProjectRef(session).settings();
            settings.setWavtool(wavtool);
            settings.setResampler(resampler);
            tx.commit();
        };
        const auto playInMode = [&](AppSettings::PlaybackMode mode) {
            e->settings().setPlaybackMode(mode);
            window->applySettings();
            return dialogsDuring([&] { play->trigger(); });
        };

        setSynthTools(QStringLiteral("missing-wavtool.exe"),
                      QStringLiteral("missing-resampler.exe"));
        QCOMPARE(playInMode(AppSettings::Prerender), QStringList{QStringLiteral("Message")});
        QCOMPARE(playInMode(AppSettings::Realtime), QStringList{QStringLiteral("Message")});
        QCOMPARE(dialogsDuring([&] { renderTrack->trigger(); }),
                 QStringList{QStringLiteral("Message")});

        const auto wavtool = pathIn(m_dir, "untrusted-wavtool.exe");
        const auto resampler = pathIn(m_dir, "untrusted-resampler.exe");
        for (const auto &tool : {wavtool, resampler}) {
            std::ofstream(tool, std::ios::binary) << "tool";
        }
        setSynthTools(QString::fromStdU16String(wavtool.u16string()),
                      QString::fromStdU16String(resampler.u16string()));
        QCOMPARE(playInMode(AppSettings::Prerender), QStringList{QStringLiteral("Question")});
        QCOMPARE(playInMode(AppSettings::Realtime), QStringList{QStringLiteral("Question")});
        QCOMPARE(dialogsDuring([&] { renderTrack->trigger(); }),
                 QStringList{QStringLiteral("Question")});
        e->settings().setPlaybackMode(AppSettings::Prerender);
    }

    // Typing a tempo into the tool bar changes the project once, on Return.
    void typing_a_tempo_in_the_tool_bar_is_one_step() {
        const auto e = editor();
        const auto window = e->openFile(savedProject(m_dir, "typed-tempo.usth"));
        QVERIFY(window);
        const auto box = window->findChild<QDoubleSpinBox *>(QStringLiteral("tempo"));
        QVERIFY(box);
        const auto session = window->document()->session();
        const auto steps = session->currentStep();

        box->selectAll();
        QTest::keyClicks(box, QStringLiteral("140"));
        QCOMPARE(session->currentStep(), steps);
        QTest::keyClick(box, Qt::Key_Return);
        QCOMPARE(session->currentStep(), steps + 1);
        QCOMPARE(session->snapshot().settings.tempo, 140.0);
    }

    // Without a voice bank, the pre-utterance and the overlap of a note are left to the voice
    // bank rather than shown as 0.
    void note_properties_without_samples_leave_the_defaults_to_the_voice_bank() {
        const auto e = editor();
        const auto window = e->openFile(savedProject(m_dir, "unsampled.usth"));
        QVERIFY(window);
        qobject_cast<PianoRoll *>(window->centralWidget())->selectAll();
        const auto properties =
            declaredActionOf(*e, window, QStringLiteral("helloutau.edit.noteProperties"));
        QVERIFY(properties);
        QString preUtterance;
        QString overlap;
        QTimer::singleShot(0, [&] {
            const auto dialog =
                qobject_cast<NotePropertiesDialog *>(QApplication::activeModalWidget());
            if (!dialog) {
                return;
            }
            preUtterance = dialog->field(NotePropertiesDialog::PreUtterance)->placeholderText();
            overlap = dialog->field(NotePropertiesDialog::VoiceOverlap)->placeholderText();
            dialog->reject();
        });
        properties->trigger();
        QCOMPARE(preUtterance, QStringLiteral("(default: voice bank)"));
        QCOMPARE(overlap, QStringLiteral("(default: voice bank)"));
    }

    // Pitch Control is available only in Mode2. Accepting a change of the portamento and the
    // vibrato is one undo step, and undoing it restores both.
    void pitch_control_is_one_step_in_mode2() {
        const auto e = editor();
        const auto window = e->openFile(savedProject(m_dir, "mode2.usth"));
        QVERIFY(window);
        qobject_cast<PianoRoll *>(window->centralWidget())->selectAll();
        const auto edit =
            declaredActionOf(*e, window, QStringLiteral("helloutau.edit.pitchControl"));
        QVERIFY(edit);
        const auto session = window->document()->session();
        const auto setMode2 = [session](bool mode2) {
            auto tx = session->transaction(QStringLiteral("mode"));
            kit::ProjectRef(session).settings().setMode2(mode2);
            tx.commit();
        };
        setMode2(false);
        QVERIFY(!edit->isEnabled());
        setMode2(true);
        QVERIFY(edit->isEnabled());

        const auto before = session->snapshot().tracks[0].notes[0];
        QVERIFY(!before.vibrato);
        QVERIFY(before.portamento.isEmpty());
        const auto steps = session->currentStep();
        bool opened = false;
        QTimer::singleShot(0, [&opened] {
            const auto dialog =
                qobject_cast<PitchControlDialog *>(QApplication::activeModalWidget());
            if (!dialog) {
                return;
            }
            opened = true;
            for (const auto box : dialog->findChildren<QCheckBox *>()) {
                if (box->text() == QStringLiteral("&Vibrato") ||
                    box->text() == QStringLiteral("&Portamento")) {
                    box->setChecked(true);
                }
            }
            dialog->accept();
        });
        edit->trigger();
        QVERIFY(opened);
        QCOMPARE(session->currentStep(), steps + 1);
        const auto after = session->snapshot().tracks[0].notes[0];
        QVERIFY(after.vibrato);
        QVERIFY(!after.portamento.isEmpty());

        session->undo();
        const auto undone = session->snapshot().tracks[0].notes[0];
        QVERIFY(!undone.vibrato);
        QVERIFY(undone.portamento.isEmpty());
    }

    // A region to edit opens the rename dialog with its name. A new name renames the region, and
    // an empty name removes only that region.
    void a_region_is_renamed_or_removed_by_its_name() {
        const auto e = editor();
        const auto window = e->openFile(savedRegions(m_dir, "renamed-regions.usth"));
        QVERIFY(window);
        const auto roll = window->pianoRoll();
        // Requests to edit region, answers the rename dialog with name, and closes any message box
        // that follows. Returns the name that the dialog showed.
        const auto rename = [roll](const kit::Region &region, const QString &name) {
            QString shown;
            QTimer timer;
            timer.setInterval(10);
            QObject::connect(&timer, &QTimer::timeout, [&shown, &name] {
                const auto widget = QApplication::activeModalWidget();
                if (const auto dialog = qobject_cast<QInputDialog *>(widget)) {
                    shown = dialog->textValue();
                    dialog->setTextValue(name);
                    dialog->accept();
                } else if (const auto box = qobject_cast<QMessageBox *>(widget)) {
                    box->reject();
                }
            });
            timer.start();
            Q_EMIT roll->regionEditRequested(region);
            return shown;
        };
        QCOMPARE(rename(kit::Region{QStringLiteral("B"), 1, 2}, QStringLiteral("C")),
                 QStringLiteral("B"));
        QCOMPARE(regionNamesOf(roll), (QStringList{QStringLiteral("A"), QStringLiteral("C")}));

        QCOMPARE(rename(kit::Region{QStringLiteral("C"), 1, 2}, QString()), QStringLiteral("C"));
        QCOMPARE(regionNamesOf(roll), QStringList{QStringLiteral("A")});
    }

    // The regions dialog selects the region given. Remove removes that region in one undo step
    // and updates the list, and Go To selects the notes of the current region.
    void the_regions_are_edited_in_their_dialog() {
        const auto e = editor();
        const auto window = e->openFile(savedRegions(m_dir, "listed-regions.usth"));
        QVERIFY(window);
        const auto roll = window->pianoRoll();
        const auto session = window->document()->session();
        const auto steps = session->currentStep();
        QString current;
        int listedAfterRemoval = -1;
        // A message box about a failed removal would otherwise block the click of Remove.
        QTimer boxes;
        boxes.setInterval(10);
        QObject::connect(&boxes, &QTimer::timeout, [] {
            if (const auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                box->reject();
            }
        });
        boxes.start();
        QTimer::singleShot(0, [&] {
            const auto dialog = qobject_cast<RegionDialog *>(QApplication::activeModalWidget());
            if (!dialog) {
                return;
            }
            current = dialog->currentRegion() ? dialog->currentRegion()->name : QString();
            dialog->removeButton()->click();
            listedAfterRemoval = int(dialog->regions().size());
            dialog->setCurrentRegion(kit::Region{QStringLiteral("A"), 0, 3});
            dialog->goToButton()->click();
            dialog->reject();
        });
        Q_EMIT roll->regionsRequested(kit::Region{QStringLiteral("B"), 1, 2});
        QCOMPARE(current, QStringLiteral("B"));
        QCOMPARE(listedAfterRemoval, 1);
        QCOMPARE(regionNamesOf(roll), QStringList{QStringLiteral("A")});
        QCOMPARE(session->currentStep(), steps + 1);
        QCOMPARE(roll->selectedIndices(), (QList<int>{0, 1, 2, 3}));
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
