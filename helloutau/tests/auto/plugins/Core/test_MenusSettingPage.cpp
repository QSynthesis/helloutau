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
#include <QtWidgets/QMenu>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeView>

#include <QAKCore/actionextension.h>
#include <QAKCore/actionregistry.h>

#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/BuiltinActions.h>
#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/ProjectWindow.h>

#include <Core/MenusSettingPage.h>

#include <helloutau/Testing/Editor/TestingEditor.h>

using namespace hello::daw;

namespace {

    // Returns the ids of the actions of the Tools menu of window.
    QStringList toolsOf(const Editor &editor, ProjectWindow *window) {
        const auto menu = declaredMenuOf(editor, window, QStringLiteral("helloutau.tools"));
        return menu ? actionIdsIn(editor, window, menu) : QStringList();
    }

    // Returns the child of parent in model whose layout entry has the id, or an invalid index.
    QModelIndex childWithId(const QAbstractItemModel *model, const QModelIndex &parent,
                            const QString &id) {
        for (int row = 0; row < model->rowCount(parent); ++row) {
            const auto index = model->index(row, 0, parent);
            if (index.data(Qt::UserRole).value<QAK::ActionLayoutEntry>().id() == id) {
                return index;
            }
        }
        return {};
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
        const auto mainMenu = childWithId(model, {}, QStringLiteral("helloutau.mainMenu"));
        QVERIFY(mainMenu.isValid());
        QVERIFY(childWithId(model, {}, QStringLiteral("helloutau.mainToolBar")).isValid());
        const auto button = [&page](const char *name) {
            return page.widget()->findChild<QPushButton *>(QLatin1String(name));
        };
        const auto tools = [&] {
            return childWithId(model, mainMenu, QStringLiteral("helloutau.tools"));
        };
        QVERIFY(tools().isValid());
        const auto editVoiceBank = QStringLiteral("helloutau.tools.editVoiceBank");
        const auto clearCache = QStringLiteral("helloutau.tools.clearCache");
        const auto renderTrack = QStringLiteral("helloutau.playback.renderTrack");
        QVERIFY(isDeclared(*e, Editor::ProjectWindowKind, clearCache));
        QVERIFY(isDeclared(*e, Editor::ProjectWindowKind, renderTrack));

        tree->setCurrentIndex(childWithId(model, tools(), editVoiceBank));
        QVERIFY(!button("moveUp")->isEnabled());
        button("moveDown")->click();
        QCOMPARE(childWithId(model, tools(), editVoiceBank).row(), 1);
        tree->setCurrentIndex(childWithId(model, tools(), clearCache));
        button("remove")->click();
        QVERIFY(!childWithId(model, tools(), clearCache).isValid());

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
                QFAIL("The dialog does not offer helloutau.playback.renderTrack.");
            }
            tree->setCurrentIndex(item);
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
        button("add")->click();
        QVERIFY(childWithId(model, tools(), renderTrack).isValid());
        QVERIFY(page.isModified());

        QString error;
        QVERIFY(page.apply(&error));
        QVERIFY(!page.isModified());
        auto menu = toolsOf(*e, window);
        QVERIFY(!menu.contains(clearCache));
        QVERIFY(menu.contains(renderTrack));
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("actionLayouts.json"))));

        // A new editor reads the file.
        e.reset();
        const auto again = std::make_unique<Editor>(std::make_unique<AppSettings>(settingsFile));
        again->setWatchesDisk(false);
        const auto other = again->newWindow();
        menu = toolsOf(*again, other);
        QVERIFY(!menu.contains(clearCache));
        QVERIFY(menu.contains(renderTrack));

        MenusSettingPage layouts(again.get());
        layouts.widget()->findChild<QPushButton *>(QStringLiteral("restoreDefaults"))->click();
        QVERIFY(layouts.apply(&error));
        menu = toolsOf(*again, other);
        QVERIFY(menu.contains(clearCache));
        QVERIFY(!menu.contains(renderTrack));
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
            const auto file =
                childWithId(model, childWithId(model, {}, QStringLiteral("helloutau.mainMenu")),
                            QStringLiteral("helloutau.file"));
            QVERIFY(file.isValid());
            const auto item = childWithId(model, file, entry);
            QVERIFY(item.isValid());
            tree->setCurrentIndex(item);
            page.widget()->findChild<QPushButton *>(up ? "moveUp" : "moveDown")->click();
        };
        QString error;
        {
            MenusSettingPage page(e.get());
            QVERIFY(page.widget());
            move(page, QStringLiteral("helloutau.file.new"), false);
            QVERIFY(page.apply(&error));
        }
        {
            MenusSettingPage page(e.get());
            QVERIFY(page.widget());
            move(page, QStringLiteral("helloutau.file.open"), true);
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
        const auto mainMenu = childWithId(model, {}, QStringLiteral("helloutau.voiceBankMenu"));
        QVERIFY(mainMenu.isValid());
        QVERIFY(
            childWithId(model, {}, QStringLiteral("helloutau.voiceBank.sampleToolBar")).isValid());
        const auto tools =
            childWithId(model, mainMenu, QStringLiteral("helloutau.voiceBank.tools"));
        QVERIFY(tools.isValid());
        const auto removeMetadata = QStringLiteral("helloutau.voiceBank.removeMetadata");
        QVERIFY(isDeclared(*e, Editor::VoiceBankWindowKind, removeMetadata));
        const auto entry = childWithId(model, tools, removeMetadata);
        QVERIFY(entry.isValid());
        tree->setCurrentIndex(entry);
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
                     .contains(removeMetadata));
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
