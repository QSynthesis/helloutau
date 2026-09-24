#ifndef HELLOKIT_EDITBASE_SLOT_H
#define HELLOKIT_EDITBASE_SLOT_H

#include <cstdint>
#include <optional>
#include <type_traits>

#include <QtCore/QVariant>

namespace hello::kit::edit {

    /// The identifier of a node of an edit session.
    ///
    /// It remains unchanged while the node exists, including across insertion, removal, undo and
    /// redo, and is never reused within the session. It is not saved to files, because it refers
    /// to a node of the session rather than to the content of the document.
    using NodeId = std::uint64_t;

    /// The permitted values of type \a T, a number. The bounds are inclusive, except the minimum
    /// if \c minimumExclusive is true. A range without \c maximum has no upper bound.
    template <class T>
    struct Range {
        static_assert(std::is_arithmetic_v<T> && !std::is_same_v<T, bool>,
                      "a range bounds a number");

        T minimum;
        std::optional<T> maximum;
        bool minimumExclusive = false;

        static inline constexpr Range between(T minimum, T maximum) {
            return {minimum, maximum, false};
        }

        static inline constexpr Range atLeast(T minimum) {
            return {minimum, std::nullopt, false};
        }

        static inline constexpr Range greaterThan(T minimum) {
            return {minimum, std::nullopt, true};
        }

        inline constexpr bool contains(T value) const {
            return (minimumExclusive ? value > minimum : value >= minimum) &&
                   (!maximum || value <= *maximum);
        }

        /// Returns this range with bounds of type \a U.
        template <class U>
        inline constexpr Range<U> to() const {
            return {U(minimum), maximum ? std::optional<U>(U(*maximum)) : std::nullopt,
                    minimumExclusive};
        }
    };

    /// The type of the range of a slot of a type that no range bounds. No Range converts to it,
    /// therefore a range of such a slot does not compile.
    struct NoRange {};

    /// The type of the values of a slot of type \a T: \a T without \c std::optional.
    template <class T>
    struct SlotNumber {
        using Type = T;
    };

    template <class T>
    struct SlotNumber<std::optional<T>> {
        using Type = T;
    };

    /// The type of the range of a slot of type \a T: the Range of its values if they are numbers,
    /// and NoRange otherwise.
    template <class T, class Number = typename SlotNumber<T>::Type>
    using RangeOf =
        std::conditional_t<std::is_arithmetic_v<Number> && !std::is_same_v<Number, bool>,
                           Range<Number>, NoRange>;

    /// A slot of a record that holds a value of type \a T, addressed by \c index.
    ///
    /// \c name is the name of the corresponding field of \c .usth, which commands and logs use.
    /// \c range constrains a numeric value and has the type of the value. A violation does not
    /// prevent the modification, but the commit of a transaction that introduces it.
    ///
    /// \sa EditSession::Transaction::commit()
    template <class T>
    struct Slot {
        using ValueType = T;

        int index;
        const char *name;
        std::optional<RangeOf<T>> range = std::nullopt;
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

#endif // HELLOKIT_EDITBASE_SLOT_H
