#include <memory>

#include <QtCore/QTemporaryDir>
#include <QtGui/QAction>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>

#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/BuiltinActions.h>
#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/ProjectWindow.h>

#include <helloutau/Testing/Editor/TestingEditor.h>

using namespace hello::daw;

class test_TestingEditor : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    // An editor with the menus and the commands of the editor manifest
    std::unique_ptr<Editor> editor() const {
        auto e = std::make_unique<Editor>(
            std::make_unique<AppSettings>(m_dir.filePath(QStringLiteral("settings.json"))));
        e->setWatchesDisk(false);
        new BuiltinActions(e.get());
        return e;
    }

private Q_SLOTS:
    // A declared action and a declared menu are found by their ids, and the ids of the actions
    // of the menu are listed.
    void declared_items_are_found_by_id() {
        const auto e = editor();
        const auto window = e->newWindow();
        const auto clearCache = QStringLiteral("helloutau.tools.clearCache");
        QVERIFY(isDeclared(*e, Editor::ProjectWindowKind, clearCache));
        QVERIFY(isDeclared(*e, Editor::ProjectWindowKind, QStringLiteral("helloutau.tools"),
                           QAK::ActionItemInfo::Menu));
        const auto action = declaredActionOf(*e, window, clearCache);
        QVERIFY(action);
        const auto menu = declaredMenuOf(*e, window, QStringLiteral("helloutau.tools"));
        QVERIFY(menu);
        QVERIFY(menu->actions().contains(action));
        QVERIFY(actionIdsIn(*e, window, menu).contains(clearCache));
        QVERIFY(unhandledActionsOf(*e, window).isEmpty());
    }

    // An id that the registry does not declare records a test failure, also for an item of
    // the other type.
    void an_undeclared_id_fails() {
        const auto e = editor();
        const auto window = e->newWindow();
        const auto missing = QStringLiteral("helloutau.tools.noSuchAction");
        const auto clearCache = QStringLiteral("helloutau.tools.clearCache");
        // Each expected failure is consumed by the failure that the call records. A call that
        // records none leaves the expectation to the next QEXPECT_FAIL, which then fails, so the
        // results are checked only after the last call.
        QEXPECT_FAIL("", "The id is an action, not a menu.", Continue);
        const bool declaredAsMenu =
            isDeclared(*e, Editor::ProjectWindowKind, clearCache, QAK::ActionItemInfo::Menu);
        QEXPECT_FAIL("", "The id is not declared.", Continue);
        const bool declared = isDeclared(*e, Editor::ProjectWindowKind, missing);
        QEXPECT_FAIL("", "The id is not declared.", Continue);
        const auto action = declaredActionOf(*e, window, missing);
        QEXPECT_FAIL("", "The id is an action, not a menu.", Continue);
        const auto menu = declaredMenuOf(*e, window, clearCache);
        QVERIFY(!declaredAsMenu);
        QVERIFY(!declared);
        QVERIFY(!action);
        QVERIFY(!menu);
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_TestingEditor test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_TestingEditor.moc"
