#ifndef HELLOKIT_TESTS_EDIT_TESTSESSION_H
#define HELLOKIT_TESTS_EDIT_TESTSESSION_H

#include <memory>
#include <vector>

#include <QtCore/QStringList>

#include <substate/ArrayNode.h>
#include <substate/VectorNode.h>
#include <qsubstate/MappingNode.h>
#include <qsubstate/StructNode.h>

#include <hellokit/Edit/EditSession.h>
#include <hellokit/Edit/Slot.h>

#include "EditSession_p.h"

// A session of a document unrelated to UTAU, which shows that EditSession depends on no
// particular structure. The root has a title, a list of items and a mapping. Each item has a
// name and an array of values.

namespace hello::kit {

    namespace TestRootSlots {
        inline constexpr Slot<QString> Title{0, "title"};
        inline constexpr ChildSlot Items{1, "items"};
        inline constexpr ChildSlot Tags{2, "tags"};
    }

    namespace TestItemSlots {
        inline constexpr Slot<QString> Name{0, "name"};
        inline constexpr ChildSlot Values{1, "values"};
    }

    class TestSession : public EditSession {
    public:
        inline TestSession() {
            auto root = std::make_unique<ss::StructNode<3>>();
            root->setAt(TestRootSlots::Title.index, QVariant(QStringLiteral("title")));
            auto items = std::make_unique<ss::VectorNode>();
            items->append(item(QStringLiteral("first"), {1, 2, 3}));
            items->append(item(QStringLiteral("second"), {}));
            root->setAt(TestRootSlots::Items.index, std::move(items));
            auto tags = std::make_unique<ss::MappingNode>();
            tags->setProperty(QStringLiteral("a"), QVariant(1));
            root->setAt(TestRootSlots::Tags.index, std::move(tags));
            EditSessionPrivate::setRoot(*this, std::move(root));
        }

        inline NodeId items() const {
            return child(root(), TestRootSlots::Items);
        }

        inline NodeId tags() const {
            return child(root(), TestRootSlots::Tags);
        }

        /// Inserts items with \a names and no values before \a index.
        inline void insertItems(int index, const QStringList &names) {
            std::vector<std::unique_ptr<ss::Node>> nodes;
            for (const auto &name : names) {
                nodes.push_back(item(name, {}));
            }
            EditSessionPrivate::insert(*this, items(), index, std::move(nodes));
        }

        /// Returns the names of the items in order.
        inline QStringList names() const {
            QStringList result;
            for (int i = 0; i < size(items()); ++i) {
                result.push_back(value(at(items(), i), TestItemSlots::Name));
            }
            return result;
        }

    private:
        static inline std::unique_ptr<ss::Node> item(const QString &name,
                                                     const std::vector<double> &values) {
            auto node = std::make_unique<ss::StructNode<2>>();
            node->setAt(TestItemSlots::Name.index, QVariant(name));
            auto array = std::make_unique<ss::ArrayNode<double>>();
            array->append(values);
            node->setAt(TestItemSlots::Values.index, std::move(array));
            return node;
        }
    };

}

#endif // HELLOKIT_TESTS_EDIT_TESTSESSION_H
