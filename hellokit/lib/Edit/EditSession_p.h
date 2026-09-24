#ifndef HELLOKIT_EDIT_EDITSESSION_P_H
#define HELLOKIT_EDIT_EDITSESSION_P_H

#include <memory>
#include <vector>

#include <substate/Model.h>
#include <substate/ModelObserver.h>
#include <substate/VectorNode.h>
#include <qsubstate/StructNode.h>

#include <hellokit/Edit/EditSession.h>

namespace hello::kit {

    class EditSession::Impl : public ss::ModelObserver {
    public:
        explicit Impl(EditSession *q);
        ~Impl();

        EditSession *q;
        ss::Model model;

        /// The message of the transaction in progress.
        QString message;

        /// Returns the node of \a id if it is in the tree and of type \a NodeType, or
        /// \c nullptr.
        template <class NodeType>
        inline NodeType *find(NodeId id) const {
            const auto node = id ? model.nodeById(id) : nullptr;
            return node && node->isAttached() ? dynamic_cast<NodeType *>(node) : nullptr;
        }

        /// Returns the node of \a id for a modification, or \c nullptr after a failed assertion
        /// if no transaction is in progress, or if the node is not in the tree or not of type
        /// \a NodeType.
        template <class NodeType>
        inline NodeType *findEditable(NodeId id) const {
            const auto node = model.inTransaction() ? find<NodeType>(id) : nullptr;
            Q_ASSERT_X(node, "EditSession",
                       "a modification requires a transaction and a node of the expected kind "
                       "in the tree");
            return node;
        }

    protected:
        void actionAboutToApply(const ss::Action &action, ss::Action::Operation operation) override;
        void actionApplied(const ss::Action &action, ss::Action::Operation operation) override;
        void stepChanged(int step) override;
    };

    /// The extension interface of EditSession, for the subclasses that supply the structure of
    /// a document. Its functions follow the rules of the modifications of EditSession.
    struct EditSessionPrivate {
        static inline EditSession::Impl &impl(const EditSession &session) {
            return *session._impl;
        }

        /// Installs \a root as the tree of \a session, without an undo step. Called once by the
        /// constructor of the subclass.
        static inline void setRoot(EditSession &session, std::unique_ptr<ss::Node> root) {
            impl(session).model.reset(std::move(root));
        }

        /// Returns the node of \a id if it is in the tree and of type \a NodeType, or
        /// \c nullptr.
        template <class NodeType = ss::Node>
        static inline NodeType *find(const EditSession &session, NodeId id) {
            return impl(session).find<NodeType>(id);
        }

        /// Inserts \a items, free nodes, into the list \a list before \a index.
        static inline void insert(EditSession &session, NodeId list, int index,
                                  std::vector<std::unique_ptr<ss::Node>> items) {
            const auto node = impl(session).findEditable<ss::VectorNode>(list);
            if (node && !items.empty()) {
                node->insert(index, std::move(items));
            }
        }

        /// Stores \a child, a free node, in the slot \a slot of the record \a record, replacing
        /// the previous child.
        static inline void setChild(EditSession &session, NodeId record, int slot,
                                    std::unique_ptr<ss::Node> child) {
            if (const auto node = impl(session).findEditable<ss::StructNodeBase>(record)) {
                node->setAt(slot, std::move(child));
            }
        }
    };

}

#endif // HELLOKIT_EDIT_EDITSESSION_P_H
