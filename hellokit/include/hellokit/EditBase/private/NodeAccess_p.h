#ifndef HELLOKIT_EDITBASE_PRIVATE_NODEACCESS_P_H
#define HELLOKIT_EDITBASE_PRIVATE_NODEACCESS_P_H

#include <memory>
#include <optional>
#include <vector>

#include <QtCore/QList>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

#include <substate/ArrayNode.h>
#include <substate/VectorNode.h>
#include <qsubstate/MappingNode.h>
#include <qsubstate/StructNode.h>

#include <hellokit/EditBase/Slot.h>

#include "EditSession_p.h"

namespace hello::kit::edit {

    /// The node of the handles of type \a Ref: \c Type is the class of the node and \c type its
    /// node type. A list handle also specifies \c itemType, the node type of its items. A
    /// document specializes this template for each of its handles.
    template <class Ref>
    struct NodeOf;

    template <class NodeClass, int nodeType>
    struct NodeTraits {
        using Type = NodeClass;
        static constexpr int type = nodeType;
    };

    template <int itemNodeType>
    struct ListTraits : NodeTraits<ss::VectorNode, ss::Node::Vector> {
        static constexpr int itemType = itemNodeType;
    };

    using MappingTraits = NodeTraits<ss::MappingNode, ss::Node::Mapping>;

    /// Returns the value of type \a T stored in the tree \a node. A document declares an
    /// explicit specialization for each type of its records, and an overload of
    /// <tt>std::unique_ptr<ss::Node> treeOf(const T &)</tt> for the reverse conversion.
    template <class T>
    T fromTree(const ss::Node *node);

    /// The operations of the handles of a document on the nodes of each kind. Each handle of a
    /// document implements each of its functions as one call of a function of this class, which
    /// determines the node from the handle by NodeOf.
    struct NodeAccess {
        template <class Ref>
        static inline typename NodeOf<Ref>::Type *find(const Ref &ref) {
            return EditSessionPrivate::find<typename NodeOf<Ref>::Type>(ref.session(), ref.id(),
                                                                        NodeOf<Ref>::type);
        }

        template <class Ref>
        static inline typename NodeOf<Ref>::Type *edit(const Ref &ref) {
            return EditSessionPrivate::findEditable<typename NodeOf<Ref>::Type>(
                ref.session(), ref.id(), NodeOf<Ref>::type);
        }

        /// Returns a copy of the value of the node of \a ref, or a default value if the handle is
        /// invalid.
        template <class T, class Ref>
        static inline T toValue(const Ref &ref) {
            const auto node = find(ref);
            return node ? fromTree<T>(node) : T();
        }

        // Records

        template <class Ref, class T>
        static inline T value(const Ref &record, Slot<T> slot) {
            const auto node = find(record);
            return SlotValue<T>::fromVariant(node ? node->variant(slot.index) : QVariant());
        }

        template <class Ref, class T>
        static inline void setValue(const Ref &record, Slot<T> slot,
                                    const typename Slot<T>::ValueType &value) {
            if (const auto node = edit(record)) {
                node->setAt(slot.index, SlotValue<T>::toVariant(value));
            }
        }

        template <class ChildRef, class Ref>
        static inline ChildRef child(const Ref &record, ChildSlot slot) {
            return ChildRef(record.session(), childId(find(record), slot));
        }

        /// Replaces the child in \a slot with the tree of \a value, or removes it if \a value is
        /// empty. A value equal to the value of the current child creates no change, as for a
        /// value field, although a new tree is never equal to the current child.
        template <class Ref, class T>
        static inline void setChild(const Ref &record, ChildSlot slot,
                                    const std::optional<T> &value) {
            const auto node = edit(record);
            if (!node) {
                return;
            }
            const auto current = node->child(slot.index);
            if (value ? current && fromTree<T>(current) == *value : !current) {
                return;
            }
            node->setAt(slot.index, value ? ss::Property(treeOf(*value)) : ss::Property());
        }

        // Lists

        template <class Ref>
        static inline int size(const Ref &list) {
            const auto node = find(list);
            return node ? node->size() : 0;
        }

        template <class ItemRef, class Ref>
        static inline ItemRef at(const Ref &list, int index) {
            const auto node = find(list);
            Q_ASSERT(!node || (index >= 0 && index < node->size()));
            return ItemRef(list.session(), node ? node->at(index)->id() : 0);
        }

