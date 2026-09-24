#ifndef HELLOKIT_EDIT_NODEREF_H
#define HELLOKIT_EDIT_NODEREF_H

#include <hellokit/Edit/EditSession.h>
#include <hellokit/Edit/Slot.h>

namespace hello::kit {

    /// A handle of a node of an edit session, the base of the typed handles such as NoteRef.
    ///
    /// A handle holds the session and the identifier of the node, and can be copied and stored.
    /// Its member functions call the functions of EditSession with the identifier, therefore the
    /// rules of EditSession apply: reading a node that is not in the tree returns default
    /// values, and modifying it is a programming error. isValid() returns whether the node is in
    /// the tree. A handle of a removed node becomes valid again if the removal is undone, because
    /// the identifier of a node does not change. The session must outlive its handles.
    ///
    /// A handle refers to a node as a pointer does, therefore a const handle can modify the node.
    class NodeRef {
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
        template <class T>
        inline T get(Slot<T> slot) const {
            return m_session ? m_session->value(m_id, slot) : T();
        }

        template <class T>
        inline void set(Slot<T> slot, const typename Slot<T>::ValueType &value) const {
            m_session->setValue(m_id, slot, value);
        }

        template <class Ref>
        inline Ref child(ChildSlot slot) const {
            return Ref(m_session, m_session ? m_session->child(m_id, slot) : 0);
        }

        template <class Ref>
        inline Ref item(int index) const {
            return Ref(m_session, m_session ? m_session->at(m_id, index) : 0);
        }

        inline int count() const {
            return m_session ? m_session->size(m_id) : 0;
        }

        EditSession *m_session = nullptr;
        NodeId m_id = 0;
    };

}

#endif // HELLOKIT_EDIT_NODEREF_H
