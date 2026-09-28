#ifndef HELLOUTAU_THEME_THEMEICON_H
#define HELLOUTAU_THEME_THEMEICON_H

#include <optional>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtGui/QColor>
#include <QtGui/QIcon>

#include <helloutau/Theme/ThemeStates.h>
#include <helloutau/Theme/ThemeSyntax.h>

namespace hello::daw {

    /// An icon drawn from SVG files in the colors of a theme: <tt>svg(file, color)</tt>, both
    /// with button states. The color replaces \c currentColor in the file, its alpha making the
    /// whole icon translucent. \c auto, or no color, follows the text that the icon goes with.
    ///
    /// \code
    /// svg("@/play.svg", (#333333, over=#000000, disabled=#999999))
    /// svg(("@/play.svg", up2="@/pause.svg"), auto)
    /// \endcode
    ///
    /// A style sheet gives it to a property of type QIcon, as in
    /// <tt>qproperty-icon: svg(...)</tt>, which ThemeStyleSheet turns into a \c url(...) whose
    /// file name describes the icon and ends in \c .svgx (fileName()). By that suffix QIcon
    /// chooses the icon engine of this library, which draws the icon in the state that QIcon
    /// passes: QIcon::Active and QIcon::Selected as \c over, QIcon::Disabled as \c disabled,
    /// QIcon::On as checked. A control that knows more, such as whether it is pressed, draws
    /// forState() instead.
    struct HELLOUTAU_THEME_EXPORT ThemeIcon {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::ThemeIcon)
    public:
        /// The paths of the SVG files.
        ThemeStates<QString> files;

        /// The colors that replace \c currentColor; an invalid color follows the text.
        ThemeStates<QColor> colors;

        /// Whether no state has a file.
        bool isNull() const;

        /// The file name that describes the icon, ending in \c .svgx, which QIcon hands to the
        /// icon engine of this library.
        QString fileName() const;

        /// The icon that \a fileName describes, if it is one that fileName() returns.
        static std::optional<ThemeIcon> fromFileName(QStringView fileName);

        /// The icon that \a icon draws, if its engine is the one of this library.
        static std::optional<ThemeIcon> of(const QIcon &icon);

        /// A QIcon that draws this icon.
        QIcon icon() const;

        /// \a icon drawn always in \a state, and where its color follows the text, in \a text;
        /// \a icon itself if its engine is not the one of this library.
        static QIcon forState(const QIcon &icon, ThemeButtonState state,
                              const QColor &text = QColor());

        /// Forgets the files read and the images drawn, so that the icons are drawn again from
        /// the files as they are now.
        static void clearCache();

        static std::optional<ThemeIcon> read(const std::vector<ThemeArgument> &arguments,
                                             ThemeError *error);

        bool operator==(const ThemeIcon &RHS) const;
    };

}

#endif // HELLOUTAU_THEME_THEMEICON_H
