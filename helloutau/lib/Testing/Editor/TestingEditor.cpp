#include "TestingEditor.h"

#include <QtGui/QAction>
#include <QtTest/QTest>
#include <QtWidgets/QMenu>

#include <QAKCore/actionregistry.h>
#include <QAKWidgets/widgetactioncontext.h>

#include <helloutau/Editor/ProjectWindow.h>
#include <helloutau/Editor/VoiceBankWindow.h>

namespace hello::daw {

    namespace {

        void fail(const QString &message) {
            QTest::qFail(qPrintable(message), __FILE__, __LINE__);
        }

        QAction *actionOf(const Editor &editor, Editor::WindowKind kind,
                          const QAK::WidgetActionContext *context, const QString &id) {
            if (!isDeclared(editor, kind, id)) {
                return nullptr;
            }
            const auto action = context->action(id);
            if (!action) {
                fail(QStringLiteral("The window has no action %1.").arg(id));
            }
            return action;
        }

        QMenu *menuOf(const Editor &editor, Editor::WindowKind kind,
                      const QAK::WidgetActionContext *context, const QString &id) {
            if (!isDeclared(editor, kind, id, QAK::ActionItemInfo::Menu)) {
                return nullptr;
            }
            const auto menu = context->menu(id);
            if (!menu) {
                fail(QStringLiteral("The window has no menu %1.").arg(id));
            }
            return menu;
        }

        QStringList idsIn(const Editor &editor, Editor::WindowKind kind,
                          const QAK::WidgetActionContext *context, const QMenu *menu) {
            const auto ids = editor.actionRegistry(kind)->actionIds();
            QStringList result;
            for (const auto action : menu->actions()) {
                for (const auto &id : ids) {
                    if (context->action(id) == action ||
                        (action->menu() && context->menu(id) == action->menu())) {
                        result.push_back(id);
                        break;
                    }
                }
            }
            return result;
        }

        // QActionKit creates the action of a declared id that the window provides no action for
        // with the context as its parent, so that the item still appears in the layouts.
        QStringList unhandledIn(const Editor &editor, Editor::WindowKind kind,
                                const QAK::WidgetActionContext *context) {
            const auto registry = editor.actionRegistry(kind);
            QStringList result;
            for (const auto &id : registry->actionIds()) {
                const auto info = registry->actionInfo(id);
                if (!info || info->type() != QAK::ActionItemInfo::Action) {
                    continue;
                }
                const auto action = context->action(id);
                if ((action && action->parent() != context) || !context->widgets(id).isEmpty()) {
                    continue;
                }
                result.push_back(id);
            }
            return result;
        }

    }

    bool isDeclared(const Editor &editor, Editor::WindowKind kind, const QString &id,
                    QAK::ActionItemInfo::Type type) {
        const auto info = editor.actionRegistry(kind)->actionInfo(id);
        if (info && info->type() == type) {
            return true;
        }
        fail(QStringLiteral("The manifest declares no item %1 of this type.").arg(id));
        return false;
    }

    QAction *declaredActionOf(const Editor &editor, ProjectWindow *window, const QString &id) {
        return actionOf(editor, Editor::ProjectWindowKind, window->actionContext(), id);
    }

    QAction *declaredActionOf(const Editor &editor, VoiceBankWindow *window, const QString &id) {
        return actionOf(editor, Editor::VoiceBankWindowKind, window->actionContext(), id);
    }

    QMenu *declaredMenuOf(const Editor &editor, ProjectWindow *window, const QString &id) {
        return menuOf(editor, Editor::ProjectWindowKind, window->actionContext(), id);
    }

    QMenu *declaredMenuOf(const Editor &editor, VoiceBankWindow *window, const QString &id) {
        return menuOf(editor, Editor::VoiceBankWindowKind, window->actionContext(), id);
    }

    QStringList actionIdsIn(const Editor &editor, ProjectWindow *window, const QMenu *menu) {
        return idsIn(editor, Editor::ProjectWindowKind, window->actionContext(), menu);
    }

    QStringList actionIdsIn(const Editor &editor, VoiceBankWindow *window, const QMenu *menu) {
        return idsIn(editor, Editor::VoiceBankWindowKind, window->actionContext(), menu);
    }

    QStringList unhandledActionsOf(const Editor &editor, ProjectWindow *window) {
        return unhandledIn(editor, Editor::ProjectWindowKind, window->actionContext());
    }

    QStringList unhandledActionsOf(const Editor &editor, VoiceBankWindow *window) {
        return unhandledIn(editor, Editor::VoiceBankWindowKind, window->actionContext());
    }

}
