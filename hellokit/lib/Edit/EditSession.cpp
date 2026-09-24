#include "EditSession.h"
#include "EditSession_p.h"

#include <substate/ArrayNode.h>
#include <substate/BytesNode.h>
#include <qsubstate/MappingNode.h>

namespace hello::kit {

    namespace {

        using DoubleArrayNode = ss::ArrayNode<double>;

        const char messageKey[] = "message";

        QString messageOf(const std::map<std::string, std::string> &message) {
            const auto it = message.find(messageKey);
            return it == message.end() ? QString() : QString::fromStdString(it->second);
        }

        ss::ArrayView<double> viewOf(const QList<double> &values) {
            return ss::ArrayView<double>(values.constData(), size_t(values.size()));
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
        switch (action.type()) {
            case ss::Action::VectorInsert:
            case ss::Action::VectorRemove: {
                const auto &change = static_cast<const ss::VectorInsDelAction &>(action);
                if (!change.isInsertion(operation)) {
                    Q_EMIT q->itemsAboutToBeRemoved(change.parent()->id(), change.index(),
                                                    int(change.children().size()));
                }
                break;
            }
            default:
                break;
        }
    }

    void EditSession::Impl::actionApplied(const ss::Action &action,
                                          ss::Action::Operation operation) {
        switch (action.type()) {
            case ss::Action::StructAssign: {
                const auto &change = static_cast<const ss::StructAssignAction &>(action);
                Q_EMIT q->valueChanged(change.parent()->id(), change.index());
                break;
            }
            case ss::Action::MappingAssign: {
                const auto &change = static_cast<const ss::MappingAssignAction &>(action);
                Q_EMIT q->entryChanged(change.parent()->id(), change.key());
                break;
            }
            case ss::Action::BytesInsert:
            case ss::Action::BytesRemove:
                Q_EMIT q->arrayChanged(
                    static_cast<const ss::BytesInsDelAction &>(action).parent()->id());
                break;
            case ss::Action::BytesReplace:
                Q_EMIT q->arrayChanged(
                    static_cast<const ss::BytesReplaceAction &>(action).parent()->id());
                break;
            case ss::Action::VectorInsert:
            case ss::Action::VectorRemove: {
                const auto &change = static_cast<const ss::VectorInsDelAction &>(action);
                const auto list = change.parent()->id();
                const int count = int(change.children().size());
                if (change.isInsertion(operation)) {
                    Q_EMIT q->itemsInserted(list, change.index(), count);
                } else {
                    Q_EMIT q->itemsRemoved(list, change.index(), count);
                }
                break;
            }
            case ss::Action::VectorMove: {
                const auto &change = static_cast<const ss::VectorMoveAction &>(action);
                Q_EMIT q->itemsMoved(change.parent()->id(), change.index(operation), change.count(),
                                     change.destination(operation));
                break;
            }
            default:
                // The session creates no other action. The root is installed before the
                // observer is notified of any action.
                Q_ASSERT(false);
                break;
        }
    }

    void EditSession::Impl::stepChanged(int step) {
        Q_UNUSED(step)
        Q_EMIT q->stepChanged();
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
    }

    EditSession::~EditSession() = default;

    NodeId EditSession::root() const {
        const auto root = _impl->model.root();
        return root ? root->id() : 0;
    }

    bool EditSession::contains(NodeId node) const {
        return _impl->find<ss::Node>(node);
    }

    int EditSession::size(NodeId node) const {
        const auto found = _impl->find<ss::Node>(node);
        if (const auto list = dynamic_cast<const ss::VectorNode *>(found)) {
            return list->size();
        }
        if (const auto array = dynamic_cast<const DoubleArrayNode *>(found)) {
            return array->size();
        }
        if (const auto mapping = dynamic_cast<const ss::MappingNode *>(found)) {
            return mapping->size();
        }
        return 0;
    }

    QVariant EditSession::value(NodeId record, int slot) const {
        const auto node = _impl->find<ss::StructNodeBase>(record);
        if (!node) {
            return {};
        }
        Q_ASSERT(slot >= 0 && slot < node->size());
        return node->variant(slot);
    }

    void EditSession::setValue(NodeId record, int slot, const QVariant &value) {
        if (const auto node = _impl->findEditable<ss::StructNodeBase>(record)) {
            Q_ASSERT(slot >= 0 && slot < node->size() && !node->at(slot).isChild());
            node->setAt(slot, value);
        }
    }

    NodeId EditSession::child(NodeId record, ChildSlot slot) const {
        const auto node = _impl->find<ss::StructNodeBase>(record);
        const auto child = node ? node->child(slot.index) : nullptr;
        return child ? child->id() : 0;
    }

    void EditSession::removeChild(NodeId record, ChildSlot slot) {
        if (const auto node = _impl->findEditable<ss::StructNodeBase>(record)) {
            Q_ASSERT(slot.index >= 0 && slot.index < node->size() &&
                     !node->at(slot.index).isVariant());
            node->setAt(slot.index, ss::Property());
        }
    }

    NodeId EditSession::at(NodeId list, int index) const {
        const auto node = _impl->find<ss::VectorNode>(list);
        if (!node) {
            return 0;
        }
        Q_ASSERT(index >= 0 && index < node->size());
        return node->at(index)->id();
    }

    void EditSession::remove(NodeId list, int index, int count) {
        if (const auto node = _impl->findEditable<ss::VectorNode>(list)) {
            node->remove(index, count);
        }
    }

    void EditSession::move(NodeId list, int index, int count, int destination) {
        if (const auto node = _impl->findEditable<ss::VectorNode>(list)) {
            node->move(index, count, destination);
        }
    }

    QStringList EditSession::keys(NodeId mapping) const {
        const auto node = _impl->find<ss::MappingNode>(mapping);
        return node ? node->keys() : QStringList();
    }

    QVariant EditSession::entry(NodeId mapping, const QString &key) const {
        const auto node = _impl->find<ss::MappingNode>(mapping);
        return node ? node->variant(key) : QVariant();
    }

    void EditSession::setEntry(NodeId mapping, const QString &key, const QVariant &value) {
        if (const auto node = _impl->findEditable<ss::MappingNode>(mapping)) {
            node->setProperty(key, value);
        }
    }

    QList<double> EditSession::values(NodeId array) const {
        const auto node = _impl->find<DoubleArrayNode>(array);
        if (!node) {
            return {};
        }
        const auto values = node->values();
        return QList<double>(values.cbegin(), values.cend());
    }

    void EditSession::replaceValues(NodeId array, int index, const QList<double> &values) {
        if (const auto node = _impl->findEditable<DoubleArrayNode>(array)) {
            node->replace(index, viewOf(values));
        }
    }

    void EditSession::insertValues(NodeId array, int index, const QList<double> &values) {
        if (const auto node = _impl->findEditable<DoubleArrayNode>(array)) {
            node->insert(index, viewOf(values));
        }
    }

    void EditSession::removeValues(NodeId array, int index, int count) {
        if (const auto node = _impl->findEditable<DoubleArrayNode>(array)) {
            node->remove(index, count);
        }
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

    QString EditSession::undoMessage() const {
        return canUndo() ? messageOf(_impl->model.stepMessage(_impl->model.currentStep()))
                         : QString();
    }

    QString EditSession::redoMessage() const {
        return canRedo() ? messageOf(_impl->model.stepMessage(_impl->model.currentStep() + 1))
                         : QString();
    }

}