        /// Inserts the trees of \a values before \a index.
        template <class Ref, class T>
        static inline void insert(const Ref &list, int index, const QList<T> &values) {
            const auto node = edit(list);
            if (!node || values.isEmpty()) {
                return;
            }
            std::vector<std::unique_ptr<ss::Node>> items;
            items.reserve(size_t(values.size()));
            for (const auto &value : values) {
                items.push_back(treeOf(value));
                Q_ASSERT(items.back()->type() == NodeOf<Ref>::itemType);
            }
            node->insert(index, std::move(items));
        }

        template <class Ref>
        static inline void remove(const Ref &list, int index, int count) {
            if (const auto node = edit(list)) {
                node->remove(index, count);
            }
        }

        template <class Ref>
        static inline void move(const Ref &list, int index, int count, int destination) {
            if (const auto node = edit(list)) {
                node->move(index, count, destination);
            }
        }

        // Mappings

        template <class Ref>
        static inline QStringList keys(const Ref &mapping) {
            const auto node = find(mapping);
            return node ? node->keys() : QStringList();
        }

        template <class Ref>
        static inline bool contains(const Ref &mapping, const QString &key) {
            const auto node = find(mapping);
            return node && node->contains(key);
        }

        template <class T, class Ref>
        static inline T entry(const Ref &mapping, const QString &key) {
            const auto node = find(mapping);
            return SlotValue<T>::fromVariant(node ? node->variant(key) : QVariant());
        }

        template <class Ref, class T>
        static inline void setEntry(const Ref &mapping, const QString &key, const T &value) {
            if (const auto node = edit(mapping)) {
                node->setProperty(key, SlotValue<T>::toVariant(value));
            }
        }

        template <class Ref>
        static inline void removeEntry(const Ref &mapping, const QString &key) {
            if (const auto node = edit(mapping)) {
                node->setProperty(key, ss::Property());
            }
        }

        // Arrays in the slot of a record. \a arrayType is the node type of the array.

        template <class T, class Ref>
        static inline QList<T> arrayValues(const Ref &record, ChildSlot slot, int arrayType) {
            const auto node = findArray<T>(record, slot, arrayType, false);
            if (!node) {
                return {};
            }
            const auto values = node->values();
            return QList<T>(values.cbegin(), values.cend());
        }

        template <class T, class Ref>
        static inline int arraySize(const Ref &record, ChildSlot slot, int arrayType) {
            const auto node = findArray<T>(record, slot, arrayType, false);
            return node ? node->size() : 0;
        }

        /// Overwrites the elements starting at \a index, extending the array if \a values
        /// reaches beyond its end.
        template <class T, class Ref>
        static inline void replaceArray(const Ref &record, ChildSlot slot, int arrayType, int index,
                                        const QList<T> &values) {
            if (const auto node = findArray<T>(record, slot, arrayType, true)) {
                node->replace(index, viewOf(values));
            }
        }

        template <class T, class Ref>
        static inline void insertArray(const Ref &record, ChildSlot slot, int arrayType, int index,
                                       const QList<T> &values) {
            if (const auto node = findArray<T>(record, slot, arrayType, true)) {
                node->insert(index, viewOf(values));
            }
        }

        template <class T, class Ref>
        static inline void removeArray(const Ref &record, ChildSlot slot, int arrayType, int index,
                                       int count) {
            if (const auto node = findArray<T>(record, slot, arrayType, true)) {
                node->remove(index, count);
            }
        }

    private:
        static inline NodeId childId(const ss::StructNodeBase *record, ChildSlot slot) {
            const auto child = record ? record->child(slot.index) : nullptr;
            return child ? child->id() : 0;
        }

        template <class T, class Ref>
        static inline ss::ArrayNode<T> *findArray(const Ref &record, ChildSlot slot, int arrayType,
                                                  bool editable) {
            const auto id = childId(find(record), slot);
            return editable ? EditSessionPrivate::findEditable<ss::ArrayNode<T>>(record.session(),
                                                                                 id, arrayType)
                            : EditSessionPrivate::find<ss::ArrayNode<T>>(record.session(), id,
                                                                         arrayType);
        }

        template <class T>
        static inline ss::ArrayView<T> viewOf(const QList<T> &values) {
            return ss::ArrayView<T>(values.constData(), size_t(values.size()));
        }
    };

}

#endif // HELLOKIT_EDITBASE_PRIVATE_NODEACCESS_P_H
