#include <filesystem>
#include <fstream>

#include <QtCore/QTemporaryDir>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

#include <hellokit/Edit/ProjectDocument.h>
#include <hellokit/Edit/ProjectRefs.h>

#include <helloutau/Widgets/CommandPalette.h>

#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/MainWindow.h>
#include <helloutau/Editor/PianoRoll.h>
#include <helloutau/Editor/VoiceBankCharsetDialog.h>

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
                                     QStringLiteral("&View"), QStringLiteral("&Tools")}));
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
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_Editor test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_Editor.moc"
