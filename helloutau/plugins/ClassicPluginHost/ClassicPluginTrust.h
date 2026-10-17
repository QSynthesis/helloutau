#ifndef HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINTRUST_H
#define HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINTRUST_H

#include <stdcorelib/support/json.h>

#include <ClassicPluginHost/ClassicPluginHostPluginGlobal.h>

namespace hello::daw {

    class ClassicPlugin;

    /// Trust of the programs of classic plugins.
    ///
    /// The program of a classic plugin runs only after the user has trusted the program, and runs
    /// again without a prompt until its content changes.
    ///
    /// The records are a JSON array in the user data of the plugin settings, at userDataPath.
    /// Each record holds the plugin folder, the path of the program relative to the folder for
    /// human readers, and the SHA-256 of the program. Only the SHA-256 is compared.
    ///
    /// \code
    ///     "approved": [{"folder": "C:\\UTAU\\plugins\\Foo", "relativePath": "foo.exe",
    ///                   "sha256": "<SHA-256>"}]
    /// \endcode
    class CLASSICPLUGINHOST_EXPORT ClassicPluginTrust {
    public:
        /// The path of the records in the user data of the plugin settings, under the ID of
        /// this plugin.
        static constexpr char userDataPath[] = "org.helloutau.classicpluginhost/approved";

        enum State {
            Trusted,
            /// The plugin folder has no record.
            New,
            /// The program has changed since it was trusted.
            Changed,
        };

        /// Returns the state of the program of \a plugin in \a records.
        static State stateOf(const stdc::json::Array &records, const ClassicPlugin &plugin);

        /// Returns \a records with trust in the program of \a plugin recorded. The record of
        /// the plugin folder is replaced if present.
        static stdc::json::Array trusted(stdc::json::Array records, const ClassicPlugin &plugin);
    };

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINTRUST_H
