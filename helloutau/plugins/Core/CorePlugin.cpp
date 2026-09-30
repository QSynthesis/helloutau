#include <filesystem>
#include <memory>

#include <stdcorelib/pluginsystem/iplugin.h>

#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/AppLoader.h>

namespace hello::daw {

    /// The core plugin: creates the editor, and once every plugin is initialized, opens the files
    /// of the command line or else a new project.
    class CorePlugin : public stdc::pluginsystem::IPlugin {
    public:
        bool initialize(std::string *errorMessage) override {
            (void) errorMessage;
            m_editor = std::make_unique<Editor>();
            return true;
        }

        // Called on the core plugin last, so that the windows open with everything the other
        // plugins registered.
        void pluginsInitialized() override {
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
        void aboutToShutdown() override {
            m_editor.reset();
        }

    private:
        std::unique_ptr<Editor> m_editor;
    };

}

STDC_EXPORT_PLUGIN(hello::daw::CorePlugin)
