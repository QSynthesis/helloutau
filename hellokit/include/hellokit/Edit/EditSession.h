#ifndef HELLOKIT_EDIT_EDITSESSION_H
#define HELLOKIT_EDIT_EDITSESSION_H

#include <memory>

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

#include <hellokit/Edit/HelloKitEditGlobal.h>
#include <hellokit/Edit/Slot.h>

namespace hello::kit {

    /// The editing of a document stored as a tree of nodes, with transactions, undo history and
    /// change notification. See docs/Editing.md.
    ///
    /// This class does not depend on the structure of a particular document. It reads and
    /// modifies the nodes by \c NodeId: the slots of records, the items of lists, the entries of
    /// mappings and the elements of arrays. A subclass supplies the tree and the operations that
    /// require the structure of the document, such as the insertion of a record from a value,
    /// for example ProjectSession.
    ///
    /// Every modification takes place in a transaction, and each committed transaction is one
    /// undo step. A modification outside a transaction, of a node that is not in the tree or of
    /// a node of another kind is a programming error. It is checked by an assertion and has no
    /// effect. Reading such a node returns default values.
    ///
    /// The signals report every change as it is applied, including the changes applied by undo,
    /// by redo and by the rollback of a transaction. A record slot is identified by its index in
    /// the slot table of the record.
    ///
    /// \warning The session emits the signals while applying a change. A slot connected to them
    ///          may read the session but must not modify it. A modification in response to a
    ///          signal requires a queued connection.
    class HELLOKIT_EDIT_EXPORT EditSession : public QObject {
        Q_OBJECT
    public:
        /// A transaction of a session, which collects modifications into one undo step.
        ///
        /// The modifications take effect immediately. commit() makes them one undo step with
        /// the message of the transaction. A transaction destroyed without commit() is rolled
        /// back, which restores the state before the transaction, therefore an early return or
        /// an exception discards the modifications.
        class HELLOKIT_EDIT_EXPORT Transaction {
        public:
            Transaction(Transaction &&RHS) noexcept;
            ~Transaction();

            Transaction(const Transaction &) = delete;
            Transaction &operator=(const Transaction &) = delete;
            Transaction &operator=(Transaction &&) = delete;

            /// Commits the transaction. A transaction without modifications creates no undo
            /// step.
            void commit();

        private:
            Transaction(EditSession *session);

            EditSession *m_session;

            friend class EditSession;
        };

        ~EditSession();

        /// Returns the root of the tree.
        NodeId root() const;

        /// Returns whether \a node exists and is in the tree.
        bool contains(NodeId node) const;

        /// Returns the number of items of a list, of elements of an array, or of entries of a
        /// mapping.
        int size(NodeId node) const;

        /// \name Records
        /// \{

        /// Returns the value in the slot \a slot of the record \a record.
        QVariant value(NodeId record, int slot) const;

        /// Stores \a value in the slot \a slot of the record \a record. An invalid \a value
        /// empties the slot. A value equal to the current one creates no change.
        void setValue(NodeId record, int slot, const QVariant &value);

        template <class T>
        inline T value(NodeId record, Slot<T> slot) const;

        template <class T>
        inline void setValue(NodeId record, Slot<T> slot, const typename Slot<T>::ValueType &value);

        /// Returns the child in the slot \a slot of the record \a record, or 0 if the slot is
        /// empty.
        NodeId child(NodeId record, ChildSlot slot) const;

        /// Removes the child in the slot \a slot of the record \a record.
        void removeChild(NodeId record, ChildSlot slot);

        /// \}

        /// \name Lists
        /// \{

        /// Returns the item at \a index of the list \a list.
        NodeId at(NodeId list, int index) const;

        void remove(NodeId list, int index, int count);

        /// Moves \a count items starting at \a index so that the first of them is at
        /// \a destination afterwards. The items keep their identifiers.
        void move(NodeId list, int index, int count, int destination);

        /// \}

        /// \name Mappings
        /// \{

        /// Returns the keys of the mapping \a mapping in ascending order.
        QStringList keys(NodeId mapping) const;

        /// Returns the value of \a key, or an invalid QVariant if the entry does not exist.
        QVariant entry(NodeId mapping, const QString &key) const;

        /// Stores \a value under \a key. An invalid \a value removes the entry.
        void setEntry(NodeId mapping, const QString &key, const QVariant &value);

        /// \}

        /// \name Arrays of double
        /// \{

        QList<double> values(NodeId array) const;

        /// Overwrites the elements starting at \a index, extending the array if \a values
        /// reaches beyond its end.
        void replaceValues(NodeId array, int index, const QList<double> &values);

        void insertValues(NodeId array, int index, const QList<double> &values);
        void removeValues(NodeId array, int index, int count);

        /// \}

        /// Begins a transaction with \a message, the description of the modification shown in
        /// the undo history. Transactions cannot be nested.
        Transaction transaction(const QString &message);

        /// Returns whether a transaction is in progress.
        bool inTransaction() const;

        bool canUndo() const;
        bool canRedo() const;

        /// Undoes the last committed transaction. Requires that no transaction is in progress.
        void undo();

        /// Redoes the last undone transaction. Requires that no transaction is in progress.
        void redo();

        /// Returns the message of the transaction that undo() reverts, or an empty string if
        /// none exists.
        QString undoMessage() const;

        /// Returns the message of the transaction that redo() applies, or an empty string if
        /// none exists.
        QString redoMessage() const;

    Q_SIGNALS:
        /// The value in the slot \a slot of the record \a node changed, including the
        /// replacement of a child.
        void valueChanged(hello::kit::NodeId node, int slot);

        /// The entry \a key of the mapping \a node was added, changed or removed.
        void entryChanged(hello::kit::NodeId node, const QString &key);

        /// The elements of the array \a node changed.
        void arrayChanged(hello::kit::NodeId node);

        /// \a count items were inserted into the list \a list at \a index.
        void itemsInserted(hello::kit::NodeId list, int index, int count);

        /// \a count items of the list \a list starting at \a index are about to be removed. They
        /// are still in the list.
        void itemsAboutToBeRemoved(hello::kit::NodeId list, int index, int count);

        /// \a count items were removed from the list \a list at \a index.
        void itemsRemoved(hello::kit::NodeId list, int index, int count);

        /// \a count items of the list \a list were moved from \a index, so that the first of them
        /// is at \a destination.
        void itemsMoved(hello::kit::NodeId list, int index, int count, int destination);

        /// The position in the undo history changed by a commit, an undo or a redo.
        void stepChanged();

    protected:
        /// Creates a session without a tree. The constructor of the subclass installs the tree
        /// through the extension interface in EditSession_p.h.
        explicit EditSession(QObject *parent = nullptr);

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;

        friend struct EditSessionPrivate;
    };

    template <class T>
    inline T EditSession::value(NodeId record, Slot<T> slot) const {
        return SlotValue<T>::fromVariant(value(record, slot.index));
    }

    template <class T>
    inline void EditSession::setValue(NodeId record, Slot<T> slot,
                                      const typename Slot<T>::ValueType &value) {
        setValue(record, slot.index, SlotValue<T>::toVariant(value));
    }

}

#endif // HELLOKIT_EDIT_EDITSESSION_H
