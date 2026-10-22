#include "WindowPlacement.h"

#include <algorithm>
#include <tuple>

#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>

namespace hello::daw {

    QWidget *WindowPlacement::anchorOf(const QList<QPointer<QWidget>> &windows,
                                       const QWidget *window, const QPoint &pointer,
                                       const QScreen *screen) {
        const auto under = QApplication::topLevelAt(pointer);
        QWidget *last = nullptr;
        for (const auto &other : windows) {
            if (!other || other == window || !other->isVisible()) {
                continue;
            }
            if (other == under) {
                return other;
            }
            if (other->screen() == screen) {
                last = other;
            }
        }
        return last;
    }

    void WindowPlacement::place(QWidget *window, const QList<QPointer<QWidget>> &windows,
                                const QPoint &pointer) {
        auto screen = QGuiApplication::screenAt(pointer);
        if (!screen) {
            screen = QGuiApplication::primaryScreen();
        }
        if (!screen) {
            return;
        }
        const auto available = screen->availableGeometry();
        // The frame of a window that is not shown yet has the size of the window.
        const auto size = window->frameGeometry().size();
        const auto anchor = anchorOf(windows, window, pointer, screen);
        const auto wanted =
            anchor ? anchor->frameGeometry().topLeft() + QPoint(cascadeStep, cascadeStep)
                   : available.center() - QPoint(size.width() / 2, size.height() / 2);
        const int right = std::max(available.left(), available.right() - size.width() + 1);
        const int bottom = std::max(available.top(), available.bottom() - size.height() + 1);
        // A window moved to another screen before its native window exists is shown on the
        // primary screen, at a position that Windows chooses. Moved after the native window is
        // created, it is shown where it was moved (measured with Qt 6.11 on two screens,
        // 2026-10-22).
        std::ignore = window->winId();
        window->move(std::clamp(wanted.x(), available.left(), right),
                     std::clamp(wanted.y(), available.top(), bottom));
    }

}
