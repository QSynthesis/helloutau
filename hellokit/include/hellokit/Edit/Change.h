#ifndef HELLOKIT_EDIT_CHANGE_H
#define HELLOKIT_EDIT_CHANGE_H

#include <memory>
#include <utility>

#include <QtCore/QString>

#include <hellokit/Edit/Slot.h>

namespace hello::kit {

    /// A change of a node of an edit session, reported by EditSession::changed().
    ///
    /// Each kind of node has its own subclasses with the data of its changes. Each subclass
    /// declares its kind as the constant \c Kind, and a receiver distinguishes the subclasses
    /// with as(). A change holds no pointer into the tree, therefore it remains valid after it is
    /// reported.
    class Change {
    public:
        /// The kinds of the changes of this library. The changes of a node type added later use
        /// kinds from \c User.
        enum BuiltInKind {
            ValueKind = 1,
            EntryKind,
            ArrayKind,
            ListKind,
            MoveKind,
            User = 1024,
        };

        inline virtual ~Change() = default;

        inline int kind() const {
            return m_kind;
        }

        /// Returns the changed node.
        inline NodeId node() const {
            return m_node;
        }

        /// Returns this change as \a T, or \c nullptr if its kind is not \c T::Kind.
        template <class T>
        inline const T *as() const {
            return m_kind == T::Kind ? static_cast<const T *>(this) : nullptr;
        }

    protected:
        inline Change(int kind, NodeId node) : m_kind(kind), m_node(node) {
        }

    private:
        int m_kind;
        NodeId m_node;
    };

    using ChangePtr = std::shared_ptr<const Change>;

    /// The value in the slot slot() of a record changed, including the replacement or removal of
    /// a child.
    class ValueChange : public Change {
    public:
        static constexpr int Kind = ValueKind;

        inline ValueChange(NodeId node, int slot) : Change(Kind, node), m_slot(slot) {
        }

        inline int slot() const {
            return m_slot;
        }

    private:
        int m_slot;
    };

    /// The entry key() of a mapping was added, changed or removed.
    class EntryChange : public Change {
    public:
        static constexpr int Kind = EntryKind;

        inline EntryChange(NodeId node, QString key) : Change(Kind, node), m_key(std::move(key)) {
        }

        inline const QString &key() const {
            return m_key;
        }

    private:
        QString m_key;
    };

    /// The elements of an array changed.
    class ArrayChange : public Change {
    public:
        static constexpr int Kind = ArrayKind;

        inline explicit ArrayChange(NodeId node) : Change(Kind, node) {
        }
    };

    /// count() items of a list starting at index() were inserted, are about to be removed, or
    /// were removed.
    class ListChange : public Change {
    public:
        static constexpr int Kind = ListKind;

        enum Type {
            Inserted,

            /// Reported before the removal, while the items are still in the list.
            AboutToBeRemoved,

            Removed,
        };

        inline ListChange(Type type, NodeId node, int index, int count)
            : Change(Kind, node), m_type(type), m_index(index), m_count(count) {
        }

        inline Type type() const {
            return m_type;
        }

        inline int index() const {
            return m_index;
        }

        inline int count() const {
            return m_count;
        }

    private:
        Type m_type;
        int m_index;
        int m_count;
    };

    /// count() items of a list were moved from index(), so that the first of them is at
    /// destination().
    class MoveChange : public Change {
    public:
        static constexpr int Kind = MoveKind;

        inline MoveChange(NodeId node, int index, int count, int destination)
            : Change(Kind, node), m_index(index), m_count(count), m_destination(destination) {
        }

        inline int index() const {
            return m_index;
        }

        inline int count() const {
            return m_count;
        }

        inline int destination() const {
            return m_destination;
        }

    private:
        int m_index;
        int m_count;
        int m_destination;
    };

}

#endif // HELLOKIT_EDIT_CHANGE_H
