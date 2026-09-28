#include <filesystem>

#include <QtWidgets/QApplication>

#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/MainWindow.h>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    // Names the settings file, see AppSettings
    QApplication::setOrganizationName(QStringLiteral("HelloUtau"));
    QApplication::setApplicationName(QStringLiteral("HelloUtau"));

    hello::daw::Editor editor;

    // Each file named on the command line opens in a window of its own.
    const auto arguments = QApplication::arguments().mid(1);
    for (const auto &argument : arguments) {
        editor.openFile(std::filesystem::path(argument.toStdU16String()));
    }
    if (editor.windows().isEmpty()) {
        editor.newWindow();
    }

    return app.exec();
}
