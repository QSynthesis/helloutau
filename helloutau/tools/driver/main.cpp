#include <QtWidgets/QApplication>

#include <helloutau/Config.h>
#include <helloutau/Editor/AppLoader.h>

int main(int argc, char *argv[]) {
    // Before the application object, which loads the platform plugin of Qt
    hello::daw::AppLoader::addQtPluginPaths();

    QApplication app(argc, argv);
    // Name the directory of the settings file and of the user's plugins of UTAU, see AppSettings
    QApplication::setOrganizationName(QStringLiteral(HELLOUTAU_ORGANIZATION_NAME));
    QApplication::setApplicationName(QStringLiteral(HELLOUTAU_APPLICATION_NAME));
    QApplication::setApplicationVersion(QStringLiteral(HELLOUTAU_APPLICATION_VERSION));

    // The core plugin creates the editor and opens the windows.
    hello::daw::AppLoader loader(QApplication::arguments());
    return loader.run();
}
