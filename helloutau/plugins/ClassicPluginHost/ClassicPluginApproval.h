#ifndef HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINAPPROVAL_H
#define HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINAPPROVAL_H

#include <stdcorelib/support/json.h>

#include <ClassicPluginHost/ClassicPluginHostPluginGlobal.h>

namespace hello::daw {

    class ClassicPlugin;

    /// The records of the programs of classic plugins that the user has approved to run.
    ///
    /// The records are a JSON array in the user data of the plugin settings, at userDataPath.
    /// Each record holds the plugin folder, the path of the program relative to the folder for
    /// human readers, and the SHA-256 of the program. Only the SHA-256 is compared.
    ///
    /// \code
    ///     "approved": [{"folder": "C:\\UTAU\\plugins\\Foo", "relativePath": "foo.exe",
    ///                   "sha256": "<SHA-256>"}]
    /// \endcode
    class CLASSICPLUGINHOST_EXPORT ClassicPluginApproval {
    public:
        /// The path of the records in the user data of the plugin settings, under the ID of
        /// this plugin.
        static constexpr char userDataPath[] = "org.helloutau.classicpluginhost/approved";

        enum State {
            Approved,
            /// The plugin folder has no record.
            New,
            /// The program has changed since its approval.
            Changed,
        };

        /// Returns the state of the program of \a plugin in \a records.
        static State stateOf(const stdc::json::Array &records, const ClassicPlugin &plugin);

        /// Returns \a records with the program of \a plugin approved. The record of the plugin
        /// folder is replaced if present.
        static stdc::json::Array approved(stdc::json::Array records, const ClassicPlugin &plugin);
    };

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINAPPROVAL_H
