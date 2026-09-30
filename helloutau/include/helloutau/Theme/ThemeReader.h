#ifndef HELLOUTAU_THEME_THEMEREADER_H
#define HELLOUTAU_THEME_THEMEREADER_H

#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtGui/QColor>

#include <helloutau/Theme/ThemeSyntax.h>

namespace hello::daw {

    /// The readers of the basic values of the value syntax. Each reader reads a ThemeValue and
    /// reports a malformed value in \a error.
    class HELLOUTAU_THEME_EXPORT ThemeReader {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::ThemeReader)
    public:
        /// Reads a color: \c #RGB, \c #RRGGBB, \c #AARRGGBB, a color name accepted by Qt style
        /// sheets, or \c rgb(), \c rgba(), \c hsv(), \c hsva(), \c hsl(), \c hsla() with numbers
        /// or percentages. The alpha of the \c a forms is a number from 0 to 255 or a percentage.
        static std::optional<QColor> color(const ThemeValue &value, ThemeError *error = nullptr);

        /// Reads a length in pixels, such as \c 2px, or 0.
        static std::optional<int> pixels(const ThemeValue &value, ThemeError *error = nullptr);

        /// Reads two lengths, such as \c 2px \c 3px, or one length for both dimensions.
        static std::optional<QSize> size(const ThemeValue &value, ThemeError *error = nullptr);

        static std::optional<double> number(const ThemeValue &value, ThemeError *error = nullptr);
        static std::optional<int> integer(const ThemeValue &value, ThemeError *error = nullptr);

        /// Reads \c true or \c false.
        static std::optional<bool> boolean(const ThemeValue &value, ThemeError *error = nullptr);

        /// Reads a string, or a word as text.
        static std::optional<QString> text(const ThemeValue &value, ThemeError *error = nullptr);
    };

}

#endif // HELLOUTAU_THEME_THEMEREADER_H
