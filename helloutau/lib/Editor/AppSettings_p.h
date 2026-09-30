#ifndef HELLOUTAU_EDITOR_APPSETTINGS_P_H
#define HELLOUTAU_EDITOR_APPSETTINGS_P_H

#include <string_view>

#include <QtCore/QString>

#include <stdcorelib/support/json.h>

#include "AppSettings.h"

namespace hello::daw {

    /// The content of the file, in groups:
    ///
    ///     {"engines": {"utauDirectory": ..., "resampler": ..., "wavtool": ...},
    ///      "playback": {"mode": ...},
    ///      "files": {"ustExportCharset": ..., "recent": [...], "recentVoiceBanks": [...]},
    ///      "commandPalette": {"recent": [...]}}
    class AppSettings::Impl {
    public:
        explicit Impl(const QString &fileName);

        /// The value at \a key, null if there is none, see SettingsJson::valueAt().
        const stdc::json::Value &value(std::string_view key) const;

        /// Replaces or removes the value at \a key, see SettingsJson::insertAt(), and writes the
        /// file.
        void setValue(std::string_view key, stdc::json::Value value);

        QString fileName;
        stdc::json::Object root;
    };

}

#endif // HELLOUTAU_EDITOR_APPSETTINGS_P_H
