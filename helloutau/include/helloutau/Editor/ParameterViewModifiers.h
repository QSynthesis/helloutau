#ifndef HELLOUTAU_EDITOR_PARAMETERVIEWMODIFIERS_H
#define HELLOUTAU_EDITOR_PARAMETERVIEWMODIFIERS_H

#include <helloutau/Widgets/ModifierBindings.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// The modifier scheme \c parameterView of the parameter area below the piano roll.
    class HELLOUTAU_EDITOR_EXPORT ParameterViewModifiers {
    public:
        enum Role {
            DragValue,
            LockTime,
            SnapValue,
        };
        // This scheme declares no wheel roles. The wheel of the parameter area scrolls and zooms
        // with the WheelScene of NoteViewModifiers, because the parameter area follows the time
        // axis of the note area.

        enum Scene {
            /// A left drag of an envelope point or a value
            DragScene,
        };

        static const ModifierScheme &scheme();
    };

}

#endif // HELLOUTAU_EDITOR_PARAMETERVIEWMODIFIERS_H
