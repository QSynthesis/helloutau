#include <QtWidgets/QApplication>

#include <hellokit/Config.h>

#include <helloutau/Config.h>
#include <helloutau/Editor/AppLoader.h>

int main(int argc, char *argv[]) {
    // The Qt plugin paths are added before the application object, which loads the platform plugin
    // of Qt.
    hello::daw::AppLoader::addQtPluginPaths();

    QApplication app(argc, argv);
    // Name the directory of the settings file and of the user's plugins of UTAU, see AppSettings
    QApplication::setOrganizationName(QStringLiteral(HELLOUTAU_ORGANIZATION_NAME));
    QApplication::setApplicationName(QStringLiteral(HELLOUTAU_APPLICATION_NAME));
    QApplication::setApplicationVersion(QStringLiteral(HELLOKIT_VERSION));

    // The core plugin creates the editor and opens the windows, after the messages that precede
    // them, the usage of --help, after which the program ends, and the warning about the
    // privileges of the administrator.
    hello::daw::AppLoader loader(QApplication::arguments());
    if (!loader.showStartMessages()) {
        return 0;
    }
    return loader.run();
}
