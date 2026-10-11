#ifndef HELLOKIT_SUPPORT_TRANSLATIONLOADER_H
#define HELLOKIT_SUPPORT_TRANSLATIONLOADER_H

#include <memory>
#include <utility>
#include <vector>

#include <QtCore/QLocale>
#include <QtCore/QString>

#include <hellokit/Support/HelloKitSupportGlobal.h>

class QTranslator;

namespace hello::kit {

    /// The translations of the interface in one language, installed in \c QCoreApplication by
    /// name: that of Qt, those of the libraries, and that of each plugin.
    ///
    /// The application creates one instance after the application object, as it creates one
    /// \c QCoreApplication, and before any window, because a translation does not change the
    /// texts already shown. Plugins reach the instance with instance().
    class HELLOKIT_SUPPORT_EXPORT TranslationLoader {
    public:
        /// Makes \a locale the default locale. At most one instance exists at a time.
        explicit TranslationLoader(const QLocale &locale);

        /// Removes the translations that remain installed.
        ~TranslationLoader();

        /// Returns the instance, or \c nullptr if none exists.
        static TranslationLoader *instance();

        inline const QLocale &locale() const {
            return m_locale;
        }

        /// Installs the translation file \a name for locale() from \a directory, the file
        /// <tt><name>_<locale>.qm</tt>, in place of a translation installed earlier under
        /// \a name.
        ///
        /// \return Whether a file for locale() was found and installed. For English, the language
        ///         of the sources, none is installed.
        bool load(const QString &name, const QString &directory);

        /// Removes the translation installed under \a name. A plugin removes its translation
        /// before its library is unloaded, because the translation may refer to the resources of
        /// the library.
        void remove(const QString &name);

    private:
        QLocale m_locale;
        // In the order of installation, which QCoreApplication searches in reverse
        std::vector<std::pair<QString, std::unique_ptr<QTranslator>>> m_translators;

        Q_DISABLE_COPY(TranslationLoader)
    };

}

#endif // HELLOKIT_SUPPORT_TRANSLATIONLOADER_H
