#include <memory>

#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QKeySequenceEdit>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTreeWidgetItemIterator>

#include <QAKCore/actionregistry.h>

#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/BuiltinActions.h>
#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/ProjectWindow.h>

#include <Core/KeymapSettingPage.h>

#include <helloutau/Testing/Editor/TestingEditor.h>

using namespace hello::daw;

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
        const auto tabs = page.widget()->findChild<QTabWidget *>(QStringLiteral("windows"));
        QVERIFY(tabs);
        const auto tree = qobject_cast<QTreeWidget *>(tabs->widget(0));
        QVERIFY(tree);
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
        const auto horizontalScroll =
            page.widget()->findChild<QComboBox *>(QStringLiteral("modifier_0"));
        QVERIFY(horizontalScroll);
        horizontalScroll->setCurrentIndex(2);
        QVERIFY(page.isModified());
        QVERIFY(page.apply(&error));
        QCOMPARE(e->modifierBindings().horizontalScroll, Qt::AltModifier);
        QCOMPARE(
            declaredActionOf(*e, window, QStringLiteral("helloutau.edit.mergeNotes"))->shortcuts(),
            (QList<QKeySequence>{QKeySequence(Qt::CTRL | Qt::Key_U),
                                 QKeySequence(Qt::Key_Insert)}));
        QVERIFY(declaredActionOf(*e, window, QStringLiteral("helloutau.edit.insertNote"))
                    ->shortcuts()
                    .isEmpty());
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("keymap.json"))));

        // A new editor reads the file.
        e.reset();
        const auto again = std::make_unique<Editor>(std::make_unique<AppSettings>(settingsFile));
        again->setWatchesDisk(false);
        const auto other = again->newWindow();
        QVERIFY(declaredActionOf(*again, other, QStringLiteral("helloutau.edit.insertNote"))
                    ->shortcuts()
                    .isEmpty());
        QCOMPARE(again->modifierBindings().horizontalScroll, Qt::AltModifier);

        // Restoring the defaults gives Insert Note its key again.
        KeymapSettingPage keymap(again.get());
        keymap.widget()->findChild<QPushButton *>(QStringLiteral("resetAll"))->click();
        QVERIFY(keymap.apply(&error));
        QCOMPARE(declaredActionOf(*again, other, QStringLiteral("helloutau.edit.insertNote"))
                     ->shortcuts(),
                 QList<QKeySequence>{QKeySequence(Qt::Key_Insert)});
    }

    // Each window kind has its own tab and an Other group for commands in no menu. A command of
    // both kinds, Undo, has shortcuts in each apart from the other, a shortcut conflicts within
    // one kind alone, and keymap.json has a section for each kind.
    void each_kind_of_window_has_its_own_keymap() {
        using Command = KeymapSettingPage::Command;
        QTemporaryDir dir;
        const auto settingsFile = dir.filePath(QStringLiteral("settings.json"));
        const auto e = std::make_unique<Editor>(std::make_unique<AppSettings>(settingsFile));
        e->setWatchesDisk(false);
        KeymapSettingPage page(e.get());
        const auto tabs = page.widget()->findChild<QTabWidget *>(QStringLiteral("windows"));
        QVERIFY(tabs);
        for (int i = 0; i < 2; ++i) {
            const auto tree = qobject_cast<QTreeWidget *>(tabs->widget(i));
            QVERIFY(tree);
            const auto other = tree->topLevelItem(tree->topLevelItemCount() - 1);
            QCOMPARE(other->text(0), QStringLiteral("Other"));
            QStringList ids;
            for (int j = 0; j < other->childCount(); ++j) {
                ids.push_back(other->child(j)->data(0, Qt::UserRole).toString());
            }
            QVERIFY(ids.contains(QStringLiteral("helloutau.file.openRecentProject")));
        }

        const Command insertEntry{Editor::VoiceBankWindowKind,
                                  QStringLiteral("helloutau.voiceBank.insertEntry")};
        const Command mergeNotes{Editor::ProjectWindowKind,
                                 QStringLiteral("helloutau.edit.mergeNotes")};
        QVERIFY(isDeclared(*e, insertEntry.kind, insertEntry.id));
        QVERIFY(page.conflicts(insertEntry, QKeySequence(Qt::Key_Insert)).isEmpty());
        QCOMPARE(page.conflicts(insertEntry, QKeySequence(Qt::CTRL | Qt::Key_D)),
                 (QList<Command>{
                     {Editor::VoiceBankWindowKind,
                      QStringLiteral("helloutau.voiceBank.duplicateEntries")}
        }));
        QCOMPARE(page.conflicts(mergeNotes, QKeySequence(Qt::Key_Insert)),
                 (QList<Command>{
                     {Editor::ProjectWindowKind, QStringLiteral("helloutau.edit.insertNote")}
        }));

        const auto undo = QStringLiteral("helloutau.edit.undo");
        const QKeySequence key(Qt::CTRL | Qt::ALT | Qt::Key_Z);
        page.addShortcut({Editor::ProjectWindowKind, undo}, key);
        QString error;
        QVERIFY(page.apply(&error));
        QCOMPARE(e->actionRegistry(Editor::ProjectWindowKind)->actionShortcuts(undo),
                 (QList<QKeySequence>{QKeySequence(Qt::CTRL | Qt::Key_Z), key}));
        QCOMPARE(e->actionRegistry(Editor::VoiceBankWindowKind)->actionShortcuts(undo),
                 QList<QKeySequence>{QKeySequence(Qt::CTRL | Qt::Key_Z)});

        QFile file(dir.filePath(QStringLiteral("keymap.json")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto object = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(object.value(QStringLiteral("projectWindow"))
                     .toObject()
                     .value(QStringLiteral("shortcuts"))
                     .toArray()
                     .size(),
                 1);
        QVERIFY(object.value(QStringLiteral("voiceBankWindow"))
                    .toObject()
                    .value(QStringLiteral("shortcuts"))
                    .toArray()
                    .isEmpty());
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
