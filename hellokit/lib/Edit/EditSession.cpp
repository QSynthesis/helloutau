#include "EditSession.h"
#include "EditSession_p.h"

#include <substate/BytesNode.h>
#include <substate/VectorNode.h>
#include <qsubstate/MappingNode.h>
#include <qsubstate/StructNode.h>

namespace hello::kit {

    namespace {

        const char messageKey[] = "message";

        QString messageOf(const std::map<std::string, std::string> &message) {
            const auto it = message.find(messageKey);
            return it == message.end() ? QString() : QString::fromStdString(it->second);
        }

        ChangePtr listChange(const ss::Action &action, ListChange::Type type) {
            const auto &insDel = static_cast<const ss::VectorInsDelAction &>(action);
            return std::make_shared<ListChange>(type, insDel.parent()->id(), insDel.index(),
                                                int(insDel.children().size()));
        }

        // The translations of the actions of the node types of substate, registered through the
        // same interface as those of node types added later.
        void registerBuiltInChanges(EditSession &session) {
            using Operation = ss::Action::Operation;

            EditSessionPrivate::registerChange(
                session, ss::Action::StructAssign, [](const ss::Action &action, Operation) {
                    const auto &assign = static_cast<const ss::StructAssignAction &>(action);
                    return std::make_shared<ValueChange>(assign.parent()->id(), assign.index());
                });

            EditSessionPrivate::registerChange(
                session, ss::Action::MappingAssign, [](const ss::Action &action, Operation) {
                    const auto &assign = static_cast<const ss::MappingAssignAction &>(action);
                    return std::make_shared<EntryChange>(assign.parent()->id(), assign.key());
                });

            const auto bytesInsDel = [](const ss::Action &action, Operation) -> ChangePtr {
                return std::make_shared<ArrayChange>(
                    static_cast<const ss::BytesInsDelAction &>(action).parent()->id());
            };
            EditSessionPrivate::registerChange(session, ss::Action::BytesInsert, bytesInsDel);
            EditSessionPrivate::registerChange(session, ss::Action::BytesRemove, bytesInsDel);
            EditSessionPrivate::registerChange(
                session, ss::Action::BytesReplace, [](const ss::Action &action, Operation) {
                    return std::make_shared<ArrayChange>(
                        static_cast<const ss::BytesReplaceAction &>(action).parent()->id());
                });

            // A removal is also reported before it is applied, while the items are in the list.
            const auto vectorAfter = [](const ss::Action &action, Operation operation) {
                const auto &insDel = static_cast<const ss::VectorInsDelAction &>(action);
                return listChange(action, insDel.isInsertion(operation) ? ListChange::Inserted
                                                                        : ListChange::Removed);
            };
            const auto vectorBefore = [](const ss::Action &action,
                                         Operation operation) -> ChangePtr {
                const auto &insDel = static_cast<const ss::VectorInsDelAction &>(action);
                return insDel.isInsertion(operation)
                           ? nullptr
                           : listChange(action, ListChange::AboutToBeRemoved);
            };
            EditSessionPrivate::registerChange(session, ss::Action::VectorInsert, vectorAfter,
                                               vectorBefore);
            EditSessionPrivate::registerChange(session, ss::Action::VectorRemove, vectorAfter,
                                               vectorBefore);

            EditSessionPrivate::registerChange(
                session, ss::Action::VectorMove, [](const ss::Action &action, Operation operation) {
                    const auto &move = static_cast<const ss::VectorMoveAction &>(action);
                    return std::make_shared<MoveChange>(move.parent()->id(), move.index(operation),
                                                        move.count(), move.destination(operation));
                });
        }

    }

    EditSession::Impl::Impl(EditSession *q) : q(q) {
        model.addObserver(this);
    }

    // The observer is removed first, because destroying the model notifies its observers, and
    // the session emitting the signals is being destroyed.
    EditSession::Impl::~Impl() {
        model.removeObserver(this);
    }

    void EditSession::Impl::actionAboutToApply(const ss::Action &action,
                                               ss::Action::Operation operation) {
        const auto it = beforeTranslators.find(action.type());
        if (it == beforeTranslators.end()) {
            return;
        }
        if (const auto change = it->second(action, operation)) {
            Q_EMIT q->changed(change);
        }
    }

    void EditSession::Impl::actionApplied(const ss::Action &action,
                                          ss::Action::Operation operation) {
        const auto it = afterTranslators.find(action.type());
        // Every action applied to the tree must be reported, otherwise a view of the tree
        // diverges from it without notice.
        Q_ASSERT_X(it != afterTranslators.end(), "EditSession",
                   "no change is registered for the action type");
        if (it == afterTranslators.end()) {
            return;
        }
        if (const auto change = it->second(action, operation)) {
            Q_EMIT q->changed(change);
        }
    }

    void EditSession::Impl::stepChanged(int step) {
        Q_EMIT q->stepChanged(step);
    }

    EditSession::Transaction::Transaction(EditSession *session) : m_session(session) {
    }

    EditSession::Transaction::Transaction(Transaction &&RHS) noexcept : m_session(RHS.m_session) {
        RHS.m_session = nullptr;
    }

    EditSession::Transaction::~Transaction() {
        if (m_session) {
            m_session->_impl->model.abortTransaction();
            m_session->_impl->message.clear();
        }
    }

    void EditSession::Transaction::commit() {
        Q_ASSERT_X(m_session, "EditSession::Transaction", "the transaction has ended");
        if (!m_session) {
            return;
        }
        auto &d = *m_session->_impl;
        d.model.commitTransaction({
            {messageKey, d.message.toStdString()}
        });
        d.message.clear();
        m_session = nullptr;
    }

    EditSession::EditSession(QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(this)) {
        registerBuiltInChanges(*this);
    }

    EditSession::~EditSession() = default;

    NodeId EditSession::root() const {
        const auto root = _impl->model.root();
        return root ? root->id() : 0;
    }

    bool EditSession::contains(NodeId node) const {
        return _impl->find(node);
    }

    EditSession::Transaction EditSession::transaction(const QString &message) {
        Q_ASSERT_X(!inTransaction(), "EditSession", "transactions cannot be nested");
        _impl->model.beginTransaction();
        _impl->message = message;
        return Transaction(this);
    }

    bool EditSession::inTransaction() const {
        return _impl->model.inTransaction();
    }

    bool EditSession::canUndo() const {
        return !inTransaction() && _impl->model.canUndo();
    }

    bool EditSession::canRedo() const {
        return !inTransaction() && _impl->model.canRedo();
    }

    void EditSession::undo() {
        Q_ASSERT_X(!inTransaction(), "EditSession", "undo during a transaction");
        if (canUndo()) {
            _impl->model.undo();
        }
    }

    void EditSession::redo() {
        Q_ASSERT_X(!inTransaction(), "EditSession", "redo during a transaction");
        if (canRedo()) {
            _impl->model.redo();
        }
    }

    int EditSession::currentStep() const {
        return _impl->model.currentStep();
    }

    int EditSession::minimumStep() const {
        return _impl->model.minimumStep();
    }

    int EditSession::maximumStep() const {
        return _impl->model.maximumStep();
    }

    QString EditSession::stepMessage(int step) const {
        return messageOf(_impl->model.stepMessage(step));
    }

    QString EditSession::undoMessage() const {
        return canUndo() ? stepMessage(currentStep()) : QString();
    }

    QString EditSession::redoMessage() const {
        return canRedo() ? stepMessage(currentStep() + 1) : QString();
    }

}
