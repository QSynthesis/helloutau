#include "FrequencyViewModifiers.h"

#include <QtCore/QCoreApplication>

namespace hello::daw {

    const ModifierScheme &FrequencyViewModifiers::scheme() {
        constexpr auto ctrl = Qt::ControlModifier;
        constexpr auto alt = Qt::AltModifier;
        constexpr auto shift = Qt::ShiftModifier;
        constexpr auto none = Qt::NoModifier;
        static const ModifierScheme scheme(
            "frequencyView", "hello::daw::FrequencyViewModifiers",
            QT_TRANSLATE_NOOP("hello::daw::FrequencyViewModifiers", "Frequency Map Area"),
            {
                {VerticalScroll, "verticalScroll",
                 QT_TRANSLATE_NOOP("hello::daw::FrequencyViewModifiers",
                                   "Wheel: Scroll Vertically"),
                 none},
                {HorizontalScroll, "horizontalScroll",
                 QT_TRANSLATE_NOOP("hello::daw::FrequencyViewModifiers",
                                   "Wheel: Scroll Horizontally"),
                 shift},
                {TimeZoom, "timeZoom",
                 QT_TRANSLATE_NOOP("hello::daw::FrequencyViewModifiers", "Wheel: Zoom Time"), ctrl},
                {KeyZoom, "keyZoom",
                 QT_TRANSLATE_NOOP("hello::daw::FrequencyViewModifiers", "Wheel: Zoom Keys"),
                 ctrl | shift},
                {Draw, "draw", QT_TRANSLATE_NOOP("hello::daw::FrequencyViewModifiers", "Drag: Draw"),
                 none},
                {SelectFrames, "selectFrames",
                 QT_TRANSLATE_NOOP("hello::daw::FrequencyViewModifiers", "Drag: Select Frames"),
                 ctrl},
                {Unvoice, "unvoice",
                 QT_TRANSLATE_NOOP("hello::daw::FrequencyViewModifiers", "Right Drag: Unvoice"),
                 none},
                {DragZoom, "dragZoom",
                 QT_TRANSLATE_NOOP("hello::daw::FrequencyViewModifiers", "Drag: Zoom"), ctrl | alt},
                {DragZoomAxisLock, "dragZoomAxisLock",
                 QT_TRANSLATE_NOOP("hello::daw::FrequencyViewModifiers", "Zoom: Lock Direction"),
                 shift},
            },
            {
                {WheelScene, {{VerticalScroll}, {HorizontalScroll}, {TimeZoom}, {KeyZoom}}},
                {LeftDragScene, {{Draw}, {SelectFrames}, {DragZoom, {DragZoomAxisLock}}}},
                {RightDragScene, {{Unvoice}}},
            });
        return scheme;
    }

}
