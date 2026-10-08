#ifndef HELLOUTAU_EDITOR_NOTEVIEWMODIFIERS_H
#define HELLOUTAU_EDITOR_NOTEVIEWMODIFIERS_H

#include <helloutau/Widgets/ModifierBindings.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// The modifier scheme \c noteView of the note area of the piano roll. The ruler above the
    /// piano roll has no scheme of its own and snaps the playhead with DisableNoteSnap.
    class HELLOUTAU_EDITOR_EXPORT NoteViewModifiers {
    public:
        enum Role {
            HorizontalScroll,
            TimeZoom,
            KeyZoom,
            DragZoom,
            DragZoomAxisLock,
            DisableNoteSnap,
        };

        static const ModifierScheme &scheme();
    };

}

#endif // HELLOUTAU_EDITOR_NOTEVIEWMODIFIERS_H
