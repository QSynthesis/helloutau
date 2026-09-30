#include "AppLoader.h"

#include <cassert>
#include <filesystem>
#include <vector>

#include <QtCore/QDir>
#include <QtCore/QtDebug>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>

#include <stdcorelib/pimpl.h>
#include <stdcorelib/pluginsystem/pluginsystem.h>

namespace hello::daw {

    namespace {

        AppLoader *currentAppLoader = nullptr;

    }

    class AppLoader::Impl {
    public:
        stdc::pluginsystem::PluginSystem system{AppLoader::pluginIid,
                                                stdc::pluginsystem::PluginSystem::Bundle};
        QStringList pluginPaths;
        QStringList files;
        bool loaded = false;
    };

    AppLoader::AppLoader(const QStringList &arguments) : _impl(std::make_unique<Impl>()) {
        stdc_impl_t;
        assert(!currentAppLoader);
        currentAppLoader = this;

        impl.pluginPaths.push_back(builtinPluginPath());
        for (int i = 1; i < arguments.size(); ++i) {
            if (arguments[i] == QLatin1String(pluginPathOption)) {
                // A trailing option without a directory is ignored.
                if (++i < arguments.size()) {
                    impl.pluginPaths.push_back(arguments[i]);
                }
                continue;
            }
            impl.files.push_back(arguments[i]);
        }
    }

    AppLoader::~AppLoader() {
        shutdown();
        currentAppLoader = nullptr;
    }

    AppLoader *AppLoader::instance() {
        return currentAppLoader;
    }

    QString AppLoader::builtinPluginPath() {
        return QDir::cleanPath(QCoreApplication::applicationDirPath() + QLatin1Char('/') +
                               QStringLiteral(HELLOUTAU_BUILTIN_PLUGINS_PATH));
    }

    QStringList AppLoader::pluginPaths() const {
        stdc_impl_t;
        return impl.pluginPaths;
    }

    void AppLoader::setPluginPaths(const QStringList &paths) {
        stdc_impl_t;
        impl.pluginPaths = paths;
    }

    QStringList AppLoader::files() const {
        stdc_impl_t;
        return impl.files;
    }

    bool AppLoader::load(QString *error) {
        stdc_impl_t;
        if (!impl.loaded) {
            impl.loaded = true;
            std::vector<std::filesystem::path> paths;
            for (const auto &path : std::as_const(impl.pluginPaths)) {
                paths.emplace_back(path.toStdU16String());
            }
            impl.system.setPluginPaths(paths);
            impl.system.loadPlugins();
        }

        for (const auto &message : errors()) {
            qWarning().noquote() << "Plugin" << message;
        }
        stdc::pluginsystem::PluginSpec *core = nullptr;
        for (const auto spec : impl.system.plugins()) {
            if (spec->id() == corePluginId) {
                core = spec;
                break;
            }
        }

        if (!core) {
            *error = tr("The core plugin was not found.");
            return false;
        }
        if (core->hasError()) {
            *error = QString::fromStdString(core->errorMessage());
            return false;
        }
        if (core->state() != stdc::pluginsystem::PluginSpec::Running) {
            *error = tr("The core plugin is disabled.");
            return false;
        }
        return true;
    }

    QStringList AppLoader::errors() const {
        stdc_impl_t;
        QStringList result;
        bool coreSeen = false;
        for (const auto spec : impl.system.plugins()) {
            // load() reports on the first core plugin, and another of the same ID is an error here.
            if (spec->id() == corePluginId && !coreSeen) {
                coreSeen = true;
                continue;
            }
            if (spec->hasError()) {
                result.push_back(QString::fromStdString(spec->id()) + QStringLiteral(": ") +
                                 QString::fromStdString(spec->errorMessage()));
            }
        }
        return result;
    }

    int AppLoader::run() {
        QString error;
        if (!load(&error)) {
            QMessageBox::critical(nullptr, QApplication::applicationName(),
                                  tr("The application cannot start: %1").arg(error));
            return 1;
        }
        const int code = QApplication::exec();
        shutdown();
        return code;
    }

    void AppLoader::shutdown() {
        stdc_impl_t;
        impl.system.shutdownPlugins();
    }

}
