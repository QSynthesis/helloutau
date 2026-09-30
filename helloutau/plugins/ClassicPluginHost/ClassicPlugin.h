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

    /// A plugin of UTAU: a folder with a \c plugin.txt and the program that it names, which
    /// edits the selected notes through a temporary file.
    ///
    /// See docs/ClassicPluginHost.md.
    class CLASSICPLUGINHOST_EXPORT ClassicPlugin {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::ClassicPlugin)
    public:
        /// Reads the plugin in \a folder , or returns \c std::nullopt if the folder has neither
        /// file below or the file cannot be read.
        ///
        /// A plugin that supports HelloUtau has a \c plugin.json in UTF-8, which replaces
        /// \c plugin.txt , none of which is then read:
        ///
        /// \code
        ///   {"name": "...", "execute": "plugin.exe", "shell": false, "notes": "selection",
        ///    "charset": "UTF-8"}
        /// \endcode
        ///
        /// \c notes is \c selection or \c all , \c selection if omitted. \c charset is the
        /// encoding of the temporary file, localCharset() if omitted. Other plugins have the
        /// \c plugin.txt of UTAU in localCharset(). See the plugins in docs/note.md.
        ///
        /// A plugin whose program cannot run is returned all the same, with the reason in
        /// \a unavailableReason , so that a menu can show it disabled.
        static std::optional<ClassicPlugin> read(const std::filesystem::path &folder,
                                                 kit::DiagnosticList &diagnostics);

        /// The plugins in the folders directly inside each of \a directories , as UTAU finds
        /// them in its \c plugins folder: by directory, and by folder name within one.
        static QList<ClassicPlugin> discover(const QList<std::filesystem::path> &directories,
                                             kit::DiagnosticList &diagnostics);

        /// The encoding of \c plugin.txt and of the temporary file: the ANSI code page on
        /// Windows, as UTAU uses, and CP932 elsewhere, since most plugins are Japanese. See the
        /// plugins in docs/note.md.
        static QString localCharset();

        inline bool isAvailable() const {
            return unavailableReason.isEmpty();
        }

    public:
        std::filesystem::path folder;

        /// The text of the menu entry, the name of the folder if \c plugin.txt gives none.
        QString name;

        /// The absolute path of the program to run, inside \a folder . Empty if \c plugin.txt
        /// names none that may run.
        std::filesystem::path program;

        /// Whether the program is started by the handler of its file type (\c shell=use ), as
        /// \c ShellExecuteEx starts it, rather than as a program.
        bool shell = false;

        /// Whether the plugin receives every note of the track rather than the selection
        /// (\c notes ), which UTAU does whatever the value is.
        bool wholeTrack = false;

        /// The encoding of the temporary file, which the result is read back in too.
        QString charset;

        /// Why the plugin cannot run, empty if it can.
        QString unavailableReason;
    };

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGIN_H
