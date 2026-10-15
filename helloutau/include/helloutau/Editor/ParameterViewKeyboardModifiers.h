#ifndef HELLOUTAU_EDITOR_PARAMETERVIEWKEYBOARDMODIFIERS_H
#define HELLOUTAU_EDITOR_PARAMETERVIEWKEYBOARDMODIFIERS_H

#include <helloutau/Widgets/KeyboardModifierBindings.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// The modifier scheme \c parameterView of the parameter area below the piano roll.
    class HELLOUTAU_EDITOR_EXPORT ParameterViewKeyboardModifiers {
    public:
        enum Role {
            DragValue,
            LockTime,
            SnapValue,
        };
        // This scheme declares no wheel roles. The wheel of the parameter area scrolls and zooms
        // with the WheelScene of NoteViewKeyboardModifiers, because the parameter area follows the
        // time axis of the note area.

        enum Scene {
            /// A left drag of an envelope point or a value
            DragScene,
        };

        static const KeyboardModifierScheme &scheme();
    };

}

#endif // HELLOUTAU_EDITOR_PARAMETERVIEWKEYBOARDMODIFIERS_H
