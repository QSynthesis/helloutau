#include <QApplication>

#include <helloutau/Widgets/MainWindow.h>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    hu::MainWindow window;
    window.show();

    return app.exec();
}
