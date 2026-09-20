#include "MainWindow.h"

#include <QLabel>

namespace hu {

    MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
        setCentralWidget(new QLabel(QStringLiteral("Hello UTAU."), this));
    }

    MainWindow::~MainWindow() = default;

}
