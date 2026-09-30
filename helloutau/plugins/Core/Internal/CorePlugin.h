#ifndef HELLOUTAU_CORE_COREPLUGIN_H
#define HELLOUTAU_CORE_COREPLUGIN_H

#include <memory>
#include <string>

#include <stdcorelib/pluginsystem/iplugin.h>

namespace hello::daw {

    class BuiltinActions;
    class Editor;

    /// The core plugin, which the application requires in order to start. The plugin registers
    /// the action extensions of the editor and creates the editor. After every plugin is
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
        std::unique_ptr<BuiltinActions> m_actions;
        std::unique_ptr<Editor> m_editor;
    };

}

#endif // HELLOUTAU_CORE_COREPLUGIN_H
