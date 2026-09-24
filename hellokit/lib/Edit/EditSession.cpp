#include "EditSession.h"

#include <vector>

#include <substate/BytesNode.h>
#include <substate/Model.h>
#include <substate/ModelObserver.h>
#include <substate/VectorNode.h>
#include <qsubstate/MappingNode.h>
#include <qsubstate/StructNode.h>

#include "ProjectTree_p.h"

namespace hello::kit {

    namespace {

        const char messageKey[] = "message";

        QString messageOf(const std::map<std::string, std::string> &message) {
            const auto it = message.find(messageKey);
            return it == message.end() ? QString() : QString::fromStdString(it->second);
        }

        ss::ArrayView<double> viewOf(const QList<double> &values) {
            return ss::ArrayView<double>(values.constData(), size_t(values.size()));
        }

    }

    class EditSession::Impl : public ss::ModelObserver {
    public:
        explicit Impl(EditSession *q) : q(q) {
            model.addObserver(this);
        }

        // The observer is removed first, because destroying the model notifies its observers,
        // and the session emitting the signals is being destroyed.
        ~Impl() {
            model.removeObserver(this);
        }

        EditSession *q;
        ss::Model model;

        // The message of the transaction in progress.
        QString message;

        // Returns the node of id if it is in the tree and of type NodeType, or nullptr.
        template <class NodeType>
        NodeType *find(NodeId id) const {
            const auto node = id ? model.nodeById(id) : nullptr;
            return node && node->isAttached() ? dynamic_cast<NodeType *>(node) : nullptr;
        }

        // Returns the node of id for a modification, or nullptr after a failed assertion if no
        // transaction is in progress, or if the node is not in the tree or not of type NodeType.
        template <class NodeType>
        NodeType *findEditable(NodeId id) const {
            const auto node = model.inTransaction() ? find<NodeType>(id) : nullptr;
            Q_ASSERT_X(node, "EditSession",
                       "a modification requires a transaction and a node of the expected kind "
                       "in the tree");
            return node;
        }

        // Returns the list of id for an insertion of items whose parent record has the type
        // owner, or nullptr after a failed assertion.
        ss::VectorNode *findEditableList(NodeId id, int owner) const {
            const auto list = findEditable<ss::VectorNode>(id);
            if (list && list->parent()->type() != owner) {
                Q_ASSERT_X(false, "EditSession", "an insertion of items of another kind");
                return nullptr;
            }
            return list;
        }

        template <class Value, class Convert>
        void insert(NodeId id, int owner, int index, const QList<Value> &values, Convert convert) {
            const auto list = findEditableList(id, owner);
            if (!list || values.isEmpty()) {
                return;
            }
            std::vector<std::unique_ptr<ss::Node>> items;
            items.reserve(size_t(values.size()));
            for (const auto &value : values) {
                items.push_back(convert(value));
            }
            list->insert(index, std::move(items));
        }

    protected:
        void actionAboutToApply(const ss::Action &action,
                                ss::Action::Operation operation) override {
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

        void actionApplied(const ss::Action &action, ss::Action::Operation operation) override {
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
                    Q_EMIT q->itemsMoved(change.parent()->id(), change.index(operation),
                                         change.count(), change.destination(operation));
                    break;
                }
                default:
                    // The session creates no other action. The root is set before the observer
                    // is notified of any action.
                    Q_ASSERT(false);
                    break;
            }
        }

        void stepChanged(int step) override {
            Q_UNUSED(step)
            Q_EMIT q->stepChanged();
        }
    };

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

    EditSession::EditSession(const Project &project, QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(this)) {
        _impl->model.reset(treeOf(project));
    }

    EditSession::~EditSession() = default;

    Project EditSession::snapshot() const {
        return projectOf(_impl->model.root());
    }

    NodeId EditSession::root() const {
        return _impl->model.root()->id();
    }

    bool EditSession::contains(NodeId node) const {
        return _impl->find<ss::Node>(node);
    }

    int EditSession::size(NodeId node) const {
        const auto found = _impl->find<ss::Node>(node);
        if (const auto list = dynamic_cast<const ss::VectorNode *>(found)) {
            return list->size();
        }
        if (const auto array = dynamic_cast<const PitchValuesNode *>(found)) {
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

    void EditSession::setPitchBend(NodeId note, const std::optional<PitchBend> &pitchBend) {
        const auto node = _impl->findEditable<ss::StructNodeBase>(note);
        if (!node) {
            return;
        }
        Q_ASSERT(node->type() == NoteType);
        node->setAt(NoteSlots::PitchBend.index,
                    pitchBend ? ss::Property(treeOfPitchBend(*pitchBend)) : ss::Property());
    }

    Note EditSession::note(NodeId note) const {
        const auto node = _impl->find<ss::Node>(note);
        return node && node->type() == NoteType ? noteOfTree(node) : Note();
    }

    NodeId EditSession::at(NodeId list, int index) const {
        const auto node = _impl->find<ss::VectorNode>(list);
        if (!node) {
            return 0;
        }
        Q_ASSERT(index >= 0 && index < node->size());
        return node->at(index)->id();
    }

    void EditSession::insert(NodeId list, int index, const QList<Note> &notes) {
        _impl->insert(list, TrackType, index, notes, treeOfNote);
    }

    void EditSession::insert(NodeId list, int index, const QList<PortamentoPoint> &points) {
        _impl->insert(list, NoteType, index, points, treeOfPortamentoPoint);
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
        const auto node = _impl->find<PitchValuesNode>(array);
        if (!node) {
            return {};
        }
        const auto values = node->values();
        return QList<double>(values.cbegin(), values.cend());
    }

    void EditSession::replaceValues(NodeId array, int index, const QList<double> &values) {
        if (const auto node = _impl->findEditable<PitchValuesNode>(array)) {
            node->replace(index, viewOf(values));
        }
    }

    void EditSession::insertValues(NodeId array, int index, const QList<double> &values) {
        if (const auto node = _impl->findEditable<PitchValuesNode>(array)) {
            node->insert(index, viewOf(values));
        }
    }

    void EditSession::removeValues(NodeId array, int index, int count) {
        if (const auto node = _impl->findEditable<PitchValuesNode>(array)) {
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
