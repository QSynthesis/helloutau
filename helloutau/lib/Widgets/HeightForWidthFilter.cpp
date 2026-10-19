#include "HeightForWidthFilter.h"

#include <QtCore/QEvent>
#include <QtWidgets/QLayout>

namespace hello::daw {

    HeightForWidthFilter::HeightForWidthFilter(QWidget *window) : QObject(window) {
        window->installEventFilter(this);
    }

    HeightForWidthFilter::~HeightForWidthFilter() = default;

    bool HeightForWidthFilter::eventFilter(QObject *watched, QEvent *event) {
        // QEvent::Polish precedes the first show, while a resize still takes effect when the
        // window is shown. A resize during QEvent::Show does not resize the layout. The layout
        // has processed QEvent::LayoutRequest before the event filters receive it.
        switch (event->type()) {
            case QEvent::Polish:
            case QEvent::LayoutRequest:
            case QEvent::Resize: {
                const auto window = static_cast<QWidget *>(watched);
                const auto layout = window->layout();
                if (layout && layout->hasHeightForWidth()) {
                    window->setMinimumHeight(layout->totalHeightForWidth(window->width()));
                }
                break;
            }
            default:
                break;
        }
        return QObject::eventFilter(watched, event);
    }

}
