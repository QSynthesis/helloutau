#ifndef HELLOUTAU_THEME_THEMEREADER_H
#define HELLOUTAU_THEME_THEMEREADER_H

#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtGui/QColor>

#include <helloutau/Theme/ThemeSyntax.h>

namespace hello::daw {

    /// Reads the basic values of the value syntax, each from a ThemeValue, reporting a value of
    /// the wrong form in \a error.
    class HELLOUTAU_THEME_EXPORT ThemeReader {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::ThemeReader)
    public:
        /// \c #RGB, \c #RRGGBB, \c #AARRGGBB, a color name as the Qt style sheet accepts it, or
        /// \c rgb(), \c rgba(), \c hsv(), \c hsva(), \c hsl(), \c hsla() with numbers or
        /// percentages, the alpha of the \c a forms from 0 to 255 or a percentage.
        static std::optional<QColor> color(const ThemeValue &value, ThemeError *error = nullptr);

        /// A length in pixels, such as \c 2px, or 0.
        static std::optional<int> pixels(const ThemeValue &value, ThemeError *error = nullptr);

        /// Two lengths, or one for both: \c 2px \c 3px.
        static std::optional<QSize> size(const ThemeValue &value, ThemeError *error = nullptr);

        static std::optional<double> number(const ThemeValue &value, ThemeError *error = nullptr);
        static std::optional<int> integer(const ThemeValue &value, ThemeError *error = nullptr);

        /// \c true or \c false.
        static std::optional<bool> boolean(const ThemeValue &value, ThemeError *error = nullptr);

        /// A string, or a word taken as text.
        static std::optional<QString> text(const ThemeValue &value, ThemeError *error = nullptr);
    };

}

#endif // HELLOUTAU_THEME_THEMEREADER_H
