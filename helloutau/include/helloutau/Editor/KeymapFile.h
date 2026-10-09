#ifndef HELLOUTAU_EDITOR_KEYMAPFILE_H
#define HELLOUTAU_EDITOR_KEYMAPFILE_H

#include <QtCore/QList>
#include <QtCore/QString>

#include <helloutau/Widgets/ModifierBindings.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace QAK {
    class ActionRegistry;
}

namespace hello::daw {

    /// The file of the shortcuts and the modifier keys that the user has assigned,
    /// \c keymap.json beside \c settings.json, apart from the settings and the plugin settings.
    /// It has a section for each kind of window. A section holds the commands of the action
    /// registry of that kind whose shortcuts differ from those of their manifests, and for each
    /// modifier scheme of that kind the roles whose modifiers differ from their defaults:
    ///
    ///     {"projectWindow": {"shortcuts": [{"id": "helloutau.edit.undo", "keys": ["Ctrl+Z"]}],
    ///                        "modifiers": {"noteView": {"timeZoom": ["Alt"]}}},
    ///      "voiceBankWindow": {"shortcuts": [...]}}
    ///
    /// An empty list of keys leaves the command without a shortcut. See the keymap in the
    /// settings dialog in docs/Widgets.md.
    class HELLOUTAU_EDITOR_EXPORT KeymapFile {
    public:
        /// A section of the file
        struct Section {
            QString key;

            /// The registry whose shortcuts the section holds
            QAK::ActionRegistry *registry = nullptr;

            /// The modifier bindings of the kind of window, one for each of its schemes
            QList<ModifierBindings> modifiers;
        };

        using Sections = QList<Section>;

        /// Returns the path of \c keymap.json in the directory of \a settingsFile.
        static QString fileNameFor(const QString &settingsFile);

        /// Gives the registry of each of \a sections the shortcuts that its section of
        /// \a fileName records, and sets the modifiers of its bindings that the section records.
        /// A missing file or section records none. A file or a section that is not a valid
        /// keymap is ignored with a warning, so that the defaults apply. Unknown roles and
        /// modifiers are ignored with a warning. Bindings that are not valid after reading
        /// (ModifierBindings::isValid()) are replaced by the defaults of their scheme with a
        /// warning.
        static void read(Sections &sections, const QString &fileName);

        /// Writes the shortcuts that the registry of each of \a sections overrides and the
        /// modifiers of its bindings that differ from their defaults to its section of
        /// \a fileName, replacing the file at once. Returns whether the file is written, with
        /// the reason in \a error otherwise.
        static bool write(const Sections &sections, const QString &fileName, QString *error);
    };

}

#endif // HELLOUTAU_EDITOR_KEYMAPFILE_H
