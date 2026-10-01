#include "ToolBarPalette_p.h"

#include <QtCore/QEvent>
#include <QtGui/QGuiApplication>
#include <QtGui/QPalette>
#include <QtGui/QStyleHints>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>

#include <helloutau/Theme/ThemeIcon.h>

namespace hello::daw {

    namespace {

        // The Windows 11 style fills a checked tool button with the accent color of the palette
        // and draws its text in the color for text on the accent. A ThemeIcon has its own colors,
        // so the checked colors must be set on the icon as well.
        void update(QToolButton *button) {
            auto palette = button->palette();
            for (const auto group : {QPalette::Active, QPalette::Inactive}) {
                // A checked button has white text on the accent background.
                palette.setColor(group, QPalette::ButtonText, Qt::white);
            }
            button->setPalette(palette);
            if (auto icon = ThemeIcon::of(button->icon())) {
                const auto checkedText = palette.color(QPalette::Active, QPalette::ButtonText);
                for (const auto state : {ThemeButtonState::CheckedUp,
                                         ThemeButtonState::CheckedOver,
                                         ThemeButtonState::CheckedDown,
                                         ThemeButtonState::CheckedDisabled}) {
                    icon->colors.setValue(state, checkedText);
                }
                button->setIcon(icon->icon());
            }
            constexpr auto Connected = "helloToolBarPaletteConnected";
            if (!button->property(Connected).toBool()) {
                button->setProperty(Connected, true);
                QObject::connect(button, &QToolButton::toggled, button,
                                 [button] { update(button); });
            }
        }

        // Gives each button of a tool bar the palette once the button is polished. A palette
        // given to the tool bar does not reach its buttons, whose palettes the style sheet of
        // the window sets as it polishes them, and the buttons are created anew whenever
        // QActionKit rebuilds the tool bar.
        class ButtonPalettes : public QObject {
        public:
            using QObject::QObject;

        protected:
            bool eventFilter(QObject *watched, QEvent *event) override {
                if (event->type() == QEvent::ChildPolished) {
                    if (const auto button = qobject_cast<QToolButton *>(
                            static_cast<QChildEvent *>(event)->child())) {
                        update(button);
                    }
                }
                return QObject::eventFilter(watched, event);
            }
        };

    }

    void followToolBarPalette(QToolBar *toolBar) {
        toolBar->installEventFilter(new ButtonPalettes(toolBar));
        // The update is queued, so that the palette of the application follows the color scheme
        // first.
        QObject::connect(
            QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, toolBar,
            [toolBar] {
                for (const auto button : toolBar->findChildren<QToolButton *>()) {
                    update(button);
                }
            },
            Qt::QueuedConnection);
    }

}
