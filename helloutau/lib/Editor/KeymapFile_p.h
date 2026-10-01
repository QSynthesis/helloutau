#ifndef HELLOUTAU_EDITOR_KEYMAPFILE_P_H
#define HELLOUTAU_EDITOR_KEYMAPFILE_P_H

#include <QtCore/QString>

namespace QAK {
    class ActionRegistry;
}

namespace hello::daw {

    /// The file of the shortcuts that the user has assigned, \c keymap.json beside
    /// \c settings.json, apart from the settings and the plugin settings. It holds only the
    /// commands whose shortcuts differ from those of their manifests:
    ///
    ///     {"shortcuts": [{"id": "helloutau.edit.undo", "keys": ["Ctrl+Z"]}, ...]}
    ///
    /// An empty list of keys leaves the command without a shortcut. See the keymap in the
    /// settings dialog in docs/Widgets.md.
    class KeymapFile {
    public:
        /// Returns the path of \c keymap.json in the directory of \a settingsFile.
        static QString fileNameFor(const QString &settingsFile);

        /// Gives \a registry the shortcuts that \a fileName assigns. A missing file assigns none.
        /// A file that does not read as a keymap is ignored with a warning, so that the defaults
        /// apply.
        static void read(QAK::ActionRegistry *registry, const QString &fileName);

        /// Writes the shortcuts that \a registry overrides to \a fileName, replacing it at once.
        /// Returns whether the file is written, with the reason in \a error otherwise.
        static bool write(const QAK::ActionRegistry *registry, const QString &fileName,
                          QString *error);
    };

}

#endif // HELLOUTAU_EDITOR_KEYMAPFILE_P_H
