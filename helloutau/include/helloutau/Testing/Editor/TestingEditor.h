#ifndef HELLOUTAU_TESTING_EDITOR_TESTINGEDITOR_H
#define HELLOUTAU_TESTING_EDITOR_TESTINGEDITOR_H

#include <QtCore/QStringList>

#include <QAKCore/actionextension.h>

#include <helloutau/Editor/Editor.h>
#include <helloutau/Testing/Editor/HelloUtauTestingEditorGlobal.h>

class QAction;
class QMenu;

/// Test helpers for the actions and the menus of the windows of an \c Editor, which the tests of
/// this repository and of the projects that build on it use.
///
/// A test finds an action by its id, never by its text, because a text changes with a rename, a
/// mnemonic or the language. Each lookup verifies that the registry of the window declares the
/// id and records a test failure otherwise. A test of an id that the manifest no longer declares
/// therefore fails, including an assertion that the action is absent, which otherwise passes
/// vacuously.
namespace hello::daw {

    class ProjectWindow;
    class VoiceBankWindow;

    /// Returns whether the registry of the windows of \a kind declares the item \a id of \a type.
    /// Records a test failure otherwise.
    HELLOUTAU_TESTING_EDITOR_EXPORT bool
        isDeclared(const Editor &editor, Editor::WindowKind kind, const QString &id,
                   QAK::ActionItemInfo::Type type = QAK::ActionItemInfo::Action);

    /// Returns the action \a id of \a window. Records a test failure and returns \c nullptr if the
    /// registry does not declare the action or the window has none.
    HELLOUTAU_TESTING_EDITOR_EXPORT QAction *
        declaredActionOf(const Editor &editor, ProjectWindow *window, const QString &id);
    HELLOUTAU_TESTING_EDITOR_EXPORT QAction *
        declaredActionOf(const Editor &editor, VoiceBankWindow *window, const QString &id);

    /// Returns the menu \a id of \a window. Records a test failure and returns \c nullptr if the
    /// registry does not declare the menu or the window has none.
    HELLOUTAU_TESTING_EDITOR_EXPORT QMenu *declaredMenuOf(const Editor &editor,
                                                          ProjectWindow *window, const QString &id);
    HELLOUTAU_TESTING_EDITOR_EXPORT QMenu *
        declaredMenuOf(const Editor &editor, VoiceBankWindow *window, const QString &id);

    /// Returns the ids of the actions that \a menu of \a window shows, in their order. An action
    /// of the menu without an id, such as a separator, is omitted.
    HELLOUTAU_TESTING_EDITOR_EXPORT QStringList actionIdsIn(const Editor &editor,
                                                            ProjectWindow *window,
                                                            const QMenu *menu);
    HELLOUTAU_TESTING_EDITOR_EXPORT QStringList actionIdsIn(const Editor &editor,
                                                            VoiceBankWindow *window,
                                                            const QMenu *menu);

}

#endif // HELLOUTAU_TESTING_EDITOR_TESTINGEDITOR_H
