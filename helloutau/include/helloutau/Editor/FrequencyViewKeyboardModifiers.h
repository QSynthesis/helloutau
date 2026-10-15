#ifndef HELLOUTAU_EDITOR_FREQUENCYVIEWKEYBOARDMODIFIERS_H
#define HELLOUTAU_EDITOR_FREQUENCYVIEWKEYBOARDMODIFIERS_H

#include <helloutau/Widgets/KeyboardModifierBindings.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// The modifier scheme \c frequencyView of the editing area of the dialog that edits
    /// frequency maps. See the editing in docs/FrequencyMaps.md.
    ///
    /// The wheel defaults to the wheel of NoteViewKeyboardModifiers. The drags default to these:
    /// - A left drag with the pen draws the frequencies.
    /// - A left drag with the select tool selects frames.
    /// - A right drag unvoices frames, as in frqeditor.
    /// - A left drag of the key line in the labels sets the key.
    class HELLOUTAU_EDITOR_EXPORT FrequencyViewKeyboardModifiers {
    public:
        enum Role {
            VerticalScroll,
            HorizontalScroll,
            TimeZoom,
            KeyZoom,
            Draw,
            SelectFrames,
            Unvoice,
            DragZoom,
            DragZoomAxisLock,
            DragKey,
        };

        enum Scene {
            /// A wheel step
            WheelScene,

            /// A left click or drag with the select tool
            SelectDragScene,

            /// A left click or drag with the pen
            DrawDragScene,

            /// A right click or drag
            RightDragScene,

            /// A left click or drag of the key line in the labels
            KeyLineScene,
        };

        static const KeyboardModifierScheme &scheme();
    };

}

#endif // HELLOUTAU_EDITOR_FREQUENCYVIEWKEYBOARDMODIFIERS_H
