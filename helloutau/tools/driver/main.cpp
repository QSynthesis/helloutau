#include <QtWidgets/QApplication>

#include <helloutau/Widgets/MainWindow.h>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    hello::daw::MainWindow window;
    window.show();

    return app.exec();
}
