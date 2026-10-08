#include <memory>

#include <QtCore/QStandardPaths>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <helloutau/Editor/AppLoader.h>
#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/BuiltinActions.h>
#include <helloutau/Editor/Editor.h>
#include <helloutau/Widgets/SettingPage.h>

#include <Core/CoreSettingPages.h>

using namespace hello::daw;

class test_CoreSettingPages : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The pages of the core plugin take their places among those of the editor, in the order of
    // the settings of JetBrains IDEs.
    void the_pages_follow_the_order_of_jetbrains_ides() {
        QTemporaryDir dir;
        Editor e(std::make_unique<AppSettings>(dir.filePath(QStringLiteral("settings.json"))));
        new BuiltinActions(&e);
        addCoreSettingPages(&e);
        const auto idsOf = [](const QList<SettingPage *> &pages) {
            QStringList ids;
            for (const auto page : pages) {
                ids.push_back(page->id());
            }
            return ids;
        };
        const auto catalog = e.settingCatalog();
        QCOMPARE(idsOf(catalog->pages()),
                 (QStringList{"editor.AppearanceAndBehavior", "core.Keymap", "editor.Editor",
                              "editor.Audio", "editor.Rendering"}));
        QCOMPARE(idsOf(catalog->page(QStringLiteral("editor.AppearanceAndBehavior"))->pages()),
                 (QStringList{"core.MenusAndToolbars", "editor.SystemSettings"}));

        // With a loader, Plugins precedes Rendering.
        AppLoader loader(
            {QStringLiteral("helloutau"), QLatin1String(AppLoader::settingsOption), dir.path()});
        Editor other(std::make_unique<AppSettings>(dir.filePath(QStringLiteral("other.json"))));
        new BuiltinActions(&other);
        addCoreSettingPages(&other, &loader);
        QCOMPARE(idsOf(other.settingCatalog()->pages()),
                 (QStringList{"editor.AppearanceAndBehavior", "core.Keymap", "editor.Editor",
                              "editor.Audio", "core.Plugins", "editor.Rendering"}));
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display. The settings directory of the user is a test directory in case a
    // loader accesses it.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    test_CoreSettingPages test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_CoreSettingPages.moc"
