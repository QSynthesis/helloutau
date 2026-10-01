#include "CorePlugin.h"

#include <filesystem>

#include <helloutau/Editor/AppLoader.h>
#include <helloutau/Editor/BuiltinActions.h>
#include <helloutau/Editor/Editor.h>

#include <Core/CoreSettingPages.h>

namespace hello::daw {

    CorePlugin::CorePlugin() = default;

    CorePlugin::~CorePlugin() = default;

    bool CorePlugin::initialize(std::string *errorMessage) {
        Q_UNUSED(errorMessage);
        m_actions = std::make_unique<BuiltinActions>();
        // The editor uses the settings of the loader, which outlive the editor, or the settings
        // of the user if no loader loaded this plugin.
        const auto loader = AppLoader::instance();
        m_editor =
            loader ? std::make_unique<Editor>(loader->settings()) : std::make_unique<Editor>();
        // The Plugins page exists only with a loader.
        addCoreSettingPages(m_editor.get(), loader);
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
    }

    // Destroys the editor and its windows while every library is still loaded. This instance is
    // a static object of the library and is destroyed only when the library is unloaded.
    void CorePlugin::aboutToShutdown() {
        m_editor.reset();
        m_actions.reset();
    }

}

STDC_EXPORT_PLUGIN(hello::daw::CorePlugin)
