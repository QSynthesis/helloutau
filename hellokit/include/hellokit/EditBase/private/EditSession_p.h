#ifndef HELLOKIT_EDITBASE_PRIVATE_EDITSESSION_P_H
#define HELLOKIT_EDITBASE_PRIVATE_EDITSESSION_P_H

#include <functional>
#include <map>
#include <memory>
#include <optional>

#include <QtCore/QJsonObject>
#include <QtCore/QStringList>

#include <substate/Model.h>
#include <substate/ModelObserver.h>

#include <hellokit/EditBase/EditSession.h>

namespace hello::kit::edit {

    /// Returns the change reported for \a action applied for \a operation, or \c nullptr if none
    /// is reported at that moment.
    using ChangeTranslator =
        std::function<ChangePtr(const ss::Action &action, ss::Action::Operation operation)>;

    struct RecordInfo;

    /// Returns the record of the nodes of type \a nodeType, or \c nullptr if the type is not a
    /// record of the document.
    using RecordLookup = const RecordInfo *(*) (int nodeType);

    /// Returns the entry of the change log for \a change, without the node, which the log adds,
    /// or \c std::nullopt if the log omits the change. \a change is of the kind for which the
    /// writer is registered. \a lookup provides the field table of the document. See ChangeLog.
    using LogWriter = std::function<std::optional<QJsonObject>(
        const EditSession &session, const Change &change, RecordLookup lookup)>;

    /// A violation of a constraint of a record, found by a Validator.
    struct Violation {
        /// The slot of the record that violates the constraint, or -1 for the record as a whole.
        int slot = -1;

        /// The message reported to the user if a transaction introduces the violation.
        QString message;

        inline bool operator==(const Violation &RHS) const {
            return slot == RHS.slot && message == RHS.message;
        }
    };

    /// Appends the violations of the constraints of \a record to \a violations. A validator is
    /// registered for a record type and covers the lists, mappings, arrays and records in the
    /// slots of the record that have no validator of their own.
    using Validator = std::function<void(const ss::Node *record, QList<Violation> &violations)>;

    /// Returns the reason why nothing in \a record and below it may be modified, or an empty
    /// string if it may be. See EditSessionPrivate::registerLock().
    using Lock = std::function<QString(const ss::Node *record)>;

    class EditSession::Impl : public ss::ModelObserver {
    public:
        explicit Impl(EditSession *q);
        ~Impl();

        EditSession *q;
        ss::Model model;

        /// The message of the outermost transaction in progress.
        QString message;

        /// The number of nested transactions in progress, 0 if none.
        int depth = 0;

        /// Whether a nested transaction ended without commit, which discards the outermost one.
        bool discarded = false;

        /// Ends the innermost transaction in progress. The outermost transaction is committed if
        /// \a commit is true and no nested transaction was discarded, and rolled back otherwise.
        /// See EditSession::Transaction::commit().
        bool endTransaction(bool commit, DiagnosticList &diagnostics);

        /// The translators by action type, for the moments before and after an action is
        /// applied.
        std::map<int, ChangeTranslator> beforeTranslators;
        std::map<int, ChangeTranslator> afterTranslators;

        /// The writers of the change log by change kind.
        std::map<int, LogWriter> logWriters;

        /// The validators by node type.
        std::map<int, Validator> validators;

        /// The violations of each record modified by the transaction in progress, in the state
        /// before its first modification. A record inserted by the transaction has none.
        std::map<NodeId, QList<Violation>> violationsBefore;

        /// The locks by node type.
        std::map<int, Lock> locks;

        /// The reasons of the locks that the transaction in progress modified under, each once.
        QStringList lockedReasons;

        /// Appends to lockedReasons the reason of each locked record at or above \a node.
        void recordLocks(const ss::Node *node);

        /// Returns \a node or its nearest ancestor with a validator, or \c nullptr if none.
        const ss::Node *validatedRecordOf(const ss::Node *node) const;

        /// Returns the violations of \a record found by its validator.
        QList<Violation> violationsOf(const ss::Node *record) const;

        /// Records the violations of the record of \a node before its first modification in the
        /// transaction in progress, or none if \a inserted is true.
        void recordViolations(const ss::Node *node, bool inserted);

