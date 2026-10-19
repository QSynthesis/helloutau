#ifndef HELLOUTAU_WIDGETS_CANVASCOLORS_H
#define HELLOUTAU_WIDGETS_CANVASCOLORS_H

#include <QtGui/QColor>

namespace hello::daw {

    /// The colors of the editing surfaces, such as the piano roll, the keyboard, the ruler and the
    /// waveform views, unless a theme sets the color properties of the widget. They are those of
    /// the light palette of the style windows11 of Qt 6.11, so that a surface is drawn alike on
    /// every system and in the dark appearance of the system, until a theme provides dark colors.
    /// The accent colors, Highlight, HighlightedText and Link, are taken from the palette, because
    /// they do not change with the appearance.
    struct CanvasColors {
        static constexpr QRgb window = 0xfff3f3f3;        ///< Background of a bar, such as a ruler
        static constexpr QRgb base = 0xffffffff;          ///< Background
        static constexpr QRgb alternateBase = 0x09000000; ///< Alternate rows, over the background
        static constexpr QRgb text = 0xe4000000;          ///< Text and lines in front
        static constexpr QRgb placeholderText = 0x80000000; ///< Text in place of absent content
        static constexpr QRgb midlight = 0xffffffff;        ///< Light lines
        static constexpr QRgb mid = 0xffa0a0a0;             ///< Grid lines and frames
        static constexpr QRgb dark = 0xff787878;            ///< Dark lines
    };

}

#endif // HELLOUTAU_WIDGETS_CANVASCOLORS_H
