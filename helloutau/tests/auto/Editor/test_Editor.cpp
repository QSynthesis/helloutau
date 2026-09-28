#include <filesystem>

#include <QtCore/QTemporaryDir>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

#include <hellokit/Edit/ProjectDocument.h>
#include <hellokit/Edit/ProjectRefs.h>

#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/MainWindow.h>

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
                                     QStringLiteral("&Tools")}));
        const auto save = actionNamed(window, QStringLiteral("&Save"));
        QVERIFY(save);
        QCOMPARE(save->shortcut(), QKeySequence(QStringLiteral("Ctrl+S")));
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
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_Editor test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_Editor.moc"
