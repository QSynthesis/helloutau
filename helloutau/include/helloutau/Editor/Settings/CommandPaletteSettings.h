#ifndef HELLOUTAU_EDITOR_SETTINGS_COMMANDPALETTESETTINGS_H
#define HELLOUTAU_EDITOR_SETTINGS_COMMANDPALETTESETTINGS_H

#include <QtCore/QString>
#include <QtCore/QStringList>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    class AppSettingsStore;

    /// The command palette, the group \c commandPalette of the settings file.
    class HELLOUTAU_EDITOR_EXPORT CommandPaletteSettings {
    public:
        ~CommandPaletteSettings();

        /// Returns the commands last chosen in the command palette, most recent first. The list
        /// is shared by all windows.
        QStringList recentCommands() const;

        /// Places \a id first among the recent commands. At most \c recentCommandCount commands
        /// are stored.
        void addRecentCommand(const QString &id);

        static constexpr int recentCommandCount = 20;

    private:
        explicit CommandPaletteSettings(AppSettingsStore &store);

        AppSettingsStore &m_store;

        friend class AppSettings;
        Q_DISABLE_COPY(CommandPaletteSettings)
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGS_COMMANDPALETTESETTINGS_H
