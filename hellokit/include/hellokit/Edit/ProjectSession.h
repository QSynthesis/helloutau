#ifndef HELLOKIT_EDIT_PROJECTSESSION_H
#define HELLOKIT_EDIT_PROJECTSESSION_H

#include <optional>

#include <QtCore/QJsonObject>

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

        /// Returns the entry of the change log for \a change, or \c std::nullopt for a change
        /// that the log omits, which is ListChange::AboutToBeRemoved. See the section on the
        /// change log in docs/Editing.md.
        ///
        /// The entry names the slot of \a change by its field in \c .usth, and writes values as
        /// in \c .usth. It requires the changed node, therefore it is called while \a change is
        /// reported by changed().
        std::optional<QJsonObject> logEntry(const Change &change) const;
    };

}

#endif // HELLOKIT_EDIT_PROJECTSESSION_H
