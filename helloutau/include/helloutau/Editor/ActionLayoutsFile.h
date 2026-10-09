#ifndef HELLOUTAU_EDITOR_ACTIONLAYOUTSFILE_H
#define HELLOUTAU_EDITOR_ACTIONLAYOUTSFILE_H

#include <utility>

#include <QtCore/QList>
#include <QtCore/QString>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace QAK {
    class ActionRegistry;
}

namespace hello::daw {

    /// The file of the changes that the user has made to the menus and the tool bars,
    /// \c actionLayouts.json beside \c settings.json, apart from the settings, the plugin
    /// settings and the keymap. It has a section for the action registry of each kind of window,
    /// which holds the changes to the default layouts as QActionKit records them, see
    /// \c QAK::ActionLayoutChange, and the version of the format:
    ///
    ///     {"version": 1,
    ///      "projectWindow": {"changes": [{"kind": "add", "container": ..., ...}, ...]},
    ///      "voiceBankWindow": {"changes": [...]}}
    ///
    /// The registry replays the changes on the layouts of the extensions registered at any
    /// time, so that a plugin registered later finds them applied. See the menus and tool bars
    /// in the settings dialog in docs/Widgets.md.
    class HELLOUTAU_EDITOR_EXPORT ActionLayoutsFile {
    public:
        /// The key of a section and the registry whose changes it holds
        using Sections = QList<std::pair<QString, QAK::ActionRegistry *>>;

        /// The version of the format that this program reads and writes. It increases whenever
        /// the recorded changes no longer apply as written, for example after actions are
        /// renamed. A file of another version, or without a version, is not read.
        static constexpr int version = 1;

        /// Returns the path of \c actionLayouts.json in the directory of \a settingsFile.
        static QString fileNameFor(const QString &settingsFile);

        /// Gives the registry of each of \a sections the changes that its section of \a fileName
        /// records. A missing file or section records none. A file of another version, or a file
        /// or a section that does not read as a list of changes, is ignored with a warning, and a
        /// change that does not read is skipped with a warning, so that the default layouts apply
        /// in their place.
        static void read(const Sections &sections, const QString &fileName);

        /// Writes the changes of the registry of each of \a sections to its section of
        /// \a fileName, replacing the file at once. Returns whether the file is written, with
        /// the reason in \a error otherwise.
        static bool write(const Sections &sections, const QString &fileName, QString *error);
    };

}

#endif // HELLOUTAU_EDITOR_ACTIONLAYOUTSFILE_H
