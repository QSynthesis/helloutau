#include "OtoWaveformKeyboardModifiers.h"

#include <QtCore/QCoreApplication>

namespace hello::daw {

    const KeyboardModifierScheme &OtoWaveformKeyboardModifiers::scheme() {
        static const KeyboardModifierScheme scheme(
            "otoWaveform", "hello::daw::OtoWaveformKeyboardModifiers",
            QT_TRANSLATE_NOOP("hello::daw::OtoWaveformKeyboardModifiers", "Waveform Area"),
            {
                {HorizontalScroll, "horizontalScroll",
                 QT_TRANSLATE_NOOP("hello::daw::OtoWaveformKeyboardModifiers",
                                   "Wheel: Scroll Horizontally"),
                 Qt::NoModifier},
                {TimeZoom, "timeZoom",
                 QT_TRANSLATE_NOOP("hello::daw::OtoWaveformKeyboardModifiers",
                                   "Wheel: Zoom Time"),
                 Qt::ControlModifier},
            },
            {
                {WheelScene, {{HorizontalScroll}, {TimeZoom}}},
            });
        return scheme;
    }

}
