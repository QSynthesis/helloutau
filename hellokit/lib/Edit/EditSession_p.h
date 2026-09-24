#ifndef HELLOKIT_EDIT_EDITSESSION_P_H
#define HELLOKIT_EDIT_EDITSESSION_P_H

#include <functional>
#include <map>
#include <memory>

#include <substate/Model.h>
#include <substate/ModelObserver.h>

#include <hellokit/Edit/EditSession.h>

namespace hello::kit {

    /// Returns the change reported for \a action applied for \a operation, or \c nullptr if none
    /// is reported at that moment.
    using ChangeTranslator =
        std::function<ChangePtr(const ss::Action &action, ss::Action::Operation operation)>;

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
    /// translation of its actions into changes with registerChange().
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
    };

}

#endif // HELLOKIT_EDIT_EDITSESSION_P_H
