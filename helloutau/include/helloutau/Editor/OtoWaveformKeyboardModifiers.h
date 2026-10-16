#ifndef HELLOUTAU_EDITOR_OTOWAVEFORMKEYBOARDMODIFIERS_H
#define HELLOUTAU_EDITOR_OTOWAVEFORMKEYBOARDMODIFIERS_H

#include <helloutau/Widgets/KeyboardModifierBindings.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// The modifier scheme \c otoWaveform of the waveform area of the voice bank window, see
    /// OtoWaveformView and the waveform area in docs/VoiceBankEditor.md.
    ///
    /// The wheel scrolls horizontally without modifiers, because the waveform has no vertical
    /// axis to scroll. The zoom defaults to the zoom of the time in NoteViewKeyboardModifiers.
    class HELLOUTAU_EDITOR_EXPORT OtoWaveformKeyboardModifiers {
    public:
        enum Role {
            HorizontalScroll,
            TimeZoom,
        };

        enum Scene {
            /// A wheel step
            WheelScene,
        };

        static const KeyboardModifierScheme &scheme();
    };

}

#endif // HELLOUTAU_EDITOR_OTOWAVEFORMKEYBOARDMODIFIERS_H
