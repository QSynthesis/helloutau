#ifndef HELLOUTAU_EDITOR_EDITORMODIFIERBINDINGS_H
#define HELLOUTAU_EDITOR_EDITORMODIFIERBINDINGS_H

#include <QtCore/Qt>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// Modifier keys used by the project editor's view interactions.
    struct HELLOUTAU_EDITOR_EXPORT EditorModifierBindings {
        Qt::KeyboardModifiers horizontalScroll = Qt::ShiftModifier;
        Qt::KeyboardModifiers timeZoom = Qt::ControlModifier;
        Qt::KeyboardModifiers keyZoom = Qt::ControlModifier | Qt::ShiftModifier;
        Qt::KeyboardModifiers dragZoom = Qt::ControlModifier | Qt::AltModifier;
        Qt::KeyboardModifiers dragZoomAxisLock = Qt::ShiftModifier;
        Qt::KeyboardModifiers disableNoteSnap = Qt::AltModifier;
        Qt::KeyboardModifiers lockParameterTime = Qt::ShiftModifier;
        Qt::KeyboardModifiers snapParameterValue = Qt::ControlModifier;

        bool operator==(const EditorModifierBindings &other) const {
            return horizontalScroll == other.horizontalScroll && timeZoom == other.timeZoom &&
                   keyZoom == other.keyZoom && dragZoom == other.dragZoom &&
                   dragZoomAxisLock == other.dragZoomAxisLock &&
                   disableNoteSnap == other.disableNoteSnap &&
                   lockParameterTime == other.lockParameterTime &&
                   snapParameterValue == other.snapParameterValue;
        }

        bool operator!=(const EditorModifierBindings &other) const { return !(*this == other); }
    };

}

#endif // HELLOUTAU_EDITOR_EDITORMODIFIERBINDINGS_H
