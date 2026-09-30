#include <QtWidgets/QApplication>

#include <helloutau/Editor/AppLoader.h>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    // Names the settings file, see AppSettings
    QApplication::setOrganizationName(QStringLiteral("HelloUtau"));
    QApplication::setApplicationName(QStringLiteral("HelloUtau"));

    // The core plugin creates the editor and opens the windows.
    hello::daw::AppLoader loader(QApplication::arguments());
    return loader.run();
}
