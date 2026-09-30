#ifndef HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGIN_H
#define HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGIN_H

#include <filesystem>
#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QList>
#include <QtCore/QString>

#include <hellokit/Support/Diagnostic.h>

#include <ClassicPluginHost/ClassicPluginHostPluginGlobal.h>

namespace hello::daw {

    /// A UTAU plugin: a folder that contains a \c plugin.txt and the program specified in it.
    /// The program edits the selected notes through a temporary file.
    ///
    /// See docs/ClassicPluginHost.md.
    class CLASSICPLUGINHOST_EXPORT ClassicPlugin {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::ClassicPlugin)
    public:
        /// Reads the plugin in \a folder .
        ///
        /// A plugin that supports HelloUtau provides a \c plugin.json in UTF-8. The file
        /// replaces \c plugin.txt entirely, and \c plugin.txt is then not read:
        ///
        /// \code
        ///   {"name": "...", "execute": "plugin.exe", "shell": false, "notes": "selection",
        ///    "charset": "UTF-8"}
        /// \endcode
        ///
        /// \c notes is \c selection or \c all , and defaults to \c selection . \c charset
        /// specifies the encoding of the temporary file and defaults to localCharset(). Other
        /// plugins provide the \c plugin.txt of UTAU in localCharset(). See the plugin section
        /// of docs/note.md.
        ///
        /// A plugin whose program cannot run is still returned, with the reason in
        /// \c unavailableReason , so that a menu can show it as disabled.
        ///
        /// \return the plugin, or \c std::nullopt if the folder contains neither
        /// \c plugin.json nor \c plugin.txt or if the file cannot be read.
        static std::optional<ClassicPlugin> read(const std::filesystem::path &folder,
                                                 kit::DiagnosticList &diagnostics);

        /// Returns the plugins in the immediate subfolders of each of \a directories , ordered
        /// by directory and then by folder name, as UTAU discovers them in its \c plugins
        /// folder.
        static QList<ClassicPlugin> discover(const QList<std::filesystem::path> &directories,
                                             kit::DiagnosticList &diagnostics);

        /// Returns the encoding of \c plugin.txt and of the temporary file. This is the ANSI
        /// code page on Windows, as used by UTAU, and CP932 elsewhere because most plugins are
        /// Japanese. See the plugin section of docs/note.md.
        static QString localCharset();

        inline bool isAvailable() const {
            return unavailableReason.isEmpty();
        }

    public:
        std::filesystem::path folder;

        /// The text of the menu entry, which is the folder name if \c plugin.txt specifies none.
        QString name;

        /// The absolute path of the program to run, inside \c folder , or an empty path if
        /// \c plugin.txt specifies no valid program.
        std::filesystem::path program;

        /// Whether the program is started by the handler of its file type (\c shell=use ), as
        /// with \c ShellExecuteEx , rather than as an executable.
        bool shell = false;

        /// Whether the plugin receives every note of the track rather than the selection
        /// (\c notes ). UTAU does so for any value of the key.
        bool wholeTrack = false;

        /// The encoding of the temporary file and of the result.
        QString charset;

        /// The reason that the plugin cannot run, or an empty string if the plugin is available.
        QString unavailableReason;
    };

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGIN_H
