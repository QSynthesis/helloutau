#include "CommandPaletteSettings.h"

#include "AppSettings_p.h"

namespace hello::daw {

    namespace {

        constexpr char KeyRecentCommands[] = "commandPalette/recent";

    }

    CommandPaletteSettings::CommandPaletteSettings(AppSettingsStore &store) : m_store(store) {
    }

    CommandPaletteSettings::~CommandPaletteSettings() = default;

    QStringList CommandPaletteSettings::recentCommands() const {
        return AppSettingsStore::stringsOf(m_store.value(KeyRecentCommands));
    }

    void CommandPaletteSettings::addRecentCommand(const QString &id) {
        auto ids = recentCommands();
        ids.removeAll(id);
        ids.prepend(id);
        m_store.setValue(KeyRecentCommands,
                         AppSettingsStore::arrayOf(ids.mid(0, recentCommandCount)));
    }

}
