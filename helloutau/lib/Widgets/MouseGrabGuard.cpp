#include "MouseGrabGuard.h"

#include <utility>

#include <QtCore/QCoreApplication>

namespace hello::daw {

    namespace {

        // Consumes the events of the given types.
        class EventTypeFilter : public QObject {
        public:
            explicit EventTypeFilter(QList<QEvent::Type> types) : m_types(std::move(types)) {
            }

        protected:
            bool eventFilter(QObject *watched, QEvent *event) override {
                if (m_types.contains(event->type())) {
                    event->accept();
                    return true;
                }
                return QObject::eventFilter(watched, event);
            }

        private:
            QList<QEvent::Type> m_types;
        };

    }

    MouseGrabGuard::MouseGrabGuard(QWidget *widget, const QList<QEvent::Type> &suppressedEvents)
        : MouseGrabGuard(widget, suppressedEvents.isEmpty()
                                     ? nullptr
                                     : std::make_unique<EventTypeFilter>(suppressedEvents)) {
    }

    MouseGrabGuard::MouseGrabGuard(QWidget *widget, std::unique_ptr<QObject> filter)
        : m_widget(widget) {
        if (filter) {
            QCoreApplication::instance()->installEventFilter(filter.get());
            m_filter = filter.release();
        }
        widget->grabMouse();
    }

    MouseGrabGuard::~MouseGrabGuard() {
        if (m_widget && QWidget::mouseGrabber() == m_widget) {
            m_widget->releaseMouse();
        }
        // Deleting the filter removes it from the application.
        if (m_filter) {
            m_filter->deleteLater();
        }
    }

    QWidget *MouseGrabGuard::widget() const {
        return m_widget;
    }

}
