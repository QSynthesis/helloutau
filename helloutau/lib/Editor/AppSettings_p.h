#ifndef HELLOUTAU_EDITOR_APPSETTINGS_P_H
#define HELLOUTAU_EDITOR_APPSETTINGS_P_H

#include <string_view>

#include <QtCore/QString>

#include <stdcorelib/support/json.h>

#include "AppSettings.h"
#include "SettingsFile_p.h"

namespace hello::daw {

    /// Content of the file, organized in groups:
    ///
    ///     {"utau": {"directory": ...},
    ///      "synthTools": {"resampler": ..., "wavtool": ...},
    ///      "playback": {"mode": ..., "threads": ...},
    ///      "appearance": {"language": ...},
    ///      "view": {"showPitch": ..., "showRenderedPitch": ..., "showEnvelopes": ...,
    ///               "showParameters": ..., "showToolBar": ..., "quantization": ...},
    ///      "files": {"ustExportCharset": ..., "recent": [...], "recentVoiceBanks": [...]},
    ///      "commandPalette": {"recent": [...]}}
    class AppSettings::Impl {
    public:
        explicit Impl(const QString &fileName);

        /// Returns the value at \a key, or null if absent, see kit::JsonInterop::valueAt().
        const stdc::json::Value &value(std::string_view key) const;

        /// Replaces or removes the value at \a key, see kit::JsonInterop::insertAt(), and writes
        /// the file once the event loop runs.
        void setValue(std::string_view key, stdc::json::Value value);

        stdc::json::Object root;

        // Declared after the content, so that it is destroyed first and writes the pending
        // changes while the content still exists
        SettingsFile file;
    };

}

#endif // HELLOUTAU_EDITOR_APPSETTINGS_P_H
