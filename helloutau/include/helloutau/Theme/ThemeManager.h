#ifndef HELLOUTAU_THEME_THEMEMANAGER_H
#define HELLOUTAU_THEME_THEMEMANAGER_H

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QStringList>

#include <helloutau/Theme/HelloUtauThemeGlobal.h>

class QWidget;

namespace hello::daw {

    /// The registry of the themes defined in theme description files, and of the widgets styled
    /// by the current theme.
    ///
    /// A description file is a \c *.res.json file at any depth under a search path. It maps
    /// widget identifiers to namespaces, and specifies for each theme a style sheet per
    /// namespace, as a file or inline, and a set of variables. The variable \c _base names the
    /// theme that a theme extends. The theme \c _common applies before any other theme. See the
    /// section on the organization of themes in docs/Theme.md for the format.
    ///
    /// A widget is installed with its identifiers. Its style sheet is assembled from the
    /// namespaces of those identifiers along the chain of themes, from the most basic theme to
    /// the current theme, in ascending priority. Each \c ${name} is replaced by the value of the
    /// variable, and the extended syntax is converted as described in ThemeStyleSheet. The style
    /// sheet is assembled again when the theme, the scale or the files change, once per series
    /// of changes.
    class HELLOUTAU_THEME_EXPORT ThemeManager : public QObject {
        Q_OBJECT
    public:
        explicit ThemeManager(QObject *parent = nullptr);
        ~ThemeManager() override;

        /// Adds \a directory to the search paths of description files, and reloads the
        /// description files.
        void addSearchPath(const QString &directory);

        /// Reads the description files and the style sheet files again.
        void reload();

        /// Returns the names of the themes defined in the description files, excluding
        /// \c _common, in sorted order.
        QStringList themes() const;

        QString currentTheme() const;
        void setCurrentTheme(const QString &theme);

        /// The scale factor of lengths in pixels, and the scale factor of font sizes.
        double scale() const;
        void setScale(double scale);
        double fontScale() const;
        void setFontScale(double scale);

        /// Applies the style sheet of the current theme for the identifiers \a ids to \a widget,
        /// and updates it after each change until \a widget is destroyed.
        void install(QWidget *widget, const QStringList &ids);

        /// Returns the style sheet of a widget with the identifiers \a ids in the current theme.
        QString styleSheet(const QStringList &ids) const;

    Q_SIGNALS:
        void currentThemeChanged();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_THEME_THEMEMANAGER_H
