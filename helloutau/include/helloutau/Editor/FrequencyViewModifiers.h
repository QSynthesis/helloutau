#ifndef HELLOUTAU_EDITOR_FREQUENCYVIEWMODIFIERS_H
#define HELLOUTAU_EDITOR_FREQUENCYVIEWMODIFIERS_H

#include <helloutau/Widgets/ModifierBindings.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// The modifier scheme \c frequencyView of the editing area of the dialog that edits
    /// frequency maps. See the editing in docs/FrequencyMaps.md.
    ///
    /// The wheel defaults to the wheel of NoteViewModifiers. The drags default to those of
    /// frqeditor:
    /// - A left drag draws the frequencies.
    /// - A left drag with Ctrl selects frames.
    /// - A right drag unvoices frames.
    class HELLOUTAU_EDITOR_EXPORT FrequencyViewModifiers {
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
        };

        enum Scene {
            /// A wheel step
            WheelScene,

            /// A left click or drag
            LeftDragScene,

            /// A right click or drag
            RightDragScene,
        };

        static const ModifierScheme &scheme();
    };

}

#endif // HELLOUTAU_EDITOR_FREQUENCYVIEWMODIFIERS_H