        /// Appends the violations that the transaction in progress introduced to \a diagnostics,
        /// and returns whether there are any.
        bool introducedViolations(DiagnosticList &diagnostics) const;

        /// Returns the node of \a id if it is in the tree, or \c nullptr.
        inline ss::Node *find(NodeId id) const {
            const auto node = id ? model.nodeById(id) : nullptr;
            return node && node->isAttached() ? node : nullptr;
        }

    protected:
        void actionAboutToApply(const ss::Action &action, ss::Action::Operation operation) override;
        void actionApplied(const ss::Action &action, ss::Action::Operation operation) override;
        void stepChanged(int step) override;
    };

    /// The extension interface of EditSession, for the subclasses that supply the tree of a
    /// document and for the handles of its nodes.
    ///
    /// A handle reads a node through find() and modifies it through findEditable(), which
    /// enforces the rules of EditSession. A node type added outside this library registers the
    /// translation of its actions into changes with registerChange(), and the change log entries
    /// of its changes with registerLogWriter(). A document registers the constraints of its
    /// records with registerValidator().
    struct EditSessionPrivate {
        static inline EditSession::Impl &impl(const EditSession &session) {
            return *session._impl;
        }

        /// Installs \a root as the tree of \a session, without an undo step. Called once by the
        /// constructor of the subclass.
        static inline void setRoot(EditSession &session, std::unique_ptr<ss::Node> root) {
            impl(session).model.reset(std::move(root));
        }

        /// Returns the node of \a id if \a session is not null and the node is in the tree, or
        /// \c nullptr.
        static inline ss::Node *find(const EditSession *session, NodeId id) {
            return session ? impl(*session).find(id) : nullptr;
        }

        /// Returns the node of \a id as \a NodeType if it is in the tree, or \c nullptr.
        ///
        /// A node of another type than \a type, which identifies \a NodeType, is a programming
        /// error, because a handle is created for a node of its own type. The type is therefore
        /// compared only by an assertion.
        template <class NodeType>
        static inline NodeType *find(const EditSession *session, NodeId id, int type) {
            Q_UNUSED(type)
            const auto node = find(session, id);
            Q_ASSERT_X(!node || node->type() == type, "EditSession",
                       "a handle of a node of another type");
            return static_cast<NodeType *>(node);
        }

        /// Returns the node of \a id as \a NodeType for a modification, or \c nullptr after a
        /// failed assertion if no transaction is in progress or the node is not in the tree.
        template <class NodeType>
        static inline NodeType *findEditable(const EditSession *session, NodeId id, int type) {
            const auto node = session && impl(*session).model.inTransaction()
                                  ? find<NodeType>(session, id, type)
                                  : nullptr;
            Q_ASSERT_X(node, "EditSession",
                       "a modification requires a transaction and a node in the tree");
            return node;
        }

        /// Registers the translation of the actions of type \a actionType into the changes
        /// reported after they are applied, and optionally before.
        static inline void registerChange(EditSession &session, int actionType,
                                          ChangeTranslator after, ChangeTranslator before = {}) {
            auto &d = impl(session);
            d.afterTranslators[actionType] = std::move(after);
            if (before) {
                d.beforeTranslators[actionType] = std::move(before);
            }
        }

        /// Registers the writer of the change log entries of the changes of kind \a changeKind.
        static inline void registerLogWriter(EditSession &session, int changeKind,
                                             LogWriter writer) {
            impl(session).logWriters[changeKind] = std::move(writer);
        }

        /// Registers the validator of the records of type \a nodeType. A transaction is committed
        /// only if it introduces no violation, see docs/Editing.md.
        static inline void registerValidator(EditSession &session, int nodeType,
                                             Validator validator) {
            impl(session).validators[nodeType] = std::move(validator);
        }

        /// Registers the lock of the records of type \a nodeType. A transaction that modifies a
        /// locked record or anything below it is not committed, whatever the state it leaves,
        /// because the lock protects content that cannot be saved correctly. Replacing or
        /// removing the locked record itself modifies its parent and is not refused, so that the
        /// document layer can replace the content, for example by reading it again.
        static inline void registerLock(EditSession &session, int nodeType, Lock lock) {
            impl(session).locks[nodeType] = std::move(lock);
        }
    };

}

#endif // HELLOKIT_EDITBASE_PRIVATE_EDITSESSION_P_H
