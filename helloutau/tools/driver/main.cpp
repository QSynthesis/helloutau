#include <QtWidgets/QApplication>

#include <helloutau/Editor/AppLoader.h>

int main(int argc, char *argv[]) {
    // Before the application object, which loads the platform plugin of Qt
    hello::daw::AppLoader::addQtPluginPaths();

    // QApplication::setStyle("Fusion");

    QApplication app(argc, argv);
    // Name the directory of the settings file and of the user's plugins of UTAU, see AppSettings
    QApplication::setOrganizationName(QStringLiteral("OpenVPI"));
    QApplication::setApplicationName(QStringLiteral("HelloUtau"));

    // The core plugin creates the editor and opens the windows.
    hello::daw::AppLoader loader(QApplication::arguments());
    return loader.run();
}
