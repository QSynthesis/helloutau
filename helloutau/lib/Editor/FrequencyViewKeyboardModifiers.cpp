#include "FrequencyViewKeyboardModifiers.h"

#include <QtCore/QCoreApplication>

namespace hello::daw {

    const KeyboardModifierScheme &FrequencyViewKeyboardModifiers::scheme() {
        constexpr auto ctrl = Qt::ControlModifier;
        constexpr auto alt = Qt::AltModifier;
        constexpr auto shift = Qt::ShiftModifier;
        constexpr auto none = Qt::NoModifier;
        static const KeyboardModifierScheme scheme(
            "frequencyView", "hello::daw::FrequencyViewKeyboardModifiers",
            QT_TRANSLATE_NOOP("hello::daw::FrequencyViewKeyboardModifiers", "Frequency Map Area"),
            {
                {VerticalScroll, "verticalScroll",
                 QT_TRANSLATE_NOOP("hello::daw::FrequencyViewKeyboardModifiers",
                                   "Wheel: Scroll Vertically"),
                 none},
                {HorizontalScroll, "horizontalScroll",
                 QT_TRANSLATE_NOOP("hello::daw::FrequencyViewKeyboardModifiers",
                                   "Wheel: Scroll Horizontally"),
                 shift},
                {TimeZoom, "timeZoom",
                 QT_TRANSLATE_NOOP("hello::daw::FrequencyViewKeyboardModifiers",
                                   "Wheel: Zoom Time"),
                 ctrl},
                {KeyZoom, "keyZoom",
                 QT_TRANSLATE_NOOP("hello::daw::FrequencyViewKeyboardModifiers",
                                   "Wheel: Zoom Keys"),
                 ctrl | shift},
                {Draw, "draw",
                 QT_TRANSLATE_NOOP("hello::daw::FrequencyViewKeyboardModifiers", "Drag: Draw"),
                 none},
                {SelectFrames, "selectFrames",
                 QT_TRANSLATE_NOOP("hello::daw::FrequencyViewKeyboardModifiers",
                                   "Drag: Select Frames"),
                 none},
                {Unvoice, "unvoice",
                 QT_TRANSLATE_NOOP("hello::daw::FrequencyViewKeyboardModifiers",
                                   "Right Drag: Unvoice"),
                 none},
                {DragZoom, "dragZoom",
                 QT_TRANSLATE_NOOP("hello::daw::FrequencyViewKeyboardModifiers", "Drag: Zoom"),
                 ctrl | alt},
                {DragZoomAxisLock, "dragZoomAxisLock",
                 QT_TRANSLATE_NOOP("hello::daw::FrequencyViewKeyboardModifiers",
                                   "Zoom: Lock Direction"),
                 shift},
                {DragKey, "dragKey",
                 QT_TRANSLATE_NOOP("hello::daw::FrequencyViewKeyboardModifiers",
                                   "Drag Key Line: Set Key"),
                 none},
            },
            {
                {WheelScene, {{VerticalScroll}, {HorizontalScroll}, {TimeZoom}, {KeyZoom}}},
                {SelectDragScene, {{SelectFrames}, {DragZoom, {DragZoomAxisLock}}}},
                {DrawDragScene, {{Draw}, {DragZoom, {DragZoomAxisLock}}}},
                {RightDragScene, {{Unvoice}}},
                {KeyLineScene, {{DragKey}}},
            });
        return scheme;
    }

}
