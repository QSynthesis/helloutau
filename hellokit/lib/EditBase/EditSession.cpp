#include "EditSession.h"
#include "EditSession_p.h"

#include <utility>

#include <stdcorelib/pimpl.h>

#include <substate/BytesNode.h>
#include <substate/VectorNode.h>
#include <qsubstate/MappingNode.h>
#include <qsubstate/StructNode.h>

#include "ChangeLog_p.h"

namespace hello::kit::edit {

    namespace {

        const char messageKey[] = "message";

        QString messageOf(const std::map<std::string, std::string> &message) {
            const auto it = message.find(messageKey);
            return it == message.end() ? QString() : QString::fromStdString(it->second);
        }

        Diagnostic errorOf(const QString &message) {
            Diagnostic diagnostic;
            diagnostic.severity = DiagnosticSeverity::Error;
            diagnostic.message = message;
            return diagnostic;
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
                session, ss::Action::StructAssign,
                [](const ss::Action &action, Operation operation) {
                    const auto &assign = static_cast<const ss::StructAssignAction &>(action);
                    return std::make_shared<ValueChange>(assign.parent()->id(), assign.index(),
                                                         assign.oldVariant(operation),
                                                         assign.newVariant(operation));
                });

            EditSessionPrivate::registerChange(
                session, ss::Action::MappingAssign,
                [](const ss::Action &action, Operation operation) {
                    const auto &assign = static_cast<const ss::MappingAssignAction &>(action);
                    return std::make_shared<EntryChange>(assign.parent()->id(), assign.key(),
                                                         assign.oldVariant(operation),
                                                         assign.newVariant(operation));
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

    EditSession::Impl::Impl(Decl *decl) : _decl(decl) {
        model.addObserver(this);
    }

    // The observer is removed first, because destroying the model notifies its observers, and
    // the session emitting the signals is being destroyed.
    EditSession::Impl::~Impl() {
        model.removeObserver(this);
    }

    void EditSession::Impl::actionAboutToApply(const ss::Action &action,
                                               ss::Action::Operation operation) {
        stdc_decl_t;
        // The first execution is the modification by the caller. Undo, redo and rollback restore
        // states that the validation has already accepted or that it does not concern.
        if (operation == ss::Action::Execute && !validators.empty()) {
            const auto it = afterTranslators.find(action.type());
            if (it != afterTranslators.end()) {
                if (const auto change = it->second(action, operation)) {
                    recordViolations(model.nodeById(change->node()), false);
                }
            }
        }

        const auto it = beforeTranslators.find(action.type());
        if (it == beforeTranslators.end()) {
            return;
        }
        if (const auto change = it->second(action, operation)) {
            Q_EMIT decl.changed(change);
        }
    }

    void EditSession::Impl::actionApplied(const ss::Action &action,
                                          ss::Action::Operation operation) {
        stdc_decl_t;
        if (operation == ss::Action::Execute && action.type() == ss::Action::VectorInsert) {
            for (const auto child :
                 static_cast<const ss::VectorInsDelAction &>(action).children()) {
                recordViolations(child, true);
            }
        }

        const auto it = afterTranslators.find(action.type());
        // Every action applied to the tree must be reported, otherwise a view of the tree
        // diverges from it without notice.
        Q_ASSERT_X(it != afterTranslators.end(), "EditSession",
                   "no change is registered for the action type");
        if (it == afterTranslators.end()) {
            return;
        }
        if (const auto change = it->second(action, operation)) {
            Q_EMIT decl.changed(change);
        }
    }

    const ss::Node *EditSession::Impl::validatedRecordOf(const ss::Node *node) const {
        for (; node; node = node->parent()) {
            if (validators.count(node->type())) {
                return node;
            }
        }
        return nullptr;
    }

    QList<Violation> EditSession::Impl::violationsOf(const ss::Node *record) const {
        QList<Violation> violations;
        validators.at(record->type())(record, violations);
        return violations;
    }

    void EditSession::Impl::recordViolations(const ss::Node *node, bool inserted) {
        const auto record = validatedRecordOf(node);
        // An inserted node without a validator belongs to the record that holds the list, which
        // the insertion has recorded before it was applied.
        if (!record || (inserted && record != node)) {
            return;
        }
        if (!violationsBefore.count(record->id())) {
            violationsBefore[record->id()] = inserted ? QList<Violation>() : violationsOf(record);
        }
    }

    bool EditSession::Impl::introducedViolations(DiagnosticList &diagnostics) const {
        bool found = false;
        for (const auto &[id, before] : violationsBefore) {
            // A record removed by the transaction has no constraints left to violate.
            const auto record = find(id);
            if (!record) {
                continue;
            }
            for (const auto &violation : violationsOf(record)) {
                if (!before.contains(violation)) {
                    diagnostics.push_back(errorOf(violation.message));
                    found = true;
                }
            }
        }
        return found;
    }

    void EditSession::Impl::stepChanged(int step) {
        stdc_decl_t;
        Q_EMIT decl.stepChanged(step);
    }

    EditSession::Transaction::Transaction(EditSession *session) : m_session(session) {
    }

    EditSession::Transaction::Transaction(Transaction &&RHS) noexcept : m_session(RHS.m_session) {
        RHS.m_session = nullptr;
    }

    bool EditSession::Impl::endTransaction(bool commit, DiagnosticList &diagnostics) {
        Q_ASSERT(depth > 0);
        discarded = discarded || !commit;
        if (--depth > 0) {
            return true;
        }

        bool committed = false;
        if (discarded) {
            if (commit) {
                diagnostics.push_back(errorOf(EditSession::tr(
                    "The modification was not applied because one of its steps was cancelled.")));
            }
        } else {
            committed = read || !introducedViolations(diagnostics);
        }

        if (committed) {
            model.commitTransaction({
                {messageKey, message.toStdString()}
            });
        } else {
            model.abortTransaction();
        }
        message.clear();
        discarded = false;
        read = false;
        violationsBefore.clear();
        return committed;
    }

    EditSession::Transaction::~Transaction() {
        if (m_session) {
            DiagnosticList ignored;
            m_session->_impl->endTransaction(false, ignored);
        }
    }

    bool EditSession::Transaction::commit(DiagnosticList &diagnostics) {
        Q_ASSERT_X(m_session, "EditSession::Transaction", "the transaction has ended");
        if (!m_session) {
            return false;
        }
        const auto session = std::exchange(m_session, nullptr);
        return session->_impl->endTransaction(true, diagnostics);
    }

    bool EditSession::Transaction::commit() {
        DiagnosticList ignored;
        return commit(ignored);
    }

    EditSession::EditSession(QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(this)) {
        registerBuiltInChanges(*this);
        ChangeLog::registerBuiltInWriters(*this);
    }

    EditSession::~EditSession() = default;

    NodeId EditSession::root() const {
        stdc_impl_t;
        const auto root = impl.model.root();
        return root ? root->id() : 0;
    }

    bool EditSession::contains(NodeId node) const {
        stdc_impl_t;
        return impl.find(node);
    }

    EditSession::Transaction EditSession::transaction(const QString &message) {
        stdc_impl_t;
        // substate supports no nesting, therefore only the outermost transaction begins one.
        if (impl.depth++ == 0) {
            impl.model.beginTransaction();
            impl.message = message;
        }
        return Transaction(this);
    }

    bool EditSession::inTransaction() const {
        stdc_impl_t;
        return impl.model.inTransaction();
    }

    bool EditSession::canUndo() const {
        stdc_impl_t;
        return !inTransaction() && impl.model.canUndo();
    }

    bool EditSession::canRedo() const {
        stdc_impl_t;
        return !inTransaction() && impl.model.canRedo();
    }

    void EditSession::undo() {
        stdc_impl_t;
        Q_ASSERT_X(!inTransaction(), "EditSession", "undo during a transaction");
        if (canUndo()) {
            impl.model.undo();
        }
    }

    void EditSession::redo() {
        stdc_impl_t;
        Q_ASSERT_X(!inTransaction(), "EditSession", "redo during a transaction");
        if (canRedo()) {
            impl.model.redo();
        }
    }

    int EditSession::currentStep() const {
        stdc_impl_t;
        return impl.model.currentStep();
    }

    int EditSession::minimumStep() const {
        stdc_impl_t;
        return impl.model.minimumStep();
    }

    int EditSession::maximumStep() const {
        stdc_impl_t;
        return impl.model.maximumStep();
    }

    QString EditSession::stepMessage(int step) const {
        stdc_impl_t;
        return messageOf(impl.model.stepMessage(step));
    }

    QString EditSession::undoMessage() const {
        return canUndo() ? stepMessage(currentStep()) : QString();
    }

    QString EditSession::redoMessage() const {
        return canRedo() ? stepMessage(currentStep() + 1) : QString();
    }

}
