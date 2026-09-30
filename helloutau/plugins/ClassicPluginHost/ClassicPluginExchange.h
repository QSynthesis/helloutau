#ifndef HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINEXCHANGE_H
#define HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINEXCHANGE_H

#include <filesystem>

#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>
#include <QtCore/QCoreApplication>

#include <hellokit/Support/Diagnostic.h>

#include <ClassicPluginHost/ClassicPluginHostPluginGlobal.h>

namespace hello::kit {
    struct Project;
    class NoteListRef;
    class VoiceBank;
}

namespace hello::daw {

    class ClassicPlugin;

    /// The two halves of a run of a ClassicPlugin that concern the project: the temporary file
    /// written for the plugin, and the modification of the track by the file it writes back.
    ///
    /// The file is written as UTAU writes it and applied as UTAU applies it, see
    /// docs/claude/utau-plugin-protocol.md.
    class CLASSICPLUGINHOST_EXPORT ClassicPluginExchange {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::ClassicPluginExchange)
    public:
        /// The absolute paths that the file carries in its settings. A path is empty if the
        /// project has none, such as a project not yet saved.
        struct Paths {
            std::filesystem::path project;
            std::filesystem::path voiceDirectory;
            std::filesystem::path cacheDirectory;
        };

        /// The temporary file for \a plugin with the \a count notes from \a first of the track
        /// of \a project , or with every note if the plugin receives the whole track.
        ///
        /// Each note carries the values that the synthesis computes for it with \a voiceBank ,
        /// which may be null: its pre-utterance, overlap and start point, and the file and the
        /// alias of its sample.
        static QByteArray input(const ClassicPlugin &plugin, const kit::Project &project, int first,
                                int count, const Paths &paths, const kit::VoiceBank *voiceBank);

        enum Outcome {
            Applied,   ///< the modifications form one undo step
            Cancelled, ///< the file has no section of a note
            Failed,    ///< the modifications were rolled back, see the diagnostics
        };

        /// Applies \a result , the file that \a plugin wrote back, to the \a count notes from
        /// \a first of \a notes , as input() wrote them, in one transaction.
        ///
        /// The sections apply in their order: a numbered section to the next note of the
        /// selection, whatever its number. An entry that a section omits stays, and an entry
        /// with an empty value is removed, which restores the default. A note inserted without
        /// a length, a lyric or a note number takes that of the note after it. A section beyond
        /// the selection is ignored with a warning.
        static Outcome apply(const ClassicPlugin &plugin, const kit::NoteListRef &notes, int first,
                             int count, QByteArrayView result, kit::DiagnosticList &diagnostics);
    };

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINEXCHANGE_H
