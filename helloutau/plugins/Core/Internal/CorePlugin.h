#ifndef HELLOUTAU_CORE_INTERNAL_COREPLUGIN_H
#define HELLOUTAU_CORE_INTERNAL_COREPLUGIN_H

#include <memory>
#include <string>

#include <stdcorelib/pluginsystem/iplugin.h>

namespace hello::daw {

    class Editor;

    /// The core plugin, which the application requires in order to start. The plugin creates the
    /// editor, registers the action extensions of the editor with it, and records it in the
    /// loader for the other plugins (AppLoader::editor()). After every plugin is
    /// initialized, it opens the files of the command line, or a new project if no window is
    /// open. See docs/Plugins.md.
    class CorePlugin : public stdc::pluginsystem::IPlugin {
    public:
        CorePlugin();
        ~CorePlugin();

        bool initialize(std::string *errorMessage) override;
        void pluginsInitialized() override;
        void aboutToShutdown() override;

    private:
        std::unique_ptr<Editor> m_editor;
    };

}

#endif // HELLOUTAU_CORE_INTERNAL_COREPLUGIN_H
