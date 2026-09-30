#ifndef HELLOUTAU_THEME_THEMESTYLESHEET_H
#define HELLOUTAU_THEME_THEMESTYLESHEET_H

#include <QtCore/QString>

#include <helloutau/Theme/HelloUtauThemeGlobal.h>

namespace hello::daw {

    /// The converter of the extended style sheet syntax of themes into the syntax of Qt style
    /// sheets. See the section on the extended syntax in docs/Theme.md.
    ///
    /// Strings and comments are copied unchanged, so that a file name such as \c "12px.png" is
    /// not interpreted as a length.
    class HELLOUTAU_THEME_EXPORT ThemeStyleSheet {
    public:
        struct Options {
            /// The directory of the style sheet, against which \c url(@/...) and the \c @/...
            /// files of \c svg(...) are resolved.
            QString directory;

            /// The scale factor of lengths in pixels, and the scale factor of the lengths of
            /// \c font-size.
            double scale = 1;
            double fontScale = 1;
        };

        /// Returns \a text with \c --key converted to \c qproperty-key, \c ---key to \c key,
        /// \c :not(:x) into \c :!x, \c url(@/a) into a path in Options::directory, \c svg(...)
        /// into the \c url(...) of a ThemeIcon, and each \c Npx scaled.
        static QString preprocess(QStringView text, const Options &options);
    };

}

#endif // HELLOUTAU_THEME_THEMESTYLESHEET_H
