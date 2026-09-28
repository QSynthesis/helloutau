#ifndef HELLOUTAU_THEME_THEMESTYLESHEET_H
#define HELLOUTAU_THEME_THEMESTYLESHEET_H

#include <QtCore/QString>

#include <helloutau/Theme/HelloUtauThemeGlobal.h>

namespace hello::daw {

    /// Converts the extended style sheet syntax of themes into what Qt reads. See the section on
    /// the extended syntax in docs/Theme.md.
    ///
    /// Strings and comments are copied unchanged, so that a file name such as \c "12px.png" is
    /// not taken for a length.
    class HELLOUTAU_THEME_EXPORT ThemeStyleSheet {
    public:
        struct Options {
            /// The folder of the style sheet, which \c url(@/...) and the files of \c svg(...)
            /// written \c @/... are relative to.
            QString directory;

            /// The factor applied to lengths in pixels, and to those of \c font-size.
            double scale = 1;
            double fontScale = 1;
        };

        /// Returns \a text with \c --key turned into \c qproperty-key, \c ---key into \c key,
        /// \c :not(:x) into \c :!x, \c url(@/a) into a path in Options::directory, \c svg(...)
        /// into the \c url(...) of a ThemeIcon, and each \c Npx scaled.
        static QString preprocess(QStringView text, const Options &options);
    };

}

#endif // HELLOUTAU_THEME_THEMESTYLESHEET_H
