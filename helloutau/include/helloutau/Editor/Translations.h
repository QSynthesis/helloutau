#ifndef HELLOUTAU_EDITOR_TRANSLATIONS_H
#define HELLOUTAU_EDITOR_TRANSLATIONS_H

#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QList>
#include <QtCore/QLocale>
#include <QtCore/QString>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// The translations of the interface: those of Qt, of the libraries, and of each plugin.
    ///
    /// The translations of the libraries are embedded in HelloUtauEditor under
    /// <tt>:/helloutau/translations</tt>, and those of a plugin in the plugin under
    /// <tt>:/helloutau/plugins/<name>/translations</tt>, each file named
    /// <tt><name>_<locale>.qm</tt>.
    class HELLOUTAU_EDITOR_EXPORT Translations {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::Translations)
    public:
        /// Returns the languages that the settings offer: the locale name that
        /// AppSettings::language() records, and the name of the language in itself. The first
        /// has an empty locale name and stands for the language of the system.
        static QList<std::pair<QString, QString>> languages();

        /// Returns the locale of \a language, that of the system if \a language is empty.
        static QLocale localeOf(const QString &language);

        /// Makes the locale of \a language the default locale and installs the translations of
        /// Qt and of the libraries for it, in place of those that earlier calls and load()
        /// installed.
        ///
        /// \note Called after the application object exists and before any window is created,
        ///       because a translation does not change the texts already shown.
        static void install(const QString &language);

        /// Installs the translation file \a name for the default locale from \a directory, as a
        /// plugin does in its initialization.
        ///
        /// \return Whether a file for the default locale was found and installed. For English,
        ///         the language of the sources, none is installed.
        static bool load(const QString &name, const QString &directory);
    };

}

#endif // HELLOUTAU_EDITOR_TRANSLATIONS_H
