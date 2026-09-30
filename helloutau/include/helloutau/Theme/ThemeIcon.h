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

    /// An icon drawn from SVG files in the colors of a theme: <tt>svg(file, color)</tt>. Both
    /// arguments support button states. The color replaces \c currentColor in the file, and its
    /// alpha channel sets the opacity of the whole icon. \c auto, or an omitted color, selects
    /// the text color of the control that displays the icon.
    ///
    /// \code
    /// svg("@/play.svg", (#333333, over=#000000, disabled=#999999))
    /// svg(("@/play.svg", up2="@/pause.svg"), auto)
    /// \endcode
    ///
    /// A style sheet assigns the icon to a property of type QIcon, as in
    /// <tt>qproperty-icon: svg(...)</tt>. ThemeStyleSheet converts the value into a \c url(...)
    /// whose file name encodes the icon and ends in \c .svgx (see fileName()). QIcon selects the
    /// icon engine of this library by that suffix, and the engine draws the icon in the state
    /// that QIcon passes: QIcon::Active and QIcon::Selected as \c over, QIcon::Disabled as
    /// \c disabled, and QIcon::On as checked. A control that tracks further states, such as the
    /// pressed state, draws the icon returned by forState() instead.
    struct HELLOUTAU_THEME_EXPORT ThemeIcon {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::ThemeIcon)
    public:
        /// The paths of the SVG files.
        ThemeStates<QString> files;

        /// The colors that replace \c currentColor. An invalid color selects the text color.
        ThemeStates<QColor> colors;

        /// Returns whether no state has a file.
        bool isNull() const;

        /// Returns the file name that encodes the icon. The name ends in \c .svgx, and QIcon
        /// passes it to the icon engine of this library.
        QString fileName() const;

        /// Returns the icon that \a fileName encodes, or \c std::nullopt if \a fileName is not a
        /// name returned by fileName().
        static std::optional<ThemeIcon> fromFileName(QStringView fileName);

        /// Returns the icon that \a icon draws, or \c std::nullopt if the engine of \a icon is
        /// not the icon engine of this library.
        static std::optional<ThemeIcon> of(const QIcon &icon);

        /// Returns a QIcon that draws this icon.
        QIcon icon() const;

        /// Returns \a icon drawn in \a state regardless of the mode and state that QIcon passes,
        /// with \a text as the color of the states that select the text color. Returns \a icon
        /// unchanged if its engine is not the icon engine of this library.
        static QIcon forState(const QIcon &icon, ThemeButtonState state,
                              const QColor &text = QColor());

        /// Clears the cached file contents and rendered images, so that subsequent drawing reads
        /// the current files.
        static void clearCache();

        static std::optional<ThemeIcon> read(const std::vector<ThemeArgument> &arguments,
                                             ThemeError *error);

        bool operator==(const ThemeIcon &RHS) const;
    };

}

#endif // HELLOUTAU_THEME_THEMEICON_H
