#ifndef HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINTRUST_H
#define HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINTRUST_H

#include <optional>

#include <QtCore/QString>

#include <stdcorelib/support/json.h>

#include <ClassicPluginHost/ClassicPluginHostPluginGlobal.h>

namespace hello::daw {

    class ClassicPlugin;

    /// Trust of the programs of classic plugins.
    ///
    /// The program of a classic plugin runs only after the user has trusted the program, and runs
    /// again without a prompt until its content changes.
    ///
    /// The records are a JSON array in the user data of the plugin settings, at userDataPath,
    /// one for each plugin folder. Each record holds the plugin folder, the path of the program
    /// relative to the folder, and the SHA-256 of the program. Only the SHA-256 is compared. See
    /// the integrity of the trusted programs in docs/ProgramIntegrity.md.
    ///
    /// \code
    ///     "trusted": [{"folder": "C:\\UTAU\\plugins\\Foo", "relativePath": "foo.exe",
    ///                  "sha256": "<SHA-256>"}]
    /// \endcode
    class CLASSICPLUGINHOST_EXPORT ClassicPluginTrust {
    public:
        /// The path of the records in the user data of the plugin settings, under the ID of
        /// this plugin.
        static constexpr char userDataPath[] = "org.helloutau.classicpluginhost/trusted";

        enum State {
            Trusted,
            /// The plugin folder has no record.
            New,
            /// The program has changed since it was trusted.
            Changed,
            /// The program cannot be read, so that its content is unknown.
            Unreadable,
        };

        /// Returns the state of the program of \a plugin in \a records.
        static State stateOf(const stdc::json::Array &records, const ClassicPlugin &plugin);

        /// Returns the reason why the program of \a plugin cannot be trusted, or
        /// \c std::nullopt if it can: the program cannot be read, or users other than the
        /// current one can replace it (kit::FilePermissions::writableByOthers()).
        static std::optional<QString> refusalOf(const ClassicPlugin &plugin);

        /// Returns \a records with trust in the program of \a plugin recorded, in place of an
        /// earlier record of the plugin folder. The records of the programs that no longer exist
        /// in a plugin folder that exists are removed, while a record of a plugin folder that
        /// cannot be reached, such as on a disconnected drive, is kept.
        ///
        /// \return the records, or \c std::nullopt if the program cannot be read
        static std::optional<stdc::json::Array> trusted(stdc::json::Array records,
                                                        const ClassicPlugin &plugin);
    };

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINTRUST_H
