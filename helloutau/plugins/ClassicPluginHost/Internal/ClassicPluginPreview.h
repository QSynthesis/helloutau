#ifndef HELLOUTAU_CLASSICPLUGINHOST_INTERNAL_CLASSICPLUGINPREVIEW_H
#define HELLOUTAU_CLASSICPLUGINHOST_INTERNAL_CLASSICPLUGINPREVIEW_H

#include <memory>

#include <QtCore/QtGlobal>

namespace hello::kit {
    struct Project;
}

namespace hello::daw {

    class ClassicPlugin;
    class ProjectWindow;

    /// The previews of one run of a classic plugin through hello-plugin-play (docs/PluginPlay.md):
    /// the pipe of the plugin process, the render cache of the run in the temporary directory of
    /// \a window , and the preview dialog of each request. The cache is removed with the object,
    /// at the end of the run.
    ///
    /// A preview is rendered through the playback of \a window with the synth tools of the
    /// project, which must be trusted. The user is not asked to trust them here.
    class ClassicPluginPreview {
    public:
        /// Prepares the previews of the run of \a plugin on the \a count notes of \a project
        /// from index \a first , as passed to the plugin.
        ClassicPluginPreview(ProjectWindow *window, const ClassicPlugin &plugin,
                             const kit::Project &project, int first, int count);

        /// Stops a preview that is still rendering or playing.
        ~ClassicPluginPreview();

        /// Starts to accept previews from the plugin process \a processId .
        ///
        /// \return whether the pipe and the cache were created
        bool start(qint64 processId);

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_INTERNAL_CLASSICPLUGINPREVIEW_H
