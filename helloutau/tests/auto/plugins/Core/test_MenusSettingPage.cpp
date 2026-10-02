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
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QMenu>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeView>

#include <QAKCore/actionregistry.h>

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
        QVERIFY(page.widget());
        QCOMPARE(page.currentKind(), Editor::ProjectWindowKind);
        const auto tree = page.tree(Editor::ProjectWindowKind);
        QVERIFY(tree);
        const auto model = tree->model();
        QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("Main Menu"));
        QCOMPARE(model->index(1, 0).data().toString(), QStringLiteral("Main Toolbar"));
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
            const auto tree = dialog->findChild<QTreeView *>(QStringLiteral("actions"));
            QVERIFY(tree);
            std::function<QModelIndex(const QModelIndex &)> find = [&](const QModelIndex &parent) {
                for (int row = 0; row < tree->model()->rowCount(parent); ++row) {
                    const auto index = tree->model()->index(row, 0, parent);
                    if (index.data(Qt::UserRole).toString() ==
                        QStringLiteral("helloutau.playback.renderTrack")) {
                        return index;
                    }
                    if (const auto child = find(index); child.isValid()) {
                        return child;
                    }
                }
                return QModelIndex();
            };
            const auto item = find({});
            if (!item.isValid()) {
                qobject_cast<QDialog *>(dialog)->reject();
                return;
            }
            tree->setCurrentIndex(item);
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

    // Reapplying an adjacent File menu reorder must remain safe when the settings page is opened
    // again. This is the sequence that previously crashed in the settings dialog.
    void adjacent_file_actions_can_be_reordered_twice() {
        QTemporaryDir dir;
        const auto settingsFile = dir.filePath(QStringLiteral("settings.json"));
        const auto e = std::make_unique<Editor>(std::make_unique<AppSettings>(settingsFile));
        e->setWatchesDisk(false);
        e->newWindow();
        const auto move = [](MenusSettingPage &page, const QString &entry, bool up) {
            const auto tree = page.tree(Editor::ProjectWindowKind);
            const auto model = tree->model();
            QModelIndex file;
            for (int row = 0; row < model->rowCount(model->index(0, 0)); ++row) {
                const auto index = model->index(row, 0, model->index(0, 0));
                if (index.data().toString() == QStringLiteral("File")) {
                    file = index;
                    break;
                }
            }
            QVERIFY(file.isValid());
            QModelIndex item;
            for (int row = 0; row < model->rowCount(file); ++row) {
                const auto index = model->index(row, 0, file);
                if (index.data().toString() == entry) {
                    item = index;
                    break;
                }
            }
            QVERIFY(item.isValid());
            tree->setCurrentIndex(item);
            page.widget()->findChild<QPushButton *>(up ? "moveUp" : "moveDown")->click();
        };
        QString error;
        {
            MenusSettingPage page(e.get());
            QVERIFY(page.widget());
            move(page, QStringLiteral("New"), false);
            QVERIFY(page.apply(&error));
        }
        {
            MenusSettingPage page(e.get());
            QVERIFY(page.widget());
            move(page, QStringLiteral("Open..."), true);
            QVERIFY(page.apply(&error));
        }
    }

    // Each kind of window has a tab of its own, whose edits change the layouts of its registry
    // alone, and actionLayouts.json has a section for each kind.
    void each_kind_of_window_has_its_own_layouts() {
        QTemporaryDir dir;
        const auto settingsFile = dir.filePath(QStringLiteral("settings.json"));
        const auto e = std::make_unique<Editor>(std::make_unique<AppSettings>(settingsFile));
        e->setWatchesDisk(false);
        MenusSettingPage page(e.get());
        QVERIFY(page.widget());
        page.setCurrentKind(Editor::VoiceBankWindowKind);
        QCOMPARE(page.currentKind(), Editor::VoiceBankWindowKind);
        const auto tree = page.tree(Editor::VoiceBankWindowKind);
        const auto model = tree->model();
        QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("Main Menu"));
        QCOMPARE(model->index(1, 0).data().toString(), QStringLiteral("Sample Toolbar"));
        QModelIndex tools;
        for (int row = 0; row < model->rowCount(model->index(0, 0)); ++row) {
            const auto index = model->index(row, 0, model->index(0, 0));
            if (index.data().toString() == QStringLiteral("Tools")) {
                tools = index;
            }
        }
        QVERIFY(tools.isValid());
        QCOMPARE(model->index(0, 0, tools).data().toString(),
                 QStringLiteral("Remove Audio Metadata..."));
        tree->setCurrentIndex(model->index(0, 0, tools));
        page.removeCurrent();
        QString error;
        QVERIFY(page.apply(&error));

        const auto toolsOf = [&e](Editor::WindowKind kind, const char *menu) {
            QStringList ids;
            for (const auto &entry :
                 e->actionRegistry(kind)->layouts().adjacencyMap().value(QLatin1String(menu))) {
                ids.push_back(entry.id());
            }
            return ids;
        };
        QVERIFY(!toolsOf(Editor::VoiceBankWindowKind, "helloutau.voiceBank.tools")
                     .contains(QStringLiteral("helloutau.voiceBank.removeMetadata")));
        QVERIFY(toolsOf(Editor::ProjectWindowKind, "helloutau.tools")
                    .contains(QStringLiteral("helloutau.tools.clearCache")));
        QVERIFY(e->actionRegistry(Editor::ProjectWindowKind)->layoutChanges().isEmpty());

        QFile file(dir.filePath(QStringLiteral("actionLayouts.json")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto object = QJsonDocument::fromJson(file.readAll()).object();
        QVERIFY(object.value(QStringLiteral("projectWindow"))
                    .toObject()
                    .value(QStringLiteral("changes"))
                    .toArray()
                    .isEmpty());
        QVERIFY(!object.value(QStringLiteral("voiceBankWindow"))
                     .toObject()
                     .value(QStringLiteral("changes"))
                     .toArray()
                     .isEmpty());
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
