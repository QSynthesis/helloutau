#ifndef HELLOKIT_EDIT_NODEREF_H
#define HELLOKIT_EDIT_NODEREF_H

#include <hellokit/Edit/EditSession.h>
#include <hellokit/Edit/HelloKitEditGlobal.h>
#include <hellokit/Edit/Slot.h>

namespace hello::kit {

    /// A handle of a node of an edit session, the base of the handles of a document such as
    /// NoteRef.
    ///
    /// A handle holds the session and the identifier of the node, and can be copied and stored.
    /// Reading through a handle of a node that is not in the tree returns default values, and
    /// modifying through it is a programming error, see EditSession. isValid() returns whether
    /// the node is in the tree. A handle of a removed node becomes valid again if the removal is
    /// undone, because the identifier of a node does not change. The session must outlive its
    /// handles.
    ///
    /// A handle refers to a node as a pointer does, therefore a const handle can modify the node.
    class HELLOKIT_EDIT_EXPORT NodeRef {
    public:
        inline NodeRef() = default;

        inline NodeRef(EditSession *session, NodeId id) : m_session(session), m_id(id) {
        }

        inline EditSession *session() const {
            return m_session;
        }

        inline NodeId id() const {
            return m_id;
        }

        /// Returns whether the node exists and is in the tree of the session.
        inline bool isValid() const {
            return m_session && m_session->contains(m_id);
        }

        inline bool operator==(const NodeRef &RHS) const {
            return m_session == RHS.m_session && m_id == RHS.m_id;
        }

        inline bool operator!=(const NodeRef &RHS) const {
            return !(*this == RHS);
        }

    protected:
        EditSession *m_session = nullptr;
        NodeId m_id = 0;
    };

}

#endif // HELLOKIT_EDIT_NODEREF_H
