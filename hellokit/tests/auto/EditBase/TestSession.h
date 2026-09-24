#ifndef HELLOKIT_TESTS_EDIT_TESTSESSION_H
#define HELLOKIT_TESTS_EDIT_TESTSESSION_H

#include <memory>
#include <vector>

#include <QtCore/QList>
#include <QtCore/QStringList>

#include <substate/ArrayNode.h>
#include <substate/VectorNode.h>
#include <qsubstate/MappingNode.h>
#include <qsubstate/StructNode.h>

#include <hellokit/EditBase/EditSession.h>

#include <hellokit/EditBase/private/EditSession_p.h>

// A session of a document unrelated to UTAU, which shows that EditSession depends on no
// particular structure. It is its own document layer: its functions read and modify the nodes
// through the extension interface, as the handles of a project do.
//
// The root holds a title in slot 0, a list of items in slot 1 and a mapping of tags in slot 2.
// Each item holds a name in slot 0 and an array of values in slot 1.

namespace hello::kit::edit {

    class TestSession : public EditSession {
    public:
        using Root = ss::StructNode<3>;
        using Item = ss::StructNode<2>;
        using Values = ss::ArrayNode<double>;

        enum NodeType {
            RootType = ss::Node::User,
            ItemType,
            ValuesType,
        };

        /// The limit of the constraint on the number of items.
        static constexpr int maximumItems = 4;

        /// Creates the tree with an item for each of \a names. The first item has the values 1,
        /// 2 and 3, the others none. The tree is not validated, as a document read from a file.
        ///
        /// The validators require at most maximumItems items and a name that is not empty.
        inline explicit TestSession(const QStringList &names = {QStringLiteral("first"),
                                                                QStringLiteral("second")}) {
            auto root = std::make_unique<Root>(RootType);
            root->setAt(0, QVariant(QStringLiteral("title")));
            auto items = std::make_unique<ss::VectorNode>();
            for (qsizetype i = 0; i < names.size(); ++i) {
                items->append(
                    item(names[i], i == 0 ? std::vector<double>{1, 2, 3} : std::vector<double>()));
            }
            root->setAt(1, std::move(items));
            auto tags = std::make_unique<ss::MappingNode>();
            tags->setProperty(QStringLiteral("a"), QVariant(1));
            root->setAt(2, std::move(tags));
            EditSessionPrivate::setRoot(*this, std::move(root));

            EditSessionPrivate::registerValidator(
                *this, RootType, [](const ss::Node *node, QList<Violation> &violations) {
                    const auto &root = static_cast<const Root &>(*node);
                    const auto count = static_cast<const ss::VectorNode *>(root.child(1))->size();
                    if (count > maximumItems) {
                        violations.push_back({1, QStringLiteral("%1 items").arg(count)});
                    }
                });
            EditSessionPrivate::registerValidator(
                *this, ItemType, [](const ss::Node *node, QList<Violation> &violations) {
                    if (static_cast<const Item &>(*node).variant(0).toString().isEmpty()) {
                        violations.push_back({0, QStringLiteral("empty name")});
                    }
                });
        }

        inline void setName(NodeId item, const QString &name) {
            edit<Item>(item, ItemType)->setAt(0, QVariant(name));
        }

        inline QString title() const {
            return find<Root>(root(), RootType)->variant(0).toString();
        }

        inline void setTitle(const QString &title) {
            edit<Root>(root(), RootType)->setAt(0, QVariant(title));
        }

        inline NodeId items() const {
            return find<Root>(root(), RootType)->child(1)->id();
        }

        inline NodeId itemAt(int index) const {
            return find<ss::VectorNode>(items(), ss::Node::Vector)->at(index)->id();
        }

        inline QStringList names() const {
            QStringList result;
            const auto list = find<ss::VectorNode>(items(), ss::Node::Vector);
            for (int i = 0; i < list->size(); ++i) {
                result.push_back(static_cast<const Item *>(list->at(i))->variant(0).toString());
            }
            return result;
        }

        inline QString nameOf(NodeId item) const {
            const auto node = find<Item>(item, ItemType);
            return node ? node->variant(0).toString() : QString();
        }

        inline void insertItems(int index, const QStringList &names) {
            std::vector<std::unique_ptr<ss::Node>> nodes;
            for (const auto &name : names) {
                nodes.push_back(item(name, {}));
            }
            edit<ss::VectorNode>(items(), ss::Node::Vector)->insert(index, std::move(nodes));
        }

        inline void removeItems(int index, int count) {
            edit<ss::VectorNode>(items(), ss::Node::Vector)->remove(index, count);
        }

        inline void moveItems(int index, int count, int destination) {
            edit<ss::VectorNode>(items(), ss::Node::Vector)->move(index, count, destination);
        }

        inline NodeId tags() const {
            return find<Root>(root(), RootType)->child(2)->id();
        }

        inline QStringList tagKeys() const {
            return find<ss::MappingNode>(tags(), ss::Node::Mapping)->keys();
        }

        /// Stores \a value under \a key, or removes the entry if \a value is invalid.
        inline void setTag(const QString &key, const QVariant &value) {
            edit<ss::MappingNode>(tags(), ss::Node::Mapping)->setProperty(key, value);
        }

        /// Returns the array of the item \a item, or 0 if the item has none.
        inline NodeId valuesOf(NodeId item) const {
            const auto child = find<Item>(item, ItemType)->child(1);
            return child ? child->id() : 0;
        }

        inline QList<double> values(NodeId array) const {
            const auto values = find<Values>(array, ValuesType)->values();
            return QList<double>(values.cbegin(), values.cend());
        }

        inline Values *editValues(NodeId array) {
            return edit<Values>(array, ValuesType);
        }

        inline void removeValuesOf(NodeId item) {
            edit<Item>(item, ItemType)->setAt(1, ss::Property());
        }

    private:
        template <class Class>
        inline Class *find(NodeId id, int type) const {
            return EditSessionPrivate::find<Class>(this, id, type);
        }

        template <class Class>
        inline Class *edit(NodeId id, int type) {
            return EditSessionPrivate::findEditable<Class>(this, id, type);
        }

        static inline std::unique_ptr<ss::Node> item(const QString &name,
                                                     const std::vector<double> &values) {
            auto node = std::make_unique<Item>(ItemType);
            node->setAt(0, QVariant(name));
            auto array = std::make_unique<Values>(ValuesType);
            array->append(values);
            node->setAt(1, std::move(array));
            return node;
        }
    };

}

#endif // HELLOKIT_TESTS_EDIT_TESTSESSION_H
