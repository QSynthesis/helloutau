#ifndef HELLOKIT_EDIT_EDITSESSION_H
#define HELLOKIT_EDIT_EDITSESSION_H

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QString>

#include <hellokit/Edit/Change.h>
#include <hellokit/Edit/HelloKitEditGlobal.h>
#include <hellokit/Edit/Slot.h>

namespace hello::kit {

    /// The editing of a document stored as a tree of nodes, with transactions, undo history and
    /// change notification. See docs/Editing.md.
    ///
    /// This class does not depend on the structure of a particular document or on the kinds of
    /// its nodes. A subclass, such as ProjectSession, supplies the tree, and the handles of the
    /// document, such as NoteRef, read and modify it.
    ///
    /// Every modification takes place in a transaction, and each committed transaction is one
    /// undo step. A modification outside a transaction, or of a node that is not in the tree, is
    /// a programming error. It is checked by an assertion and has no effect.
    ///
    /// changed() reports every change as it is applied, including the changes applied by undo,
    /// by redo and by the rollback of a transaction.
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

            Transaction &operator=(Transaction &&) = delete;

            /// Commits the transaction. A transaction without modifications creates no undo
            /// step.
            void commit();

        private:
            Transaction(EditSession *session);

            EditSession *m_session;

            Q_DISABLE_COPY(Transaction);

            friend class EditSession;
        };

        ~EditSession();

        /// Returns the root of the tree.
        NodeId root() const;

        /// Returns whether \a node exists and is in the tree.
        bool contains(NodeId node) const;

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

        /// Returns the current position in the undo history, the number of the last applied
        /// transaction. It is 0 before the first transaction, increases by one with each commit
        /// and redo, and decreases by one with each undo. A committed transaction discards the
        /// transactions after the current position, and its number follows the current position.
        ///
        /// The number identifies a state of the document within the session, for example the
        /// saved state: the document is unmodified if the current step equals the step at which
        /// it was saved.
        int currentStep() const;

        /// Returns the earliest step that undo() can reach. It increases if the history discards
        /// its earliest transactions.
        int minimumStep() const;

        /// Returns the latest step that redo() can reach.
        int maximumStep() const;

        /// Returns the message of the transaction with the number \a step, or an empty string if
        /// the history does not contain it. The history contains the steps from
        /// minimumStep() + 1 to maximumStep().
        QString stepMessage(int step) const;

        /// Returns the message of the transaction that undo() reverts, or an empty string if
        /// none exists.
        QString undoMessage() const;

        /// Returns the message of the transaction that redo() applies, or an empty string if
        /// none exists.
        QString redoMessage() const;

    Q_SIGNALS:
        /// Reports \a change after it is applied, or before it is applied for
        /// ListChange::AboutToBeRemoved. The subclass of \a change describes the change.
        void changed(const hello::kit::ChangePtr &change);

        /// The current position in the undo history changed to \a step by a commit, an undo or a
        /// redo. See currentStep().
        void stepChanged(int step);

    protected:
        /// Creates a session without a tree. The constructor of the subclass installs the tree
        /// through the extension interface in EditSession_p.h.
        explicit EditSession(QObject *parent = nullptr);

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;

        friend struct EditSessionPrivate;
    };

}

#endif // HELLOKIT_EDIT_EDITSESSION_H
