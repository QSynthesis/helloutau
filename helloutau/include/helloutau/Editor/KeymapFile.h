#ifndef HELLOUTAU_EDITOR_KEYMAPFILE_H
#define HELLOUTAU_EDITOR_KEYMAPFILE_H

#include <utility>

#include <QtCore/QList>
#include <QtCore/QString>

#include <helloutau/Editor/EditorModifierBindings.h>
#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace QAK {
    class ActionRegistry;
}

namespace hello::daw {

    /// The file of the shortcuts that the user has assigned, \c keymap.json beside
    /// \c settings.json, apart from the settings and the plugin settings. It has a section for
    /// the action registry of each kind of window, which holds only the commands whose shortcuts
    /// differ from those of their manifests:
    ///
    ///     {"projectWindow": {"shortcuts": [{"id": "helloutau.edit.undo", "keys": ["Ctrl+Z"]}]},
    ///      "voiceBankWindow": {"shortcuts": [...]}}
    ///
    /// An empty list of keys leaves the command without a shortcut. See the keymap in the
    /// settings dialog in docs/Widgets.md.
    class HELLOUTAU_EDITOR_EXPORT KeymapFile {
    public:
        /// The key of a section and the registry whose shortcuts it holds
        using Sections = QList<std::pair<QString, QAK::ActionRegistry *>>;

        /// Returns the path of \c keymap.json in the directory of \a settingsFile.
        static QString fileNameFor(const QString &settingsFile);

        /// Gives the registry of each of \a sections the shortcuts that its section of
        /// \a fileName assigns. A missing file or section assigns none. A file or a section that
        /// does not read as a keymap is ignored with a warning, so that the defaults apply.
        static void read(const Sections &sections, const QString &fileName,
                         EditorModifierBindings *modifiers = nullptr);

        /// Writes the shortcuts that the registry of each of \a sections overrides to its
        /// section of \a fileName, replacing the file at once. Returns whether the file is
        /// written, with the reason in \a error otherwise.
        static bool write(const Sections &sections, const QString &fileName, QString *error,
                          const EditorModifierBindings *modifiers = nullptr);
    };

}

#endif // HELLOUTAU_EDITOR_KEYMAPFILE_H
