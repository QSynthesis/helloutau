#ifndef HELLOKIT_EDIT_PROJECTSESSION_H
#define HELLOKIT_EDIT_PROJECTSESSION_H

#include <hellokit/Document/Project.h>

#include <hellokit/Edit/EditSession.h>
#include <hellokit/Edit/HelloKitEditGlobal.h>

namespace hello::kit {

    /// The editing of one project.
    ///
    /// The session owns the project as a tree of nodes with the slots of ProjectSchema.h. The
    /// tree is the document while the session exists. A \c Project is a snapshot of the tree, for
    /// saving and rendering. The handles in ProjectRefs.h, obtained from a ProjectRef of the
    /// session, read and modify the tree.
    class HELLOKIT_EDIT_EXPORT ProjectSession : public EditSession {
        Q_OBJECT
    public:
        /// Creates a session that edits a copy of \a project.
        explicit ProjectSession(const Project &project, QObject *parent = nullptr);
        ~ProjectSession();

        /// Returns the project in its current state.
        Project snapshot() const;
    };

}

#endif // HELLOKIT_EDIT_PROJECTSESSION_H
