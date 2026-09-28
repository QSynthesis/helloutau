#ifndef HELLOUTAU_THEME_THEMETYPES_H
#define HELLOUTAU_THEME_THEMETYPES_H

#include <optional>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QList>
#include <QtCore/QMargins>
#include <QtCore/QMetaType>
#include <QtCore/QPointF>
#include <QtCore/QStringList>
#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtGui/QPen>

#include <helloutau/Theme/ThemeStates.h>
#include <helloutau/Theme/ThemeSyntax.h>

namespace hello::daw {

    /// A pen as a style sheet writes it: <tt>qpen(color, width, style, cap, join, dashPattern,
    /// dashOffset, miterLimit, cosmetic)</tt>, the color with button states. The dash pattern and
    /// the widths are in pixels.
    ///
    /// \code
    /// qpen((lightgrey, down=white), 1px, solid, flat, bevel)
    /// qpen(blue, 1px, dash, dashPattern=(2px, 2px), cosmetic=true)
    /// \endcode
    struct HELLOUTAU_THEME_EXPORT ThemePen {
        Q_GADGET
    public:
        ThemeStates<QColor> color = ThemeStates<QColor>(QColor(Qt::black));
        double width = 1;
        Qt::PenStyle style = Qt::SolidLine;
        Qt::PenCapStyle cap = Qt::SquareCap;
        Qt::PenJoinStyle join = Qt::BevelJoin;
        QList<double> dashPattern;
        double dashOffset = 0;
        double miterLimit = 2;
        bool cosmetic = false;

        /// The pen for \a state.
        QPen pen(ThemeButtonState state = ThemeButtonState::Up) const;

        static std::optional<ThemePen> read(const std::vector<ThemeArgument> &arguments,
                                            ThemeError *error);

        bool operator==(const ThemePen &RHS) const;
    };

    /// A font and the color of its text: <tt>qfont(color, size, weight, italic, family)</tt>, the
    /// color with button states. The size is in pixels (\c px) or points (\c pt), the weight a
    /// number from 1 to 1000 or a name from \c thin to \c black, the family a name or a group of
    /// names. What is not written is left as the font it applies to has it.
    ///
    /// \code
    /// qfont(#FFFFFF, 12px, bold)
    /// qfont(#FFFFFF, 15pt, family=("Microsoft YaHei", "SimSun"))
    /// \endcode
    struct HELLOUTAU_THEME_EXPORT ThemeFont {
        Q_GADGET
    public:
        /// Invalid colors if not written.
        ThemeStates<QColor> color;
        int pixelSize = -1;
        double pointSize = -1;
        int weight = -1;
        std::optional<bool> italic;
        QStringList families;

        /// \a base with what this font writes.
        QFont font(const QFont &base = QFont()) const;

        static std::optional<ThemeFont> read(const std::vector<ThemeArgument> &arguments,
                                             ThemeError *error);

        bool operator==(const ThemeFont &RHS) const;
    };

    /// A filled rectangle: <tt>qrect(color, margins, radius)</tt>, the color with button states,
    /// the margins one length for all sides, a group of two (vertical, horizontal) or of four
    /// (left, top, right, bottom), the radius of its corners a length.
    ///
    /// \code
    /// qrect(white, (1px, 2px), 3px)
    /// \endcode
    struct HELLOUTAU_THEME_EXPORT ThemeRect {
        Q_GADGET
    public:
        ThemeStates<QColor> color;
        QMargins margins;
        int radius = 0;

        static std::optional<ThemeRect> read(const std::vector<ThemeArgument> &arguments,
                                             ThemeError *error);

        bool operator==(const ThemeRect &RHS) const;
    };

    /// A drop shadow: <tt>qshadow(color, blur, offset)</tt>, the blur radius a length, the offset
    /// one or two lengths (horizontal, vertical).
    ///
    /// \code
    /// qshadow(#40000000, 16px, 0 4px)
    /// \endcode
    struct HELLOUTAU_THEME_EXPORT ThemeShadow {
        Q_GADGET
    public:
        QColor color;
        int blur = 0;
        QPointF offset;

        bool isVisible() const {
            return color.isValid() && color.alpha() > 0;
        }

        static std::optional<ThemeShadow> read(const std::vector<ThemeArgument> &arguments,
                                               ThemeError *error);

        bool operator==(const ThemeShadow &RHS) const;
    };

    /// Registers the conversions by which a style sheet assigns the types above to properties.
    ///
    /// Qt passes a value written \c qpen(...) to \c setProperty as the list of its name and the
    /// text inside its parentheses, and a plain word as a string; see the section on assigning
    /// custom types in docs/Theme.md. A value that cannot be read is reported with qCWarning and
    /// leaves the property as it was.
    class HELLOUTAU_THEME_EXPORT ThemeTypes {
    public:
        /// Idempotent.
        static void registerConversions();
    };

}

#endif // HELLOUTAU_THEME_THEMETYPES_H
