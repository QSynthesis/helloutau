#ifndef HELLOUTAU_EDITOR_BUILTINACTIONS_H
#define HELLOUTAU_EDITOR_BUILTINACTIONS_H

#include <memory>

#include <QtCore/QObject>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    class ActionRegistration;
    class Editor;

    /// Registers the action extensions of the editor with the registry of \a editor for the
    /// lifetime of this object, one for each kind of window, which declare the menus, the tool
    /// bars and the commands of that kind. The windows create the actions of these commands
    /// themselves.
    ///
    /// The object is a child of \a editor and is destroyed with it at the latest. The core plugin
    /// creates one for its editor, and a test that creates an Editor without the plugin also
    /// creates one.
    class HELLOUTAU_EDITOR_EXPORT BuiltinActions : public QObject {
    public:
        explicit BuiltinActions(Editor *editor);
        ~BuiltinActions() override;

    private:
        std::unique_ptr<ActionRegistration> m_registration;
    };

}

#endif // HELLOUTAU_EDITOR_BUILTINACTIONS_H
