#ifndef HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINEXCHANGE_H
#define HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINEXCHANGE_H

#include <filesystem>
#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>
#include <QtCore/QCoreApplication>

#include <hellokit/Document/Project.h>
#include <hellokit/Support/Diagnostic.h>

#include <ClassicPluginHost/ClassicPluginHostPluginGlobal.h>

namespace hello::kit {
    class NoteListRef;
    class VoiceBank;
}

namespace hello::daw {

    class ClassicPlugin;

    /// The two project-related parts of a ClassicPlugin run: writing the temporary file for the
    /// plugin, and modifying the track according to the file that the plugin writes back.
    ///
    /// The file is written and applied as UTAU writes and applies it. See
    /// docs/claude/utau-plugin-protocol.md.
    class CLASSICPLUGINHOST_EXPORT ClassicPluginExchange {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::ClassicPluginExchange)
    public:
        /// The absolute paths in the settings of the file. A path is empty if the project has
        /// no such path, as for an unsaved project.
        struct Paths {
            std::filesystem::path project;
            std::filesystem::path voiceDirectory;
            std::filesystem::path cacheDirectory;
        };

        /// Returns the temporary file for \a plugin with the \a count notes of the track of
        /// \a project from index \a first , or with every note if the plugin receives the whole
        /// track.
        ///
        /// Each note contains the values computed by the synthesis with \a voiceBank , which
        /// may be null: the pre-utterance, overlap, and start point, and the file and alias of
        /// the sample.
        static QByteArray input(const ClassicPlugin &plugin, const kit::Project &project, int first,
                                int count, const Paths &paths, const kit::VoiceBank *voiceBank);

        enum Outcome {
            Applied,  ///< the modifications form one undo step
            Canceled, ///< the file contains no note section
            Failed,   ///< the modifications were rolled back, see the diagnostics
        };

        /// Applies \a result , the file written back by \a plugin , to the \a count notes of
        /// \a notes from index \a first , as passed to input(), in one transaction.
        ///
        /// The sections are applied in order of appearance. A numbered section applies to the
        /// next note of the selection regardless of its number. An entry omitted from a section
        /// is unchanged, and an entry with an empty value is removed, which restores the
        /// default. An inserted note that omits the length, lyric, or note number copies it
        /// from the following note. A section beyond the selection is ignored with a warning.
        static Outcome apply(const ClassicPlugin &plugin, const kit::NoteListRef &notes, int first,
                             int count, QByteArrayView result, kit::DiagnosticList &diagnostics);

        /// A project with the result of a plugin applied, for a preview without changes to the
        /// document. See docs/PluginPlay.md.
        struct Preview {
            kit::Project project;

            /// The selection in \a project , shifted by insertions and deletions
            int first = 0;
            int count = 0;
        };

        /// Returns \a project with \a result applied to its \a count notes from index \a first ,
        /// as apply() applies it to a document. The project is unchanged if \a result contains no
        /// note section.
        ///
        /// \return the project and its selection, or \c std::nullopt if the modifications fail,
        ///         with the reason in \a diagnostics
        static std::optional<Preview> preview(const ClassicPlugin &plugin,
                                              const kit::Project &project, int first, int count,
                                              QByteArrayView result,
                                              kit::DiagnosticList &diagnostics);
    };

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINEXCHANGE_H
