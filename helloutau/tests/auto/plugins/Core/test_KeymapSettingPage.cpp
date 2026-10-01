#include <memory>

#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QKeySequenceEdit>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTreeWidgetItemIterator>

#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/BuiltinActions.h>
#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/ProjectWindow.h>

#include <Core/KeymapSettingPage.h>

using namespace hello::daw;

namespace {

    QAction *actionNamed(QWidget *window, const QString &text) {
        for (const auto action : window->findChildren<QAction *>()) {
            if (action->text() == text) {
                return action;
            }
        }
        return nullptr;
    }

}

class test_KeymapSettingPage : public QObject {
    Q_OBJECT

private:
    BuiltinActions m_actions;

private Q_SLOTS:
    // The keymap lists the commands of each window under their menus. A shortcut conflicts with
    // a command of the same window only, and the user may remove it from that command. The page
    // applies the shortcuts to every window and writes keymap.json beside the settings, which a
    // new editor reads.
    void the_keymap_assigns_shortcuts() {
        QTemporaryDir dir;
        const auto settingsFile = dir.filePath(QStringLiteral("settings.json"));
        auto e = std::make_unique<Editor>(std::make_unique<AppSettings>(settingsFile));
        e->setWatchesDisk(false);
        const auto window = e->newWindow();
        KeymapSettingPage page(e.get());
        QCOMPARE(page.id(), QStringLiteral("core.Keymap"));
        const auto tree = page.widget()->findChild<QTreeWidget *>(QStringLiteral("commands"));
        QVERIFY(tree);
        QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("Project Window"));
        QCOMPARE(tree->topLevelItem(1)->text(0), QStringLiteral("Voice Bank Window"));
        const auto itemOf = [tree](const QString &id) -> QTreeWidgetItem * {
            for (QTreeWidgetItemIterator it(tree); *it; ++it) {
                if ((*it)->data(0, Qt::UserRole).toString() == id) {
                    return *it;
                }
            }
            return nullptr;
        };
        const auto merge = itemOf(QStringLiteral("helloutau.edit.mergeNotes"));
        QVERIFY(merge);
        QCOMPARE(merge->parent()->text(0), QStringLiteral("Edit"));

        // Ins conflicts with Insert Note of the project window, and not with Insert Entry of the
        // voice bank window. The conflict is removed.
        tree->setCurrentItem(merge);
        QString conflicts;
        QTimer::singleShot(0, [&conflicts] {
            const auto dialog = QApplication::activeModalWidget();
            QVERIFY(dialog);
            const auto edit = dialog->findChild<QKeySequenceEdit *>(QStringLiteral("shortcut"));
            edit->setKeySequence(QKeySequence(Qt::Key_Insert));
            Q_EMIT edit->keySequenceChanged(edit->keySequence());
            conflicts = dialog->findChild<QLabel *>(QStringLiteral("conflicts"))->text();
            QTimer::singleShot(0, [] {
                const auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                QVERIFY(box);
                for (const auto button : box->buttons()) {
                    if (box->buttonRole(button) == QMessageBox::AcceptRole) {
                        button->click();
                    }
                }
            });
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
        page.widget()->findChild<QPushButton *>(QStringLiteral("add"))->click();
        QVERIFY(conflicts.contains(QStringLiteral("Insert Note")));
        QVERIFY(!conflicts.contains(QStringLiteral("Insert Entry")));
        QVERIFY(merge->text(1).contains(
            QKeySequence(Qt::Key_Insert).toString(QKeySequence::NativeText)));
        QVERIFY(itemOf(QStringLiteral("helloutau.edit.insertNote"))->text(1).isEmpty());
        QVERIFY(page.isModified());

        QString error;
        QVERIFY(page.apply(&error));
        QVERIFY(!page.isModified());
        QCOMPARE(actionNamed(window, QStringLiteral("Mer&ge Notes"))->shortcuts(),
                 (QList<QKeySequence>{QKeySequence(Qt::CTRL | Qt::Key_U),
                                      QKeySequence(Qt::Key_Insert)}));
        QVERIFY(actionNamed(window, QStringLiteral("&Insert Note"))->shortcuts().isEmpty());
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("keymap.json"))));

        // A new editor reads the file.
        e.reset();
        const auto again = std::make_unique<Editor>(std::make_unique<AppSettings>(settingsFile));
        again->setWatchesDisk(false);
        const auto other = again->newWindow();
        QVERIFY(actionNamed(other, QStringLiteral("&Insert Note"))->shortcuts().isEmpty());

        // Restoring the defaults gives Insert Note its key again.
        KeymapSettingPage keymap(again.get());
        keymap.widget()->findChild<QPushButton *>(QStringLiteral("resetAll"))->click();
        QVERIFY(keymap.apply(&error));
        QCOMPARE(actionNamed(other, QStringLiteral("&Insert Note"))->shortcuts(),
                 QList<QKeySequence>{QKeySequence(Qt::Key_Insert)});
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_KeymapSettingPage test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_KeymapSettingPage.moc"
