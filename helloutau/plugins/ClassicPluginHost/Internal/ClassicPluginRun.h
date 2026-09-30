#ifndef HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINRUN_H
#define HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINRUN_H

namespace hello::daw {

    class ClassicPlugin;
    class ProjectWindow;

    /// Runs \a plugin on the selection of \a window, as UTAU runs it from its menu, and applies
    /// its result as one undo step.
    ///
    /// Before a plugin runs for the first time, and after its program changed, the user is asked
    /// to confirm the program. While it runs, a modal dialog offers to cancel it. See
    /// docs/ClassicPluginHost.md.
    void runClassicPlugin(ProjectWindow *window, const ClassicPlugin &plugin);

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINRUN_H
