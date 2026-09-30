#ifndef HELLOUTAU_EDITOR_BUILTINACTIONS_H
#define HELLOUTAU_EDITOR_BUILTINACTIONS_H

#include <memory>

#include <QtCore/QtGlobal>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    class ActionRegistration;

    /// Registers the action extensions of the editor while it exists: the menus of the windows
    /// with the commands of the application, and the commands of the windows inserted into
    /// them. The windows create the actions of these commands themselves.
    ///
    /// The core plugin holds one, and so does a test that creates an Editor without the plugin.
    class HELLOUTAU_EDITOR_EXPORT BuiltinActions {
    public:
        BuiltinActions();
        ~BuiltinActions();

    private:
        std::unique_ptr<ActionRegistration> m_app;
        std::unique_ptr<ActionRegistration> m_windows;

        Q_DISABLE_COPY_MOVE(BuiltinActions)
    };

}

#endif // HELLOUTAU_EDITOR_BUILTINACTIONS_H
