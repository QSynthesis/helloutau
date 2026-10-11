#ifndef HELLOUTAU_EDITOR_SETTINGS_APPEARANCESETTINGS_H
#define HELLOUTAU_EDITOR_SETTINGS_APPEARANCESETTINGS_H

#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QList>
#include <QtCore/QLocale>
#include <QtCore/QString>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    class AppSettingsStore;

    /// The appearance of the interface, the group \c appearance of the settings file.
    class HELLOUTAU_EDITOR_EXPORT AppearanceSettings {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::AppearanceSettings)
    public:
        ~AppearanceSettings();

        /// The language of the interface, as a locale name such as \c zh_CN, or empty for that of
        /// the system, the default. It takes effect at the next start, when AppLoader installs
        /// the translations of the language.
        QString language() const;
        void setLanguage(const QString &language);

        /// Returns the languages that the settings offer: the locale name that language()
        /// records, and the name of the language in itself. The first has an empty locale name
        /// and stands for the language of the system.
        static QList<std::pair<QString, QString>> languages();

        /// Returns the locale of \a language, that of the system if \a language is empty.
        static QLocale localeOf(const QString &language);

    private:
        explicit AppearanceSettings(AppSettingsStore &store);

        AppSettingsStore &m_store;

        friend class AppSettings;
        Q_DISABLE_COPY(AppearanceSettings)
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGS_APPEARANCESETTINGS_H
