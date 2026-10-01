#include <memory>

#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMenu>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeView>

#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/BuiltinActions.h>
#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/ProjectWindow.h>

#include <Core/MenusSettingPage.h>

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

    // The texts of the Tools menu of window
    QStringList toolsOf(QWidget *window) {
        QStringList list;
        for (const auto action : actionNamed(window, QStringLiteral("&Tools"))->menu()->actions()) {
            list.push_back(action->text());
        }
        return list;
    }

}

class test_MenusSettingPage : public QObject {
    Q_OBJECT

private:
    BuiltinActions m_actions;

private Q_SLOTS:
    // The page edits the menus of the windows: an entry moved down, an entry removed, and an
    // action added to a menu. It applies the layouts to every window and writes
    // actionLayouts.json beside the settings, which a new editor reads. Restore Defaults
    // restores them.
    void the_menus_are_edited() {
        QTemporaryDir dir;
        const auto settingsFile = dir.filePath(QStringLiteral("settings.json"));
        auto e = std::make_unique<Editor>(std::make_unique<AppSettings>(settingsFile));
        e->setWatchesDisk(false);
        const auto window = e->newWindow();
        MenusSettingPage page(e.get());
        QCOMPARE(page.id(), QStringLiteral("core.MenusAndToolbars"));
        const auto tree = page.widget()->findChild<QTreeView *>(QStringLiteral("layouts"));
        QVERIFY(tree);
        const auto model = tree->model();
        QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("Project Window: Main Menu"));
        const auto childNamed = [model](const QModelIndex &parent, const QString &text) {
            for (int row = 0; row < model->rowCount(parent); ++row) {
                if (model->index(row, 0, parent).data().toString() == text) {
                    return model->index(row, 0, parent);
                }
            }
            return QModelIndex();
        };
        const auto button = [&page](const char *name) {
            return page.widget()->findChild<QPushButton *>(QLatin1String(name));
        };
        const auto tools = [&] { return childNamed(model->index(0, 0), QStringLiteral("Tools")); };
        QVERIFY(tools().isValid());

        tree->setCurrentIndex(childNamed(tools(), QStringLiteral("Edit Voice Bank")));
        QVERIFY(!button("moveUp")->isEnabled());
        button("moveDown")->click();
        QCOMPARE(model->index(1, 0, tools()).data().toString(), QStringLiteral("Edit Voice Bank"));
        tree->setCurrentIndex(childNamed(tools(), QStringLiteral("Clear Render Cache")));
        button("remove")->click();
        QVERIFY(!childNamed(tools(), QStringLiteral("Clear Render Cache")).isValid());

        tree->setCurrentIndex(tools());
        QTimer::singleShot(0, [] {
            const auto dialog = QApplication::activeModalWidget();
            QVERIFY(dialog);
            const auto list = dialog->findChild<QListWidget *>(QStringLiteral("actions"));
            for (int i = 0; i < list->count(); ++i) {
                if (list->item(i)->data(Qt::UserRole).toString() ==
                    QStringLiteral("helloutau.playback.renderTrack")) {
                    list->setCurrentRow(i);
                }
            }
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
        button("add")->click();
        QVERIFY(childNamed(tools(), QStringLiteral("Render Track to WAV...")).isValid());
        QVERIFY(page.isModified());

        QString error;
        QVERIFY(page.apply(&error));
        QVERIFY(!page.isModified());
        auto menu = toolsOf(window);
        QVERIFY(!menu.contains(QStringLiteral("&Clear Render Cache")));
        QVERIFY(menu.contains(QStringLiteral("Render &Track to WAV...")));
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("actionLayouts.json"))));

        // A new editor reads the file.
        e.reset();
        const auto again = std::make_unique<Editor>(std::make_unique<AppSettings>(settingsFile));
        again->setWatchesDisk(false);
        const auto other = again->newWindow();
        menu = toolsOf(other);
        QVERIFY(!menu.contains(QStringLiteral("&Clear Render Cache")));
        QVERIFY(menu.contains(QStringLiteral("Render &Track to WAV...")));

        MenusSettingPage layouts(again.get());
        layouts.widget()->findChild<QPushButton *>(QStringLiteral("restoreDefaults"))->click();
        QVERIFY(layouts.apply(&error));
        menu = toolsOf(other);
        QVERIFY(menu.contains(QStringLiteral("&Clear Render Cache")));
        QVERIFY(!menu.contains(QStringLiteral("Render &Track to WAV...")));
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_MenusSettingPage test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_MenusSettingPage.moc"
