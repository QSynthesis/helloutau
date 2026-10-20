#ifndef HELLOUTAU_CORE_INTERNAL_COREPLUGIN_H
#define HELLOUTAU_CORE_INTERNAL_COREPLUGIN_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <stdcorelib/pluginsystem/iplugin.h>

#include <helloutau/Editor/Editor.h>

namespace hello::daw {

    /// The core plugin, which the application requires in order to start. The plugin creates the
    /// editor, registers the action extensions of the editor with it, and records it in the
    /// loader for the other plugins (AppLoader::editor()). After every plugin is
    /// initialized, it opens the files of the command line, or a new project if no window is
    /// open. The files of a remote command, from the Finder on macOS or from a second start, open
    /// as well. See docs/Plugins.md.
    class CorePlugin : public stdc::pluginsystem::IPlugin {
    public:
        CorePlugin();
        ~CorePlugin();

        bool initialize(std::string *errorMessage) override;
        void pluginsInitialized() override;
        void aboutToShutdown() override;
        void remoteCommand(const std::vector<std::string> &arguments,
                           const std::filesystem::path &workingDirectory) override;

    private:
        std::optional<Editor> m_editor;
    };

}

#endif // HELLOUTAU_CORE_INTERNAL_COREPLUGIN_H
