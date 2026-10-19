#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>

#include <helloutau/Widgets/HeightForWidthFilter.h>

using namespace hello::daw;

namespace {

    // A window with a line of text and a word-wrapped label of several lines below it
    struct WrappingWindow {
        QWidget window;
        QLabel *label;

        WrappingWindow() {
            label = new QLabel(QStringLiteral("word ").repeated(60).trimmed());
            label->setWordWrap(true);
            auto layout = new QVBoxLayout(&window);
            layout->addWidget(new QLabel(QStringLiteral("line")));
            layout->addWidget(label);
        }

        // Returns whether the label has the height that its text requires for its width.
        bool labelFits() const {
            return label->height() >= label->heightForWidth(label->width());
        }
    };

}

class test_HeightForWidthFilter : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // A window resized to its minimum height gives a word-wrapped label the height of one line
    // without the filter.
    void a_window_at_its_minimum_height_fits_a_wrapped_label() {
        WrappingWindow window;
        new HeightForWidthFilter(&window.window);
        window.window.resize(200, 0);
        window.window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window.window));
        QVERIFY(window.labelFits());
        QVERIFY(window.label->height() > window.label->fontMetrics().height() * 2);
    }

    void a_label_shown_later_fits() {
        WrappingWindow window;
        new HeightForWidthFilter(&window.window);
        window.label->hide();
        window.window.resize(200, 0);
        window.window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window.window));

        window.label->show();
        QTRY_VERIFY(window.labelFits());
    }

    void a_narrower_window_fits_the_label() {
        WrappingWindow window;
        new HeightForWidthFilter(&window.window);
        window.window.resize(400, 0);
        window.window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window.window));
        const int height = window.window.height();

        window.window.resize(200, window.window.height());
        QTRY_VERIFY(window.labelFits());
        QVERIFY(window.window.height() > height);
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_HeightForWidthFilter test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_HeightForWidthFilter.moc"
