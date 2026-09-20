#ifndef HELLOUTAU_WIDGETS_MAINWINDOW_H
#define HELLOUTAU_WIDGETS_MAINWINDOW_H

#include <QtWidgets/QMainWindow>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

namespace hello::daw {

    /// The editor window.
    class HELLOUTAU_WIDGETS_EXPORT MainWindow : public QMainWindow {
        Q_OBJECT
    public:
        explicit MainWindow(QWidget *parent = nullptr);
        ~MainWindow();
    };

}

#endif // HELLOUTAU_WIDGETS_MAINWINDOW_H
