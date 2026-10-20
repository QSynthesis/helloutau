#include "CorePlugin.h"

#include <filesystem>

#include <hellokit/Edit/ProjectDocument.h>
#include <hellokit/Support/TranslationLoader.h>

#include <helloutau/Editor/AppLoader.h>
#include <helloutau/Editor/BuiltinActions.h>
#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/ProjectWindow.h>

#include <Core/CoreSettingPages.h>

namespace hello::daw {

    namespace {

        // Returns whether window holds a new project that the user has not edited, such as the
        // project that a start without files opens
        bool isUntouched(const ProjectWindow *window) {
            const auto document = window->document();
            const auto session = document->session();
            return document->sourcePath().empty() && !document->isModified() &&
                   !session->canUndo() && !session->canRedo();
        }

    }

    CorePlugin::CorePlugin() = default;

    CorePlugin::~CorePlugin() = default;

    bool CorePlugin::initialize(std::string *errorMessage) {
        Q_UNUSED(errorMessage);
        if (const auto translations = kit::TranslationLoader::instance()) {
            translations->load(QStringLiteral("Core"),
                               QStringLiteral(":/helloutau/plugins/Core/translations"));
        }
        // The editor uses the settings of the loader, which outlive the editor, or the settings
        // of the user if no loader loaded this plugin.
        const auto loader = AppLoader::instance();
        if (loader) {
            m_editor.emplace(loader->settings());
        } else {
            m_editor.emplace();
        }
        // A child of the editor, destroyed with it
        new BuiltinActions(&*m_editor);
        // The Plugins page exists only with a loader. The other plugins, which depend on this
        // plugin and initialize after it, obtain the editor from the loader.
        addCoreSettingPages(&*m_editor, loader);
        if (loader) {
            loader->setEditor(&*m_editor);
        }
        return true;
    }

    // Called on the core plugin last, so that the windows open with all registrations of the
    // other plugins.
    void CorePlugin::pluginsInitialized() {
        if (const auto loader = AppLoader::instance()) {
            for (const auto &file : loader->files()) {
                m_editor->openFile(std::filesystem::path(file.toStdU16String()));
            }
        }
        if (m_editor->windows().isEmpty()) {
            m_editor->newWindow();
        }
        // The check runs only with a loader, so that a test that loads this plugin sends no
        // request.
        if (AppLoader::instance()) {
            m_editor->checkForUpdatesAtStartup();
        }
    }

    void CorePlugin::remoteCommand(const std::vector<std::string> &arguments,
                                   const std::filesystem::path &workingDirectory) {
        if (!m_editor) {
            return;
        }
        // A window opened from the file manager comes to the front.
        const auto activate = [](QWidget *window) {
            if (window) {
                window->raise();
                window->activateWindow();
            }
        };
        // A second start without files would have shown a new project.
        if (arguments.empty()) {
            activate(m_editor->newWindow());
            return;
        }
        for (const auto &argument : arguments) {
            auto path = std::filesystem::path(QString::fromStdString(argument).toStdU16String());
            if (path.is_relative() && !workingDirectory.empty()) {
                path = workingDirectory / path;
            }
            // The window of an untouched new project takes the file if it is the only window,
            // because the Finder passes the file of a start only after the start opened that
            // window.
            const auto windows = m_editor->windows();
            const auto from =
                windows.size() == 1 && isUntouched(windows.first()) ? windows.first() : nullptr;
            activate(m_editor->openFile(path, from));
        }
    }

    // Destroys the editor and its windows while every library is still loaded. This instance is
    // a static object of the library and is destroyed only when the library is unloaded.
    void CorePlugin::aboutToShutdown() {
        if (const auto loader = AppLoader::instance()) {
            loader->setEditor(nullptr);
        }
        m_editor.reset();
        // Removed before this library, which holds the translation, is unloaded
        if (const auto translations = kit::TranslationLoader::instance()) {
            translations->remove(QStringLiteral("Core"));
        }
    }

}

STDC_EXPORT_PLUGIN(hello::daw::CorePlugin)
