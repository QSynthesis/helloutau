#include "CorePlugin.h"

#include <filesystem>

#include <helloutau/Editor/AppLoader.h>
#include <helloutau/Editor/Editor.h>

namespace hello::daw {

    CorePlugin::CorePlugin() = default;

    CorePlugin::~CorePlugin() = default;

    bool CorePlugin::initialize(std::string *errorMessage) {
        Q_UNUSED(errorMessage);
        m_editor = std::make_unique<Editor>();
        return true;
    }

    // Called on the core plugin last, so that the windows open with everything the other
    // plugins registered.
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

    // The editor and its windows go now, while every library is loaded, rather than with this
    // instance, a static of the library destroyed as the library is unloaded.
    void CorePlugin::aboutToShutdown() {
        m_editor.reset();
    }

}

STDC_EXPORT_PLUGIN(hello::daw::CorePlugin)
