#ifndef HELLOUTAU_EDITOR_NOTEVIEWMODIFIERS_H
#define HELLOUTAU_EDITOR_NOTEVIEWMODIFIERS_H

#include <helloutau/Widgets/ModifierBindings.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// The modifier scheme \c noteView of the note area of the piano roll. The ruler above the
    /// piano roll has no scheme of its own and turns snapping off while DisableNoteSnap is held.
    class HELLOUTAU_EDITOR_EXPORT NoteViewModifiers {
    public:
        enum Role {
            VerticalScroll,
            HorizontalScroll,
            TimeZoom,
            KeyZoom,
            SelectNote,
            ToggleNote,
            ExtendNotes,
            MoveNotes,
            PlainLength,
            FreeLength,
            TakeFromNextNote,
            SelectPoint,
            TogglePoint,
            MovePoints,
            ClearSelection,
            ReplaceBand,
            AddToBand,
            ToggleBand,
            PlainDraw,
            FillDraw,
            DrawPitch,
            ErasePitch,
            DragVibrato,
            DragZoom,
            DragZoomAxisLock,
            DisableNoteSnap,
            PointSnapTime,
            PointSnapPitch,
        };

        enum Scene {
            /// A wheel step
            WheelScene,

            /// A left click on a note, on its body or its end
            NoteClickScene,

            /// A left drag from the body of a note
            NoteDragScene,

            /// A left drag from the end of a note
            NoteEndDragScene,

            /// A left click on a pitch point
            PointClickScene,

            /// A left drag from a pitch point
            PointDragScene,

            /// A left click on no note with the select tool
            BlankClickScene,

            /// A left drag from no note with the select tool
            BandDragScene,

            /// A left click on no note with the pen
            DrawClickScene,

            /// A left drag from no note with the pen
            DrawDragScene,

            /// A right click or drag, which selects a span of time
            SpanScene,

            /// A left click or drag with the pitch tool that draws the Mode1 pitch
            PitchDrawScene,

            /// A right click or drag with the pitch tool that erases the Mode1 pitch
            PitchEraseScene,

            /// A left click or drag on a handle of the vibrato of a note
            VibratoDragScene,
        };

        static const ModifierScheme &scheme();
    };

}

#endif // HELLOUTAU_EDITOR_NOTEVIEWMODIFIERS_H
