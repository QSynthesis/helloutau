#ifndef HELLOKIT_EDIT_SLOT_H
#define HELLOKIT_EDIT_SLOT_H

#include <cstdint>
#include <optional>

#include <QtCore/QVariant>

namespace hello::kit {

    /// The identifier of a node of an edit session.
    ///
    /// It remains unchanged while the node exists, including across insertion, removal, undo and
    /// redo, and is never reused within the session. It is not saved to files, because it refers
    /// to a node of the session rather than to the content of the document.
    using NodeId = std::uint64_t;

    /// A slot of a record that holds a value of type \a T, addressed by \c index.
    ///
    /// \c name is the name of the corresponding field of \c .usth, which commands and logs use.
    template <class T>
    struct Slot {
        using ValueType = T;

        int index;
        const char *name;
    };

    /// A slot of a record that holds a child node, addressed by \c index.
    struct ChildSlot {
        int index;
        const char *name;
    };

    /// The conversion of a slot value to and from the QVariant stored in a node. An empty
    /// \c std::optional is stored as an invalid QVariant, which leaves the slot empty.
    template <class T>
    struct SlotValue {
        static inline QVariant toVariant(const T &value) {
            return QVariant::fromValue(value);
        }

        static inline T fromVariant(const QVariant &variant) {
            return variant.value<T>();
        }
    };

    template <class T>
    struct SlotValue<std::optional<T>> {
        static inline QVariant toVariant(const std::optional<T> &value) {
            return value ? SlotValue<T>::toVariant(*value) : QVariant();
        }

        static inline std::optional<T> fromVariant(const QVariant &variant) {
            if (!variant.isValid()) {
                return std::nullopt;
            }
            return SlotValue<T>::fromVariant(variant);
        }
    };

}

#endif // HELLOKIT_EDIT_SLOT_H
