#ifndef HELLOUTAU_THEME_THEMEMANAGER_H
#define HELLOUTAU_THEME_THEMEMANAGER_H

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QStringList>

#include <helloutau/Theme/HelloUtauThemeGlobal.h>

class QWidget;

namespace hello::daw {

    /// The themes found in theme description files, and the widgets that follow the current one.
    ///
    /// A description file is a \c *.res.json anywhere under a search path. It maps widget
    /// identifiers to namespaces, gives each theme a style sheet per namespace, as a file or
    /// inline, and each theme its variables, of which \c _base names the theme it extends. The
    /// theme \c _common applies before any other. See the section on the organization of themes
    /// in docs/Theme.md for the format.
    ///
    /// A widget is installed with its identifiers. Its style sheet is assembled from the
    /// namespaces of those identifiers, along the chain of themes from the most basic to the
    /// current one, in ascending priority, with \c ${name} replaced by variables and the extended
    /// syntax converted, see ThemeStyleSheet. It is assembled again when the theme, the scale or
    /// the files change, once per series of changes.
    class HELLOUTAU_THEME_EXPORT ThemeManager : public QObject {
        Q_OBJECT
    public:
        explicit ThemeManager(QObject *parent = nullptr);
        ~ThemeManager() override;

        /// Adds \a directory to the folders searched for description files, and reads them.
        void addSearchPath(const QString &directory);

        /// Reads the description files and the style sheet files again.
        void reload();

        /// The themes that the files define, without \c _common, sorted.
        QStringList themes() const;

        QString currentTheme() const;
        void setCurrentTheme(const QString &theme);

        /// The factor applied to lengths in pixels, and the one of font sizes.
        double scale() const;
        void setScale(double scale);
        double fontScale() const;
        void setFontScale(double scale);

        /// Makes \a widget follow the current theme under \a ids, until it is destroyed.
        void install(QWidget *widget, const QStringList &ids);

        /// The style sheet of a widget with \a ids in the current theme.
        QString styleSheet(const QStringList &ids) const;

    Q_SIGNALS:
        void currentThemeChanged();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_THEME_THEMEMANAGER_H
