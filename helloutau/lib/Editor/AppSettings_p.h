#ifndef HELLOUTAU_EDITOR_APPSETTINGS_P_H
#define HELLOUTAU_EDITOR_APPSETTINGS_P_H

#include <filesystem>
#include <memory>
#include <string_view>

#include <QtCore/QString>
#include <QtCore/QStringList>

#include <stdcorelib/support/json.h>

#include <hellokit/Support/SettingsFile.h>

#include "AppSettings.h"

namespace hello::daw {

    /// The keys whose values the store normalizes when the file is read and when a value is set
    /// by key: the paths, with native separators, and the lists of recent paths, without
    /// duplicates.
    struct AppSettingsKeys {
        static constexpr char utauDirectory[] = "utau/directory";
        static constexpr char resampler[] = "synthTools/resampler";
        static constexpr char wavtool[] = "synthTools/wavtool";
        static constexpr char recentFiles[] = "files/recent";
        static constexpr char recentVoiceBanks[] = "files/recentVoiceBanks";
    };

    /// The content of the settings file, organized in groups, each read and written by one class
    /// of the \c Settings directory:
    ///
    ///     {"utau": {"directory": ...},
    ///      "voiceBanks": {"relativeToUtau": ...},
    ///      "synthTools": {"resampler": ..., "wavtool": ...},
    ///      "renderLog": {"accumulated": ..., "limit": ...},
    ///      "playback": {"mode": ..., "threads": ...},
    ///      "audio": {"outputDevice": ...},
    ///      "appearance": {"language": ...},
    ///      "view": {"showPitch": ..., "showRenderedPitch": ..., "showEnvelopes": ...,
    ///               "showParameters": ..., "showToolBar": ..., "quantization": ...},
    ///      "files": {"ustExportCharset": ..., "recent": [...], "recentVoiceBanks": [...]},
    ///      "commandPalette": {"recent": [...]},
    ///      "pitchControl": {"portamento": {...}, "vibrato": {...}, "vibratoPreset": ...},
    ///      "updates": {"mode": ..., "source": ..., "lastCheck": ..., "skippedVersion": ...,
    ///                  "extractionThreads": ...}}
    class AppSettingsStore {
    public:
        /// Reads \a fileName, and normalizes the values of AppSettingsKeys.
        explicit AppSettingsStore(const QString &fileName);

        /// Returns the value at \a key, or null if absent, see kit::JsonInterop::valueAt().
        const stdc::json::Value &value(std::string_view key) const;

        /// Replaces or removes the value at \a key, see kit::JsonInterop::insertAt(), and writes
        /// the file once the event loop runs.
        void setValue(std::string_view key, stdc::json::Value value);

        /// Returns the string at \a key, or an empty string if absent.
        QString text(std::string_view key) const;

        /// Returns \a path in the form stored in the settings: normalized, with native
        /// separators.
        static QString pathText(const std::filesystem::path &path);

        /// Returns the path in \a text in the form of pathText(), or an empty string if \a text
        /// is empty.
        static QString normalizedPathText(const QString &text);

        /// Returns the strings of the array \a value.
        static QStringList stringsOf(const stdc::json::Value &value);

        /// Returns an array of \a strings.
        static stdc::json::Value arrayOf(const QStringList &strings);

        /// Returns the paths of the array \a value in the form of pathText(), without empty
        /// paths and duplicates.
        static QStringList recentTextsOf(const stdc::json::Value &value);

        stdc::json::Object root;

        // Declared after the content, so that it is destroyed first and writes the pending
        // changes while the content still exists
        kit::SettingsFile file;
    };

    class AppSettings::Impl {
    public:
        explicit Impl(const QString &fileName) : store(fileName) {
        }

        AppSettingsStore store;
        std::filesystem::path userDirectory;

        std::unique_ptr<UtauSettings> utau;
        std::unique_ptr<VoiceBankSettings> voiceBanks;
        std::unique_ptr<RenderingSettings> rendering;
        std::unique_ptr<AudioSettings> audio;
        std::unique_ptr<AppearanceSettings> appearance;
        std::unique_ptr<ViewSettings> view;
        std::unique_ptr<PitchControlSettings> pitchControl;
        std::unique_ptr<FileSettings> files;
        std::unique_ptr<CommandPaletteSettings> commandPalette;
        std::unique_ptr<UpdateSettings> updates;
    };

}

#endif // HELLOUTAU_EDITOR_APPSETTINGS_P_H
