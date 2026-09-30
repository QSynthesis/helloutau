#ifndef HELLOUTAU_CORE_COREPLUGIN_H
#define HELLOUTAU_CORE_COREPLUGIN_H

#include <memory>
#include <string>

#include <stdcorelib/pluginsystem/iplugin.h>

namespace hello::daw {

    class Editor;

    /// The core plugin, without which the application does not start: creates the editor, and
    /// once every plugin is initialized, opens the files of the command line or else a new
    /// project. See docs/Plugins.md.
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

#endif // HELLOUTAU_CORE_COREPLUGIN_H
