#ifndef HELLOKIT_EDIT_VALIDATION_P_H
#define HELLOKIT_EDIT_VALIDATION_P_H

#include <cmath>
#include <optional>

#include <QtCore/QList>
#include <QtCore/QString>

#include <qsubstate/StructNode.h>

#include <hellokit/Edit/Slot.h>

#include "EditSession_p.h"

namespace hello::kit {

    /// The checks that the validators of a document compose.
    struct Validation {
        /// Appends a violation to \a violations if the value in \a slot of \a record is outside
        /// the range of the slot. An empty optional value and a slot without a range pass.
        template <class T>
        static inline void checkRange(const ss::StructNodeBase &record, Slot<T> slot,
                                      QList<Violation> &violations) {
            if (!slot.range) {
                return;
            }
            const auto value = numberOf(SlotValue<T>::fromVariant(record.variant(slot.index)));
            if (value && !slot.range->contains(*value)) {
                violations.push_back({slot.index, EditSession::tr("The %1 %2 is %3.")
                                                      .arg(QString::fromLatin1(slot.name))
                                                      .arg(*value)
                                                      .arg(describe(*slot.range))});
            }
        }

    private:
        template <class T>
        static inline std::optional<double> numberOf(const T &value) {
            return double(value);
        }

        template <class T>
        static inline std::optional<double> numberOf(const std::optional<T> &value) {
            return value ? std::optional<double>(double(*value)) : std::nullopt;
        }

        static inline QString describe(const Range &range) {
            if (std::isinf(range.maximum)) {
                return range.minimumExclusive
                           ? EditSession::tr("not greater than %1").arg(range.minimum)
                           : EditSession::tr("less than %1").arg(range.minimum);
            }
            return EditSession::tr("outside the range from %1 to %2")
                .arg(range.minimum)
                .arg(range.maximum);
        }
    };

}

#endif // HELLOKIT_EDIT_VALIDATION_P_H
