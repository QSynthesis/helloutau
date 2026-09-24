#ifndef HELLOKIT_EDIT_PROJECTSESSION_H
#define HELLOKIT_EDIT_PROJECTSESSION_H

#include <optional>

#include <QtCore/QList>

#include <hellokit/Document/Project.h>

#include <hellokit/Edit/EditSession.h>
#include <hellokit/Edit/HelloKitEditGlobal.h>

namespace hello::kit {

    /// The editing of one project.
    ///
    /// The session owns the project as a tree of nodes with the slots of ProjectSchema.h. The
    /// tree is the document while the session exists. A \c Project is a snapshot of the tree, for
    /// saving and rendering. The handles in ProjectRefs.h provide the operations of the session
    /// as typed member functions.
    ///
    /// This class adds the operations that require the structure of a project to those of
    /// EditSession, and follows the same rules.
    class HELLOKIT_EDIT_EXPORT ProjectSession : public EditSession {
        Q_OBJECT
    public:
        /// Creates a session that edits a copy of \a project.
        explicit ProjectSession(const Project &project, QObject *parent = nullptr);
        ~ProjectSession();

        /// Returns the project in its current state.
        Project snapshot() const;

        /// Returns a copy of the note \a note, or a default note if \a note is not a note in
        /// the tree.
        Note note(NodeId note) const;

        /// Inserts copies of \a notes into the list of notes \a list before \a index.
        void insert(NodeId list, int index, const QList<Note> &notes);

        /// Inserts copies of \a points into the list of portamento points \a list before
        /// \a index.
        void insert(NodeId list, int index, const QList<PortamentoPoint> &points);

        /// Replaces the Mode1 pitch curve of the note \a note, or removes it if \a pitchBend is
        /// empty.
        void setPitchBend(NodeId note, const std::optional<PitchBend> &pitchBend);
    };

}

#endif // HELLOKIT_EDIT_PROJECTSESSION_H
