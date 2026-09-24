#ifndef HELLOKIT_EDIT_EDITSESSION_H
#define HELLOKIT_EDIT_EDITSESSION_H

#include <memory>

#include <hellokit/Document/Project.h>

#include <hellokit/Edit/HelloKitEditGlobal.h>

namespace hello::kit {

    /// The editing of one project.
    ///
    /// The session owns the project as a tree of nodes, see docs/Editing.md. The tree is the
    /// document while the session exists. A \c Project is a snapshot of the tree, for saving and
    /// rendering.
    class HELLOKIT_EDIT_EXPORT EditSession {
    public:
        /// Creates a session that edits a copy of \a project.
        explicit EditSession(const Project &project);
        ~EditSession();

        EditSession(const EditSession &) = delete;
        EditSession &operator=(const EditSession &) = delete;

        /// Returns the project in its current state.
        Project snapshot() const;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOKIT_EDIT_EDITSESSION_H
