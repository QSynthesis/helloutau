#ifndef HELLOUTAU_EDITOR_PARAMETERVIEWMODIFIERS_H
#define HELLOUTAU_EDITOR_PARAMETERVIEWMODIFIERS_H

#include <helloutau/Widgets/ModifierBindings.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// The modifier scheme \c parameterView of the parameter area below the piano roll.
    class HELLOUTAU_EDITOR_EXPORT ParameterViewModifiers {
    public:
        enum Role {
            LockTime,
            SnapValue,
        };
        // Not declared: the wheel of the parameter area scrolls and zooms with HorizontalScroll,
        // TimeZoom and KeyZoom of NoteViewModifiers, because the parameter area follows the time
        // axis of the note area.

        static const ModifierScheme &scheme();
    };

}

#endif // HELLOUTAU_EDITOR_PARAMETERVIEWMODIFIERS_H
