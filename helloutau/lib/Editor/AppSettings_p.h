#ifndef HELLOUTAU_EDITOR_APPSETTINGS_P_H
#define HELLOUTAU_EDITOR_APPSETTINGS_P_H

#include <string_view>

#include <QtCore/QString>

#include <stdcorelib/support/json.h>

#include "AppSettings.h"

namespace hello::daw {

    /// The content of the file, in the JSON of stdcorelib, whose values change in place.
    ///
    /// Each key is the path of a value in the groups of the file, the names joined by slashes:
    ///
    ///     {"engines": {"utauDirectory": ..., "resampler": ..., "wavtool": ...},
    ///      "playback": {"mode": ...},
    ///      "files": {"ustExportCharset": ..., "recent": [...], "recentVoiceBanks": [...]},
    ///      "commandPalette": {"recent": [...]},
    ///      "plugins": {"enabledPlugins": [...], "disabledPlugins": [...],
    ///                  "userData": {<plugin ID>: {...}}}}
    class AppSettings::Impl {
    public:
        explicit Impl(const QString &fileName);

        /// The value at \a key, null if there is none.
        const stdc::json::Value &value(std::string_view key) const;

        /// Replaces the value at \a key, creating its groups, or removes it if \a value is null,
        /// with each group that it leaves empty, and writes the file.
        void setValue(std::string_view key, stdc::json::Value value);

        /// Writes the whole file.
        void save() const;

        QString fileName;
        stdc::json::Object root;
    };

}

#endif // HELLOUTAU_EDITOR_APPSETTINGS_P_H
