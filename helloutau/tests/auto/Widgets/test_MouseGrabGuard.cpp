#include <memory>

#include <QtCore/QPointer>
#include <QtGui/QContextMenuEvent>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <helloutau/Widgets/MouseGrabGuard.h>

using namespace hello::daw;

namespace {

    // Counts the context menu events and the user events that reach it.
    class CountingWidget : public QWidget {
    public:
        int contextMenus = 0;
        int userEvents = 0;

    protected:
        void contextMenuEvent(QContextMenuEvent *event) override {
            ++contextMenus;
            event->accept();
        }

        bool event(QEvent *event) override {
            if (event->type() == QEvent::User) {
                ++userEvents;
                return true;
            }
            return QWidget::event(event);
        }
    };

    // Consumes the user events of the application, and counts them.
    class UserEventFilter : public QObject {
    public:
        int filtered = 0;

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override {
            if (event->type() == QEvent::User) {
                ++filtered;
                return true;
            }
            return QObject::eventFilter(watched, event);
        }
    };

    void sendContextMenu(QWidget *widget) {
        QContextMenuEvent event(QContextMenuEvent::Mouse, QPoint(1, 1));
        QCoreApplication::sendEvent(widget, &event);
    }

    void sendUser(QWidget *widget) {
        QEvent event(QEvent::User);
        QCoreApplication::sendEvent(widget, &event);
    }

}

class test_MouseGrabGuard : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<CountingWidget> m_grabber;
    std::unique_ptr<CountingWidget> m_other;

private Q_SLOTS:
    void init() {
        m_grabber = std::make_unique<CountingWidget>();
        m_other = std::make_unique<CountingWidget>();
        m_grabber->setGeometry(100, 100, 100, 100);
        m_other->setGeometry(300, 100, 100, 100);
        m_grabber->show();
        m_other->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_grabber.get()));
        QVERIFY(QTest::qWaitForWindowExposed(m_other.get()));
    }

    void cleanup() {
        m_grabber.reset();
        m_other.reset();
    }

    void the_grab_lasts_as_long_as_the_guard() {
        {
            MouseGrabGuard guard(m_grabber.get());
            QCOMPARE(QWidget::mouseGrabber(), m_grabber.get());
            QCOMPARE(guard.widget(), m_grabber.get());
        }
        QCOMPARE(QWidget::mouseGrabber(), nullptr);
    }

    // The guard releases the grab only if its widget still holds it, so that it does not end the
    // grab of another widget.
    void the_grab_of_another_widget_is_not_released() {
        {
            MouseGrabGuard guard(m_grabber.get());
            m_other->grabMouse();
        }
        QCOMPARE(QWidget::mouseGrabber(), m_other.get());
        m_other->releaseMouse();
    }

    // The listed events are consumed in the whole application while the guard exists, and the
    // others are delivered.
    void the_listed_events_are_suppressed_everywhere() {
        {
            MouseGrabGuard guard(m_grabber.get(), {QEvent::ContextMenu});
            sendContextMenu(m_other.get());
            sendUser(m_other.get());
            QCOMPARE(m_other->contextMenus, 0);
            QCOMPARE(m_other->userEvents, 1);
        }
        // The filter remains until the event loop runs, for the events that follow the release.
        sendContextMenu(m_other.get());
        QCOMPARE(m_other->contextMenus, 0);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        sendContextMenu(m_other.get());
        QCOMPARE(m_other->contextMenus, 1);
    }

    // A filter of the caller is installed on the application, and the guard deletes it.
    void a_given_filter_is_installed_and_deleted() {
        auto filter = std::make_unique<UserEventFilter>();
        const auto raw = filter.get();
        QPointer<QObject> watched = raw;
        {
            MouseGrabGuard guard(m_grabber.get(), std::move(filter));
            sendUser(m_other.get());
            QCOMPARE(raw->filtered, 1);
            QCOMPARE(m_other->userEvents, 0);
        }
        QVERIFY(watched);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!watched);
        sendUser(m_other.get());
        QCOMPARE(m_other->userEvents, 1);
    }

    void a_null_filter_only_grabs() {
        MouseGrabGuard guard(m_grabber.get(), std::unique_ptr<QObject>());
        QCOMPARE(QWidget::mouseGrabber(), m_grabber.get());
        sendUser(m_other.get());
        QCOMPARE(m_other->userEvents, 1);
    }
};

QTEST_MAIN(test_MouseGrabGuard)

#include "test_MouseGrabGuard.moc"
