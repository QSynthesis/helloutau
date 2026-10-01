#include <memory>

#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/BuiltinActions.h>
#include <helloutau/Editor/Editor.h>
#include <helloutau/Widgets/SettingPage.h>

#include <Core/CoreSettingPages.h>

using namespace hello::daw;

class test_CoreSettingPages : public QObject {
    Q_OBJECT

private:
    BuiltinActions m_actions;

private Q_SLOTS:
    // The pages of the core plugin take their places among those of the editor, in the order of
    // the settings of JetBrains IDEs.
    void the_pages_follow_the_order_of_jetbrains_ides() {
        QTemporaryDir dir;
        Editor e(std::make_unique<AppSettings>(dir.filePath(QStringLiteral("settings.json"))));
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
                              "editor.Rendering"}));
        QCOMPARE(idsOf(catalog->page(QStringLiteral("editor.AppearanceAndBehavior"))->pages()),
                 (QStringList{"core.MenusAndToolbars", "editor.SystemSettings"}));
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_CoreSettingPages test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_CoreSettingPages.moc"
