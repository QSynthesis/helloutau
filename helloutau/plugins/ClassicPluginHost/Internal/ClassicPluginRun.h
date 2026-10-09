#ifndef HELLOUTAU_CLASSICPLUGINHOST_INTERNAL_CLASSICPLUGINRUN_H
#define HELLOUTAU_CLASSICPLUGINHOST_INTERNAL_CLASSICPLUGINRUN_H

namespace hello::daw {

    class ClassicPlugin;
    class ProjectWindow;

    /// Runs \a plugin on the selection of \a window , as UTAU runs a plugin from its menu, and
    /// applies the result as one undo step.
    ///
    /// Before the first run of a plugin, and after its program has changed, the user must
    /// confirm the program. While the plugin runs, a modal dialog provides a Cancel button. See
    /// docs/ClassicPluginHost.md.
    void runClassicPlugin(ProjectWindow *window, const ClassicPlugin &plugin);

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_INTERNAL_CLASSICPLUGINRUN_H
