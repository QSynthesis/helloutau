#include "ToolBarPalette_p.h"

#include <QtCore/QEvent>
#include <QtGui/QGuiApplication>
#include <QtGui/QPalette>
#include <QtGui/QStyleHints>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>

namespace hello::daw {

    namespace {

        // The share of the window text in the background of a checked button
        constexpr double checkedTint = 0.15;

        // The Windows 11 style fills a checked tool button with the accent color of the palette
        // and draws its text in the color for text on the accent. A ThemeIcon is drawn in the
        // window text color regardless of the button, and the accent of the dark color scheme
        // hides that color. A menu draws a checked icon on its own background, therefore the
        // palette applies to tool buttons only.
        void update(QToolButton *button) {
            const auto application = QGuiApplication::palette();
            auto palette = button->palette();
            for (const auto group : {QPalette::Active, QPalette::Inactive}) {
                const auto window = application.color(group, QPalette::Window);
                const auto text = application.color(group, QPalette::WindowText);
                const auto mix = [](int a, int b) { return int(a + (b - a) * checkedTint); };
                palette.setColor(group, QPalette::Accent,
                                 QColor(mix(window.red(), text.red()),
                                        mix(window.green(), text.green()),
                                        mix(window.blue(), text.blue())));
                // A checked button has the text color of the other buttons.
                palette.setColor(group, QPalette::ButtonText, text);
            }
            button->setPalette(palette);
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
