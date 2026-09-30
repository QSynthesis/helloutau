#ifndef HELLOUTAU_INTERCHANGE_IMPORTMERGE_H
#define HELLOUTAU_INTERCHANGE_IMPORTMERGE_H

#include <optional>

#include <QtCore/QCoreApplication>

#include <hellokit/Support/Diagnostic.h>

#include <Interchange/InterchangePluginGlobal.h>

namespace hello::kit {
    struct Project;
    class ProjectRef;
}

namespace hello::daw {

    /// Inserts the notes of an imported project into the track of a project. This is the
    /// application layer of an import: the insertion position and the handling of the imported
    /// settings. See docs/ImportExport.md.
    class INTERCHANGEPLUGIN_EXPORT ImportMerge {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::ImportMerge)
    public:
        /// Insertion position relative to the selection.
        enum Position {
            After,   ///< after the selection
            Before,  ///< before the selection
            Replace, ///< in place of the selection
            AtEnd,   ///< after the last note, for a project without a selection
        };

        struct Options {
            Position position = AtEnd;

            /// The selection: \c count notes from index \c first. Ignored by \c AtEnd.
            int first = 0;
            int count = 0;

            /// If true, the inserted notes keep the tempo of the imported project. Otherwise
            /// they play at the tempo of the insertion position.
            bool keepTempo = true;

            /// If true, the rests before the first sounding note are kept, which preserves the
            /// bar positions of the imported notes.
            bool keepLeadingRest = false;
        };

        /// Inserted notes: \c count notes from index \c first.
        struct Range {
            int first = 0;
            int count = 0;
        };

        /// Inserts the notes of the first track of \a imported into the first track of
        /// \a project at the position of \a options, in one transaction.
        ///
        /// The settings of the imported project and of its track are discarded, and a
        /// diagnostic of severity \c Note lists them. If the tempo is kept and the tempo of the
        /// imported project differs from the tempo at the insertion position, the first inserted
        /// note records the imported tempo. The note after the inserted notes retains its
        /// previous tempo in every case. Pitch data of the other pitch mode than that of
        /// \a project is removed, with a warning.
        ///
        /// \return the inserted notes, an empty range if no note remains to insert, or
        ///         \c std::nullopt if the project was not modified because of an error reported
        ///         in \a diagnostics
        static std::optional<Range> merge(const kit::Project &imported,
                                          const kit::ProjectRef &project, const Options &options,
                                          kit::DiagnosticList &diagnostics);
    };

}

#endif // HELLOUTAU_INTERCHANGE_IMPORTMERGE_H
