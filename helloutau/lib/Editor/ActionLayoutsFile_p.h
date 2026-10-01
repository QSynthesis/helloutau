#ifndef HELLOUTAU_EDITOR_ACTIONLAYOUTSFILE_P_H
#define HELLOUTAU_EDITOR_ACTIONLAYOUTSFILE_P_H

#include <QtCore/QString>

namespace QAK {
    class ActionRegistry;
}

namespace hello::daw {

    /// The file of the changes that the user has made to the menus and the tool bars,
    /// \c actionLayouts.json beside \c settings.json, apart from the settings, the plugin
    /// settings and the keymap. It holds the changes to the default layouts as QActionKit
    /// records them, see \c QAK::ActionLayoutChange:
    ///
    ///     {"changes": [{"kind": "add", "container": ..., "entry": {...}, ...}, ...]}
    ///
    /// The registry replays the changes on the layouts of the extensions registered at any
    /// time, so that a plugin registered later finds them applied. See the menus and tool bars
    /// in the settings dialog in docs/Widgets.md.
    class ActionLayoutsFile {
    public:
        /// Returns the path of \c actionLayouts.json in the directory of \a settingsFile.
        static QString fileNameFor(const QString &settingsFile);

        /// Gives \a registry the changes that \a fileName records. A missing file records none.
        /// A file that does not read as a list of changes is ignored with a warning, and a
        /// change that does not read is skipped with a warning, so that the default layouts
        /// apply in their place.
        static void read(QAK::ActionRegistry *registry, const QString &fileName);

        /// Writes the changes of \a registry to \a fileName, replacing it at once. Returns
        /// whether the file is written, with the reason in \a error otherwise.
        static bool write(const QAK::ActionRegistry *registry, const QString &fileName,
                          QString *error);
    };

}

#endif // HELLOUTAU_EDITOR_ACTIONLAYOUTSFILE_P_H
