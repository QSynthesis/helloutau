#include <memory>

#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtGui/QWheelEvent>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QTabWidget>

#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/BuiltinActions.h>
#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/NoteViewKeyboardModifiers.h>

#include <Core/KeyboardModifierSettingPage.h>

using namespace hello::daw;

namespace {

    // Returns an editor with the built-in actions and its settings in \a dir.
    std::unique_ptr<Editor> editorIn(const QTemporaryDir &dir) {
        auto e = std::make_unique<Editor>(
            std::make_unique<AppSettings>(dir.filePath(QStringLiteral("settings.json"))));
        new BuiltinActions(e.get());
        e->setWatchesDisk(false);
        return e;
    }

    Qt::KeyboardModifiers horizontalScrollOf(const Editor &e) {
        return e.keyboardModifierBindings(Editor::ProjectWindowKind,
                                          NoteViewKeyboardModifiers::scheme())
            .modifiers(NoteViewKeyboardModifiers::HorizontalScroll)
            .value_or(Qt::NoModifier);
    }

}

class test_KeyboardModifierSettingPage : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The page applies the modifiers to the editor and writes them to keymap.json, which a new
    // editor reads. Restore Defaults returns them to their defaults.
    void the_page_assigns_modifiers() {
        QTemporaryDir dir;
        auto e = editorIn(dir);
        KeyboardModifierSettingPage page(e.get());
        QCOMPARE(page.id(), QStringLiteral("core.KeyboardModifiers"));
        const auto box = page.widget()->findChild<QComboBox *>(
            QStringLiteral("projectWindow/noteView/horizontalScroll"));
        QVERIFY(box);
        QVERIFY(!page.isModified());
        box->setCurrentIndex(box->findData(int(Qt::AltModifier)));
        QVERIFY(page.isModified());
        QString error;
        QVERIFY(page.apply(&error));
        QVERIFY(!page.isModified());
        QCOMPARE(horizontalScrollOf(*e), Qt::KeyboardModifiers(Qt::AltModifier));
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("keymap.json"))));

        // A new editor reads the file.
        e.reset();
        const auto again = editorIn(dir);
        QCOMPARE(horizontalScrollOf(*again), Qt::KeyboardModifiers(Qt::AltModifier));

        KeyboardModifierSettingPage other(again.get());
        const auto restored = other.widget()->findChild<QComboBox *>(
            QStringLiteral("projectWindow/noteView/horizontalScroll"));
        QCOMPARE(restored->currentData().toInt(), int(Qt::AltModifier));
        other.widget()->findChild<QPushButton *>(QStringLiteral("restoreDefaults"))->click();
        QVERIFY(restored->currentData().toInt() != int(Qt::AltModifier));
        QVERIFY(other.apply(&error));
        QCOMPARE(horizontalScrollOf(*again),
                 KeyboardModifierBindings(NoteViewKeyboardModifiers::scheme())
                     .modifiers(NoteViewKeyboardModifiers::HorizontalScroll)
                     .value_or(Qt::NoModifier));
    }

    // An option of modifiers is named as the platform writes the modifiers of a shortcut.
    void the_modifier_options_are_named_as_shortcuts_are() {
        QTemporaryDir dir;
        const auto e = editorIn(dir);
        KeyboardModifierSettingPage page(e.get());
        int named = 0;
        for (const auto box : page.widget()->findChildren<QComboBox *>()) {
            for (int i = 0; i < box->count(); ++i) {
                const int data = box->itemData(i).toInt();
                if (data <= 0) {
                    continue;
                }
                const auto text = box->itemText(i);
                const auto shortcut =
                    QKeySequence(QKeyCombination(Qt::KeyboardModifiers(data), Qt::Key_A))
                        .toString(QKeySequence::NativeText);
                QVERIFY2(!text.isEmpty() && !text.endsWith(QLatin1Char('+')), qPrintable(text));
                QVERIFY2(shortcut.startsWith(text),
                         qPrintable(text + QStringLiteral(" ") + shortcut));
                ++named;
            }
        }
        QVERIFY(named > 0);
    }

    // Each kind of window with modifiers has a tab that scrolls its groups. A page lower than its
    // groups still shows Restore Defaults beside the tabs.
    void each_kind_of_window_has_a_tab_that_scrolls() {
        QTemporaryDir dir;
        const auto e = editorIn(dir);
        KeyboardModifierSettingPage page(e.get());
        const auto widget = page.widget();
        const auto tabs = widget->findChild<QTabWidget *>(QStringLiteral("windows"));
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), int(std::size(Editor::windowKinds)));
        for (int i = 0; i < tabs->count(); ++i) {
            QVERIFY(qobject_cast<QScrollArea *>(tabs->widget(i)));
        }
        const auto restore =
            widget->findChild<QPushButton *>(QStringLiteral("restoreDefaults"));
        QVERIFY(restore);
        widget->resize(600, 240);
        widget->show();
        QVERIFY(QTest::qWaitForWindowExposed(widget));
        QVERIFY(widget->minimumSizeHint().height() <= 240);
        QCOMPARE(restore->visibleRegion().boundingRect().size(), restore->size());
        const auto scroll = qobject_cast<QScrollArea *>(tabs->currentWidget());
        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
    }

    // Restore Defaults is enabled only while a binding differs from its default, and a binding
    // changed and restored leaves nothing modified. The boxes ignore the wheel.
    void the_defaults_are_restored() {
        QTemporaryDir dir;
        const auto e = editorIn(dir);
        KeyboardModifierSettingPage page(e.get());
        const auto restore =
            page.widget()->findChild<QPushButton *>(QStringLiteral("restoreDefaults"));
        QVERIFY(restore);
        QVERIFY(!restore->isEnabled());
        const auto box = page.widget()->findChild<QComboBox *>(
            QStringLiteral("projectWindow/noteView/horizontalScroll"));
        QVERIFY(box);
        const auto original = box->currentIndex();

        // The wheel does not change the binding.
        QWheelEvent wheel(QPointF(5, 5), box->mapToGlobal(QPointF(5, 5)), QPoint(), QPoint(0, -120),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(box, &wheel);
        QCOMPARE(box->currentIndex(), original);

        box->setCurrentIndex(box->findData(int(Qt::AltModifier)));
        QVERIFY(restore->isEnabled());
        QVERIFY(page.isModified());
        restore->click();
        QCOMPARE(box->currentIndex(), original);
        QVERIFY(!restore->isEnabled());
        QVERIFY(!page.isModified());
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_KeyboardModifierSettingPage test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_KeyboardModifierSettingPage.moc"
